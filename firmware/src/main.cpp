// main.cpp - Hub Central et Orchestrateur FreeRTOS

#include <Arduino.h>
#include "display_tft.h"
#include "audio_i2s.h"
#include "network_ws.h"
#include "audio_chunk.h"
#include <lvgl.h>

// --- Configuration Wi-Fi et Serveur ---
const char* ssid = "VOTRE_SSID";
const char* password = "VOTRE_PASSWORD";
const char* ws_server_ip = "192.168.x.x"; // IP de votre PC Pop!_OS
const uint16_t ws_server_port = 8765;

// --- Instances des Modules ---
TFT_Display display;
Audio_I2S audio;
Network_WS network;

// --- Files d'attente (Queues) FreeRTOS (IPC) ---
QueueHandle_t emotionQueue;
QueueHandle_t audioTxQueue; // Serveur -> ESP32 (Haut-parleur)
QueueHandle_t audioRxQueue; // ESP32 (Micro) -> Serveur

// --- Architecture Dual-State (Machine à États) ---
enum SystemState {
  OFFLINE_MP3, // Mode 1: Serveur IA injoignable. Lecteur MP3 Autonome (Carte SD -> I2S_1). Interface manuelle.
  ONLINE_AI    // Mode 2: Serveur IA connecté. Full-Duplex WebSockets (I2S_0 + I2S_1). Interface "Visage/Émotions".
};
// Modifié pour que le mode en-ligne soit le mode par défaut au boot (ou géré dynamiquement)
SystemState currentState = ONLINE_AI;

// Nouvelle Queue pour le changement d'état "Dual-State"
QueueHandle_t commandQueue;

// --- LVGL Variables ---
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static lv_obj_t *mp3_screen = NULL;

// Display flushing function for LVGL
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    display.tft.startWrite();
    display.tft.setAddrWindow(area->x1, area->y1, w, h);
    display.tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    display.tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Touchpad read function for LVGL
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;
    bool touched = display.tft.getTouch(&touchX, &touchY);

    if(!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

// Button callback to go back to AI mode
static void btn_exit_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        // Send stop_mp3 command to orchestrator via queue
        char cmdToSend[32] = "stop_mp3";
        if (commandQueue != NULL) {
            xQueueSend(commandQueue, &cmdToSend, 0);
        }
    }
}

// --- Fonctions de basculement d'architecture (State Machine) ---
void load_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> OFFLINE_MP3");

  lv_init();

  // Allocate display buffer dynamically
  size_t buffer_size = display.tft.width() * 20; // 20 lines buffer
  buf1 = (lv_color_t *)malloc(buffer_size * sizeof(lv_color_t));
  if (!buf1) {
      Serial.println("LVGL buffer allocation failed!");
      return;
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, buffer_size);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = display.tft.width();
  disp_drv.ver_res = display.tft.height();
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // --- UI Spotify ---
  mp3_screen = lv_obj_create(NULL);
  lv_scr_load(mp3_screen);
  lv_obj_set_style_bg_color(mp3_screen, lv_color_hex(0x121212), 0); // Spotify dark background

  // Title
  lv_obj_t * label_title = lv_label_create(mp3_screen);
  lv_label_set_text(label_title, "Spotify Clone");
  lv_obj_set_style_text_color(label_title, lv_color_hex(0x1DB954), 0); // Spotify green
  // Use default font instead of missing font
  // lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, 0);
  lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

  // Playback controls (Play, Pause, Next)
  lv_obj_t * btn_play = lv_btn_create(mp3_screen);
  lv_obj_align(btn_play, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(btn_play, lv_color_hex(0x1DB954), 0);
  lv_obj_set_style_radius(btn_play, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_size(btn_play, 60, 60);
  lv_obj_t * label_play = lv_label_create(btn_play);
  lv_label_set_text(label_play, LV_SYMBOL_PLAY);
  lv_obj_center(label_play);

  // Exit button
  lv_obj_t * btn_exit = lv_btn_create(mp3_screen);
  lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_set_style_bg_color(btn_exit, lv_color_hex(0xFF0000), 0);
  lv_obj_add_event_cb(btn_exit, btn_exit_event_cb, LV_EVENT_ALL, NULL);
  lv_obj_t * label_exit = lv_label_create(btn_exit);
  lv_label_set_text(label_exit, "Exit to AI");
  lv_obj_center(label_exit);
}

void destroy_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> ONLINE_AI");

  if (buf1) {
      // Complete teardown of LVGL
      lv_deinit();
      free(buf1);
      buf1 = NULL;
      mp3_screen = NULL;
  }
}

