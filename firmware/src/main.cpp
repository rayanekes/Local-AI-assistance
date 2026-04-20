// main.cpp - Hub Central et Orchestrateur FreeRTOS

#include <Arduino.h>
#include "display_tft.h"
#include "audio_i2s.h"
#include "network_ws.h"
#include "audio_chunk.h"

// --- Configuration Wi-Fi et Serveur ---
const char* ssid = "VOTRE_SSID";
const char* password = "VOTRE_PASSWORD";
const char* ws_server_ip = "192.168.x.x"; // IP de votre PC Pop!_OS
const uint16_t ws_server_port = 8765;

uint8_t micBuffers[10][1024];
uint8_t spkBuffers[10][4096];

// --- Instances des Modules ---
TFT_Display display;
Audio_I2S audio;
Network_WS network;

// --- Files d'attente (Queues) FreeRTOS (IPC) ---
QueueHandle_t emotionQueue;
QueueHandle_t audioTxQueue; // Serveur -> ESP32 (Haut-parleur)
QueueHandle_t audioRxQueue; // ESP32 (Micro) -> Serveur
QueueHandle_t micFreeQueue;
QueueHandle_t spkFreeQueue;

// --- Architecture Dual-State (Machine à États) ---
enum SystemState {
  OFFLINE_MP3, // Mode 1: Serveur IA injoignable. Lecteur MP3 Autonome (Carte SD -> I2S_1). Interface manuelle.
  ONLINE_AI    // Mode 2: Serveur IA connecté. Full-Duplex WebSockets (I2S_0 + I2S_1). Interface "Visage/Émotions".
};
// Modifié pour que le mode en-ligne soit le mode par défaut au boot (ou géré dynamiquement)
SystemState currentState = ONLINE_AI;

// Nouvelle Queue pour le changement d'état "Dual-State"
QueueHandle_t commandQueue;

/*
 * [RÉVISION ARCHITECTURALE - JULES]
 * Décision concernant la librairie GUI pour le Mode MP3 (OFFLINE_MP3) :
 * -> LVGL (Option B) APPROUVÉE SOUS CONDITION DE "TEARDOWN" STRICT.
 *
 * Explication:
 * Suite à l'analyse de l'utilisateur, l'utilisation de LVGL est acceptée.
 * Pour éviter la saturation des 520 Ko de RAM de l'ESP32, nous allons implémenter un "Teardown" dynamique (Allocation/Désallocation totale).
 *
 * Fonctionnement :
 * 1. Le mode "ONLINE_AI" est le mode par défaut.
 * 2. Si l'utilisateur demande la musique au serveur IA, le serveur envoie un JSON : {"command": "start_mp3"}.
 * 3. L'ESP32 reçoit la commande, suspend les tâches I2S Microphone (désallocation des buffers), et alloue dynamiquement
 *    la mémoire de LVGL (lv_init) pour charger la "Mini App Lecteur".
 * 4. Lorsque le Wi-Fi se connecte (ou via un bouton "Quitter" sur le TFT), l'ESP32 déclenche un nettoyage absolu (lv_deinit() ou équivalent),
 *    libérant les ~40Ko de RAM utilisés par la GUI avant de relancer les tâches IA WebSockets.
 */

#include <lvgl.h>

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1;
static lv_disp_drv_t disp_drv;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  // Ignored right now, but would flush to TFT in a real setup
  lv_disp_flush_ready(disp);
}

// --- Fonctions de basculement d'architecture (State Machine) ---
void load_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> OFFLINE_MP3");

  lv_init();

  buf1 = (lv_color_t *)malloc(320 * 40 * sizeof(lv_color_t));
  if (buf1 == NULL) {
      Serial.println("LVGL malloc failed");
      return;
  }

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 40);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_obj_t * bg = lv_obj_create(lv_scr_act());
  lv_obj_set_size(bg, 320, 240);
  lv_obj_set_style_bg_color(bg, lv_color_hex(0x1DB954), 0); // Spotify Green

  lv_obj_t * label = lv_label_create(bg);
  lv_label_set_text(label, "Spotify Player");
  lv_obj_center(label);

  lv_timer_handler();
}

void destroy_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> ONLINE_AI");
  lv_obj_clean(lv_scr_act());
  lv_deinit();
  if (buf1 != NULL) {
      free(buf1);
      buf1 = NULL;
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
    // Si on est en mode MP3, on ne gère pas les visages
    if (currentState == OFFLINE_MP3) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
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
        xQueueSend(micFreeQueue, &rxChunk.data, 0); // Retour au pool libre
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

        // Utilisation des buffers statiques via FreeRTOS
    uint8_t* micBuffer;
    if (xQueueReceive(micFreeQueue, &micBuffer, portMAX_DELAY) == pdPASS) {
      size_t bytesRead = audio.readMic((int16_t*)micBuffer, bufferSize);
      if (bytesRead > 0) {
        AudioChunk chunk;
        chunk.data = micBuffer;
        chunk.length = bytesRead;

        if (xQueueSend(audioRxQueue, &chunk, 0) != pdPASS) {
          xQueueSend(micFreeQueue, &micBuffer, 0); // Drop le paquet
        }
      } else {
        xQueueSend(micFreeQueue, &micBuffer, 0);
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
        xQueueSend(spkFreeQueue, &txChunk.data, 0); // Retour au pool libre
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
  micFreeQueue = xQueueCreate(10, sizeof(uint8_t*));
  spkFreeQueue = xQueueCreate(10, sizeof(uint8_t*));

  if (emotionQueue == NULL || audioTxQueue == NULL || audioRxQueue == NULL || commandQueue == NULL || micFreeQueue == NULL || spkFreeQueue == NULL) {
    Serial.println("Erreur critique: Création des Queues FreeRTOS échouée.");
    while (1);
  }

  // Pre-fill free queues with pointers to static buffers
  for (int i = 0; i < 10; i++) {
    uint8_t* mptr = micBuffers[i];
    uint8_t* sptr = spkBuffers[i];
    xQueueSend(micFreeQueue, &mptr, 0);
    xQueueSend(spkFreeQueue, &sptr, 0);
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
