// main.cpp - Hub Central du Robot IA Vivant
#include <Arduino.h>
#include "display_tft.h"
#include "audio_i2s.h"
#include "network_ws.h"
#include "audio_chunk.h"
#include "audio_mp3.h"
#include "gui_spotify.h"

#include <WiFiMulti.h>

WiFiMulti wifiMulti;

// --- Architecture Dual-State (Machine à États) ---
enum SystemState {
  OFFLINE_MP3, 
  ONLINE_AI    
};
SystemState currentState = ONLINE_AI; 

// --- Configuration Wi-Fi et Serveur ---
const char* ssid1 = "LB_ADSL_EGPK";
const char* pass1 = "TUKhDTzHUmQXMdaUHZ";
const char* ssid2 = "Linux_ya_jdk";
const char* pass2 = "https2008";

const char* ws_server_ip_box = "192.168.11.113";
const char* ws_server_ip_hotspot = "10.42.0.1";
const uint16_t ws_server_port = 8765;

// --- Instances des Modules (Pointeurs pour la RAM) ---
TFT_Display* display = nullptr;
Audio_I2S* audio = nullptr;
Network_WS* network = nullptr;
AudioMP3* mp3Player = nullptr;
GuiSpotify* spotifyUi = nullptr;

QueueHandle_t emotionQueue;
QueueHandle_t commandQueue;
QueueHandle_t audioTxQueue;
QueueHandle_t audioRxQueue;

bool isMp3ModeInitialized = false;
String currentEmotion = "neutre";
bool isSpeaking = false;

// --- Callbacks Audio ---
void audio_info(const char *info){ Serial.print("Audio Info: "); Serial.println(info); }

void load_lvgl_mp3_app(bool from_online_ai = false) {
  if (isMp3ModeInitialized) return;
  if (from_online_ai) audio->uninstallSpeaker();
  spotifyUi->init(display->getTftPointer());
  spotifyUi->buildInterface();
  mp3Player->init();
  if (SD.exists("/test.mp3")) mp3Player->play("/test.mp3");
  else if (SD.exists("/music/test.mp3")) mp3Player->play("/music/test.mp3");
  spotifyUi->updateTitle("L'Morphine", "Skit");
  isMp3ModeInitialized = true;
}

void destroy_lvgl_mp3_app() {
  if (!isMp3ModeInitialized) return;
  mp3Player->deinit();
  spotifyUi->deinit();
  display->getTftPointer()->fillScreen(TFT_BLACK);
  isMp3ModeInitialized = false;
}

void orchestratorTask(void *pvParameters) {
  char cmdBuf[32];
  for(;;) {
    if (xQueueReceive(commandQueue, &cmdBuf, portMAX_DELAY) == pdPASS) {
      String cmd = String(cmdBuf);
      if (cmd == "start_mp3") currentState = OFFLINE_MP3;
      else if (cmd == "stop_mp3") currentState = ONLINE_AI;
    }
  }
}