// Variables globales pour stocker l'état visuel
String currentEmotion = "";
bool isSpeaking = false;

// ==========================================
// TÂCHES FREERTOS
// ==========================================

// --- Tâche : Cerveau / Orchestrateur de Commandes ---
// Gère les commandes "start_mp3" et "stop_mp3" depuis le WebSocket
void orchestratorTask(void *pvParameters) {
  char receivedCommand[32];
  for(;;) {
    if (xQueueReceive(commandQueue, &receivedCommand, portMAX_DELAY) == pdPASS) {
      String cmd = String(receivedCommand);
      if (cmd == "start_mp3" && currentState != OFFLINE_MP3) {
        currentState = OFFLINE_MP3;
        load_lvgl_mp3_app();
      } else if (cmd == "stop_mp3" && currentState != ONLINE_AI) {
        currentState = ONLINE_AI;
        destroy_lvgl_mp3_app();

        // Reset visuel
        display.init();
        currentEmotion = "neutre";
        display.displayEmotion(currentEmotion, 1);
      }
    }
  }
}

// --- Tâche : Affichage TFT (Core 1) ---
// Gère l'affichage asynchrone et les animations (clignements)
void displayTask(void *pvParameters) {
  display.init();

  char receivedEmotion[32];
  int currentFrame = 1;
  unsigned long lastAnimTime = 0;

  // Par défaut, l'émotion de démarrage
  currentEmotion = "neutre";
  display.displayEmotion(currentEmotion, currentFrame);

  for (;;) {
    // Si on est en mode MP3, on ne gère pas les visages, on gère LVGL
    if (currentState == OFFLINE_MP3) {
      lv_timer_handler();
      vTaskDelay(5 / portTICK_PERIOD_MS); // LVGL timer tick
      continue;
    }

    // Timeout court (50ms)
    if (xQueueReceive(emotionQueue, &receivedEmotion, 50 / portTICK_PERIOD_MS) == pdPASS) {
      String newEmotion = String(receivedEmotion);

      if (newEmotion == "parle") {
        isSpeaking = true;
      } else if (newEmotion == "idle") {
        isSpeaking = false;
        // Restaurer l'émotion actuelle
        display.displayEmotion(currentEmotion, currentFrame);
      } else {
        currentEmotion = newEmotion;
      }

      // Toujours commencer par la frame 1 lors d'un changement
      currentFrame = 1;
      lastAnimTime = millis();

      String emotionToDisplay = isSpeaking ? "parle" : currentEmotion;
      display.displayEmotion(emotionToDisplay, currentFrame);

    } else {
      // Gestion de l'animation de l'émotion en cours
      String emotionToDisplay = isSpeaking ? "parle" : currentEmotion;

      // La bouche s'anime beaucoup plus vite (200ms) que les yeux (2000ms/300ms)
      unsigned long animDelay;
      if (isSpeaking) {
        animDelay = 200;
      } else {
        animDelay = (currentFrame == 1) ? 2000 : 300;
      }

      if (millis() - lastAnimTime > animDelay) {
        currentFrame = (currentFrame == 1) ? 2 : 1;
        display.displayEmotion(emotionToDisplay, currentFrame);
        lastAnimTime = millis();
      }
    }
  }
}

