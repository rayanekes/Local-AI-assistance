// main.cpp - Hub Central du Robot IA Hybride (v3.4_stable)
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <atomic>
#include <WiFi.h>
#include "secrets.h"
#include "display_tft.h"
#include "audio_i2s.h"
#include "network_ws.h"
#include "audio_chunk.h"
#include "audio_mp3.h"
#include "gui_spotify.h"
#include "face_renderer.h"

#define MAX_SD_MODE_PIN 15
#define MAX_DIN_PIN     12
#define TFT_RST_PIN     4
#define TFT_CS_PIN      14
#define TOUCH_CS_PIN    13
#define SD_CS_PIN       5

// --- Instances ---
TFT_Display* display   = nullptr;
Audio_I2S*   audio     = nullptr;
Network_WS*  network   = nullptr;
AudioMP3*    mp3Player = nullptr;
GuiSpotify*  spotifyUi = nullptr;
FaceRenderer* face     = nullptr;

QueueHandle_t emotionQueue;
QueueHandle_t commandQueue;
QueueHandle_t audioTxQueue;
QueueHandle_t audioRxQueue;

enum SystemState { OFFLINE_MP3, ONLINE_AI };
volatile SystemState currentState = OFFLINE_MP3;
bool isMp3ModeInitialized = false;
String currentEmotion = "neutre";
bool isSpeaking = false;
std::atomic<bool> spk_ready{false};

const uint16_t ws_server_port = 8765;

// --- Recherche de musique ---
void search_and_play(String query) {
  query.toLowerCase();
  File root = SD.open("/music");
  if (!root) return;
  
  String targetFile = "";
  while (File file = root.openNextFile()) {
      String name = String(file.name());
      name.toLowerCase();
      if (name.indexOf(query) != -1) {
          targetFile = "/music/" + String(file.name());
          break;
      }
  }
  
  if (targetFile != "") {
      Serial.println("[PLAYER] Trouvé : " + targetFile);
      if (currentState != OFFLINE_MP3) currentState = OFFLINE_MP3;
      
      String cleanTitle = targetFile.substring(7); // Supprime "/music/"
      if (cleanTitle.endsWith(".mp3")) cleanTitle = cleanTitle.substring(0, cleanTitle.length() - 4);
      
      mp3Player->play(targetFile.c_str());
      spotifyUi->updateTitle(cleanTitle.c_str(), "Robot AI Player");
  }
}

// ==========================================
// TÂCHES FREERTOS
// ==========================================

void debugTask(void *pvParameters) {
    for(;;) { vTaskDelay(10000 / portTICK_PERIOD_MS); }
}

void orchestratorTask(void *pvParameters) {
    char cmdBuf[128];
    for(;;) {
        if (xQueueReceive(commandQueue, &cmdBuf, portMAX_DELAY) == pdPASS) {
            StaticJsonDocument<256> doc;
            deserializeJson(doc, cmdBuf);
            String cmd = doc["command"] | "none";
            
            if (cmd == "play_song") {
                String title = doc["title"] | "test";
                search_and_play(title);
            } else if (cmd == "start_mp3") {
                currentState = OFFLINE_MP3;
            } else if (cmd == "stop_mp3") {
                currentState = ONLINE_AI;
            }
        }
    }
}