void displayTask(void *pvParameters) {
  display->init();
  lv_init();
  SystemState lastState = ONLINE_AI;
  
  if (currentState == ONLINE_AI) display->displayEmotion(currentEmotion, 1);
  else load_lvgl_mp3_app(false);

  for (;;) {
    if (currentState != lastState) {
      if (currentState == OFFLINE_MP3) load_lvgl_mp3_app(true);
      else {
        destroy_lvgl_mp3_app();
        audio->initSpeaker();
        display->displayEmotion("neutre", 1);
      }
      lastState = currentState;
    }

    if (currentState == OFFLINE_MP3) {
      if (isMp3ModeInitialized) {
        spotifyUi->updateProgress(mp3Player->getAudioCurrentTime(), mp3Player->getAudioFileDuration());
        spotifyUi->togglePlayPauseIcon(mp3Player->isPlaying());
        lv_task_handler();
        mp3Player->loop();
        if (spotifyUi->isPlayBtnClicked) { spotifyUi->isPlayBtnClicked = false; mp3Player->pause(); }
      }
      lv_tick_inc(5);
      vTaskDelay(5 / portTICK_PERIOD_MS);
      continue;
    }

    char receivedEmotion[32];
    if (xQueueReceive(emotionQueue, &receivedEmotion, 50 / portTICK_PERIOD_MS) == pdPASS) {
      String em = String(receivedEmotion);
      if (em == "parle") isSpeaking = true;
      else if (em == "idle") isSpeaking = false;
      else currentEmotion = em;
      display->displayEmotion(isSpeaking ? "parle" : currentEmotion, 1);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void networkTask(void *pvParameters) {
  wifiMulti.addAP(ssid1, pass1);
  wifiMulti.addAP(ssid2, pass2);

  Serial.println("Recherche Wi-Fi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  String currentIP = WiFi.localIP().toString();
  const char* targetIP = currentIP.startsWith("10.42.") ? ws_server_ip_hotspot : ws_server_ip_box;
  
  Serial.print("\nWi-Fi OK: "); Serial.println(WiFi.SSID());
  Serial.print("IP Robot: "); Serial.println(currentIP);
  Serial.print("Serveur cible: "); Serial.println(targetIP);

  network->initWebSocket(targetIP, ws_server_port);
  
  AudioChunk rxChunk;
  for (;;) {
    network->loop();
    if (xQueueReceive(audioRxQueue, &rxChunk, 0) == pdPASS) {
      if (rxChunk.data) { network->sendAudio(rxChunk.data, rxChunk.length); free(rxChunk.data); }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void micTask(void *pvParameters) {
  audio->initMic();
  for (;;) {
    // Si on est en mode Lecteur MP3 OU si on n'est pas encore connecté au serveur IA, on attend.
    // Cela évite de saturer le buffer réseau pendant le "Handshake" WebSocket.
    if (currentState == OFFLINE_MP3 || !network->isConnected()) { 
      vTaskDelay(100 / portTICK_PERIOD_MS); 
      continue; 
    }
    
    int16_t* buf = (int16_t*)malloc(1024);
    if (buf) {
      size_t r = audio->readMic(buf, 1024);
      if (r > 0) {
        AudioChunk c = {(uint8_t*)buf, r};
        if (xQueueSend(audioRxQueue, &c, 0) != pdPASS) free(buf);
      } else free(buf);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void speakerTask(void *pvParameters) {
  // On n'initialise pas ici, on attend que networkTask nous donne le feu vert
  AudioChunk txChunk;
  for (;;) {
    if (currentState == OFFLINE_MP3) {
      if (xQueueReceive(audioTxQueue, &txChunk, 100) == pdPASS && txChunk.data) free(txChunk.data);
      continue;
    }
    
    if (xQueueReceive(audioTxQueue, &txChunk, 100) == pdPASS && txChunk.data) {
      // Initialisation à la volée du haut-parleur si nécessaire
      static bool spk_ready = false;
      if (!spk_ready) {
          audio->initSpeaker();
          spk_ready = true;
      }
      
      if (currentState == ONLINE_AI) audio->writeSpeaker(txChunk.data, txChunk.length);
      free(txChunk.data);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  // ÉTEINDRE LE SOUFFLE IMMÉDIATEMENT
  pinMode(22, OUTPUT); 
  digitalWrite(22, LOW); // Force la ligne DIN à la masse

  // SÉCURITÉ DU BUS SPI
  pinMode(14, OUTPUT); digitalWrite(14, HIGH); // TFT_CS
  pinMode(13, OUTPUT); digitalWrite(13, HIGH); // TOUCH_CS
  pinMode(5, OUTPUT);  digitalWrite(5, HIGH);  // SD_CS

  // Correction I2S critique
  i2s_driver_uninstall(I2S_NUM_1);
  i2s_driver_uninstall(I2S_NUM_0);
  
  delay(500);
  SPI.begin(18, 19, 23, 5);
  
  int retry = 0;
  while (!SD.begin(5, SPI, 4000000) && retry < 3) { // 4 MHz maximum pour la stabilité
      Serial.println("SD_RETRY...");
      delay(500);
      retry++;
  }
  
  if (retry >= 3) Serial.println("SD_FAIL_DEFINITIVE");
  else Serial.println("SD_OK_SETUP");
  
  display = new TFT_Display();
  audio = new Audio_I2S();
  network = new Network_WS();
  mp3Player = new AudioMP3();
  spotifyUi = new GuiSpotify();

  emotionQueue = xQueueCreate(5, 32);
  commandQueue = xQueueCreate(5, 32);
  audioTxQueue = xQueueCreate(10, sizeof(AudioChunk));
  audioRxQueue = xQueueCreate(10, sizeof(AudioChunk));

  xTaskCreatePinnedToCore(networkTask, "Net", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(orchestratorTask, "Orc", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(displayTask, "Disp", 6144, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(micTask, "Mic", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(speakerTask, "Spk", 4096, NULL, 3, NULL, 1);
}

void loop() { vTaskDelete(NULL); }