// --- Tâche : Réseau Wi-Fi & WebSocket (Core 0) ---
// Gère la connexion et l'envoi thread-safe des flux montants
void networkTask(void *pvParameters) {
  network.initWiFi(ssid, password);
  network.initWebSocket(ws_server_ip, ws_server_port);

  AudioChunk rxChunk;

  for (;;) {
    network.loop();

    // Vérifier si la tâche micro a envoyé de l'audio à transmettre
    if (xQueueReceive(audioRxQueue, &rxChunk, 0) == pdPASS) {
      if (rxChunk.data != NULL) {
        network.sendAudio(rxChunk.data, rxChunk.length);
        free(rxChunk.data); // Libérer la mémoire allouée par micTask
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// --- Tâche : Audio Micro INMP441 -> Serveur (Core 1) ---
void micTask(void *pvParameters) {
  audio.initMic();

  const size_t bufferSize = 1024;

  for (;;) {
    // Si on est en mode Lecteur MP3, on suspend la capture micro pour économiser CPU/RAM
    if (currentState == OFFLINE_MP3) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    // On alloue un buffer pour chaque lecture
    int16_t* micBuffer = (int16_t*)malloc(bufferSize);

    if (micBuffer != NULL) {
      size_t bytesRead = audio.readMic(micBuffer, bufferSize);

      if (bytesRead > 0) {
        AudioChunk chunk;
        chunk.data = (uint8_t*)micBuffer;
        chunk.length = bytesRead;

        // Envoyer à networkTask de manière sécurisée
        if (xQueueSend(audioRxQueue, &chunk, 0) != pdPASS) {
          free(micBuffer); // Queue pleine = on drop le paquet
        }
      } else {
        free(micBuffer);
      }
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// --- Tâche : Serveur -> Haut-Parleur MAX98357A (Core 1) ---
void speakerTask(void *pvParameters) {
  audio.initSpeaker();

  AudioChunk txChunk;

  for (;;) {
    // Attendre qu'un paquet audio arrive depuis le réseau
    if (xQueueReceive(audioTxQueue, &txChunk, portMAX_DELAY) == pdPASS) {
      if (txChunk.data != NULL) {
        // Écrire la taille exacte reçue du serveur
        audio.writeSpeaker(txChunk.data, txChunk.length);
        free(txChunk.data);
      }
    }
  }
}

// ==========================================
// SETUP & LOOP (Hub Central)
// ==========================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- Démarrage du Client ESP32 Robot IA ---");

  // Initialisation des Files d'Attente (IPC)
  emotionQueue = xQueueCreate(5, sizeof(char[32]));
  commandQueue = xQueueCreate(5, sizeof(char[32]));
  audioTxQueue = xQueueCreate(10, sizeof(AudioChunk));
  audioRxQueue = xQueueCreate(10, sizeof(AudioChunk));

  if (emotionQueue == NULL || audioTxQueue == NULL || audioRxQueue == NULL || commandQueue == NULL) {
    Serial.println("Erreur critique: Création des Queues FreeRTOS échouée.");
    while (1);
  }

  // Lancement de la tâche Réseau sur le Core 0
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, NULL, 0);

  // Lancement de la tâche d'Orchestration (State Machine) sur Core 1
  xTaskCreatePinnedToCore(orchestratorTask, "OrchestratorTask", 2048, NULL, 1, NULL, 1);

  // Lancement des tâches Matérielles sur le Core 1
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(micTask, "MicTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(speakerTask, "SpeakerTask", 4096, NULL, 3, NULL, 1); // Plus haute priorité pour l'audio

  Serial.println("Toutes les tâches FreeRTOS sont lancées !");
}

void loop() {
  // Le main.cpp est désormais vide, FreeRTOS gère tout via ses tâches.
  vTaskDelete(NULL); // Détruit la tâche loop() par défaut pour économiser de la RAM
}