void networkTask(void *pvParameters) {
  esp_task_wdt_add(NULL);
  IPAddress ip_h(10, 42, 0, 100);
  IPAddress gate_h(10, 42, 0, 1);
  IPAddress ip_b(192, 168, 11, 100);
  IPAddress gate_b(192, 168, 11, 1);
  IPAddress sub(255, 255, 255, 0);
  
  WiFi.begin(WIFI_SSID_BOX, WIFI_PASS_BOX); 
  WiFi.config(ip_b, gate_b, sub);
  String currentIP = WiFi.localIP().toString();
  const char* targetIP = currentIP.startsWith("10.42.") ? "10.42.0.1" : "192.168.11.113";
  network->initWebSocket(targetIP, ws_server_port);

  AudioChunk rxChunk;
  for (;;) {
    esp_task_wdt_reset();
    network->loop();
    if (xQueueReceive(audioRxQueue, &rxChunk, 0) == pdPASS) {
      if (rxChunk.data) { network->sendAudio(rxChunk.data, rxChunk.length); free(rxChunk.data); }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void micTask(void *pvParameters) {
    audio->initMic();
    static int16_t staticMicBuffer[2048];
    for (;;) {
        if (currentState == OFFLINE_MP3 || !network->isConnected()) { vTaskDelay(100 / portTICK_PERIOD_MS); continue; }
        size_t r = audio->readMic(staticMicBuffer, 2048);
        if (r > 0 && uxQueueSpacesAvailable(audioRxQueue) > 2) {
            int16_t* heapBuf = (int16_t*)malloc(r);
            if (heapBuf) { memcpy(heapBuf, staticMicBuffer, r); AudioChunk c = {(uint8_t*)heapBuf, r}; if (xQueueSend(audioRxQueue, &c, 0) != pdPASS) free(heapBuf); }
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

void speakerTask(void *pvParameters) {
    AudioChunk txChunk;
    for (;;) {
        if (currentState == OFFLINE_MP3) { if (xQueueReceive(audioTxQueue, &txChunk, 100) == pdPASS && txChunk.data) free(txChunk.data); continue; }
        if (xQueueReceive(audioTxQueue, &txChunk, 100) == pdPASS && txChunk.data) {
            if (!spk_ready.load()) { digitalWrite(MAX_SD_MODE_PIN, HIGH); audio->initSpeaker(); spk_ready.store(true); }
            if (face && face->isInitialized()) face->setAudioRMS(computeRMS(txChunk.data, txChunk.length));
            audio->writeSpeaker(txChunk.data, txChunk.length);
            free(txChunk.data);
        }
    }
}

void displayTask(void *pvParameters) {
    display->init();
    lv_init(); // UNIQUE APPEL pour LVGL
    
    spotifyUi->init(display->getTftPointer());
    spotifyUi->buildInterface();
    isMp3ModeInitialized = true;
    
    SystemState lastState = OFFLINE_MP3;
    uint32_t lastTick = millis();

    for (;;) {
        uint32_t delta = millis() - lastTick;
        lastTick = millis();

        if (network->isConnected() && currentState == OFFLINE_MP3 && lastState == OFFLINE_MP3) {
            currentState = ONLINE_AI;
        }

        if (currentState != lastState) {
            if (currentState == OFFLINE_MP3) {
                if (spk_ready.load()) { audio->uninstallSpeaker(); spk_ready.store(false); }
                if (face) face->deinit();
                digitalWrite(MAX_SD_MODE_PIN, LOW);
                spotifyUi->init(display->getTftPointer());
                spotifyUi->buildInterface();
                isMp3ModeInitialized = true;
            } else {
                if (isMp3ModeInitialized) { mp3Player->deinit(); spotifyUi->deinit(); isMp3ModeInitialized = false; }
                display->getTftPointer()->fillScreen(TFT_BLACK);
                digitalWrite(MAX_SD_MODE_PIN, HIGH);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                
                if (!face) face = new FaceRenderer();
                face->init(display->getTftPointer());
                audio->initSpeaker(); spk_ready.store(true);
            }
            lastState = currentState;
        }

        if (currentState == OFFLINE_MP3) {
            if (spotifyUi->isQuitBtnClicked) { spotifyUi->isQuitBtnClicked = false; currentState = ONLINE_AI; }
            spotifyUi->updateProgress(mp3Player->getAudioCurrentTime(), mp3Player->getAudioFileDuration());
            lv_task_handler(); mp3Player->loop(); lv_tick_inc(5); vTaskDelay(5 / portTICK_PERIOD_MS);
        } else {
            char receivedEmotion[32];
            if (xQueueReceive(emotionQueue, &receivedEmotion, 0) == pdPASS) {
                String em = String(receivedEmotion);
                if (em == "parle") { isSpeaking = true; face->setEmotion(FaceEmotion::PARLE); }
                else if (em == "idle") { isSpeaking = false; face->setEmotion(FaceEmotion::NEUTRE); }
                else if (!isSpeaking) face->setEmotionFromString(em);
            }
            face->tick(delta > 50 ? 16 : delta);
            vTaskDelay(8 / portTICK_PERIOD_MS);
        }
    }
}

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(30, true);
    pinMode(MAX_SD_MODE_PIN, OUTPUT); digitalWrite(MAX_SD_MODE_PIN, LOW);
    pinMode(TFT_CS_PIN, OUTPUT); digitalWrite(TFT_CS_PIN, HIGH);
    pinMode(TOUCH_CS_PIN, OUTPUT); digitalWrite(TOUCH_CS_PIN, HIGH);
    pinMode(SD_CS_PIN, OUTPUT); digitalWrite(SD_CS_PIN, HIGH);
    pinMode(TFT_RST_PIN, OUTPUT); digitalWrite(TFT_RST_PIN, HIGH);
    SPI.begin(18, 19, 23, 5);
    int retry = 0;
    while (!SD.begin(5, SPI, 4000000) && retry < 3) { retry++; delay(500); }
    
    display = new TFT_Display();
    audio = new Audio_I2S();
    network = new Network_WS();
    mp3Player = new AudioMP3();
    spotifyUi = new GuiSpotify();

    emotionQueue = xQueueCreate(5, 32);
    commandQueue = xQueueCreate(10, 128);
    audioTxQueue = xQueueCreate(10, sizeof(AudioChunk));
    audioRxQueue = xQueueCreate(10, sizeof(AudioChunk));

    xTaskCreatePinnedToCore(networkTask, "Net", 9216, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(orchestratorTask, "Orc", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(displayTask, "Disp", 10240, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(micTask, "Mic", 6144, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(speakerTask, "Spk", 6144, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(debugTask, "Debug", 2048, NULL, 0, NULL, 0);
}

void loop() { vTaskDelete(NULL); }
