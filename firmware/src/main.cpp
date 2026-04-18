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

// --- Instances des Modules ---
TFT_Display display;
Audio_I2S audio;
Network_WS network;

// --- Files d'attente (Queues) FreeRTOS (IPC) ---
QueueHandle_t emotionQueue;
QueueHandle_t audioTxQueue; // Serveur -> ESP32 (Haut-parleur)
QueueHandle_t audioRxQueue; // ESP32 (Micro) -> Serveur

// --- Static Memory Pools for Audio ---
// Pour éviter la fragmentation RAM causée par malloc/free dans les tâches I2S
uint8_t micBuffers[MIC_BUFFER_COUNT][MIC_BUFFER_SIZE];
QueueHandle_t micFreeQueue;

uint8_t spkBuffers[SPK_BUFFER_COUNT][SPK_BUFFER_SIZE];
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

// --- Fonctions de basculement d'architecture (State Machine) ---
void load_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> OFFLINE_MP3");
  // TODO: malloc des buffers LVGL, init écran, création widgets
}

void destroy_lvgl_mp3_app() {
  Serial.println("[STATE] Basculement -> ONLINE_AI");
  // TODO: lv_obj_del(lv_scr_act()), free des buffers, flush complet de la mémoire
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

// --- Tâche : Bouton Invisible TFT ---
// Scrute les événements de l'écran tactile pour le basculement manuel
void touchTask(void *pvParameters) {
  uint16_t x, y;
  bool touchPressed = false;
  unsigned long lastTouchTime = 0;

  for (;;) {
    if (display.getTouch(&x, &y)) {
      if (!touchPressed && (millis() - lastTouchTime > 500)) { // Debounce de 500ms
        touchPressed = true;
        lastTouchTime = millis();
        char cmdToSend[32];
        if (currentState == ONLINE_AI) {
          strcpy(cmdToSend, "start_mp3");
        } else {
          strcpy(cmdToSend, "stop_mp3");
        }
        xQueueSend(commandQueue, &cmdToSend, 0);
        Serial.println("[TOUCH] Bouton invisible pressé : Basculement d'état !");
      }
    } else {
      touchPressed = false;
    }
    vTaskDelay(50 / portTICK_PERIOD_MS); // Polling toutes les 50ms
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
        // Remettre le buffer dans la queue des buffers libres
        xQueueSend(micFreeQueue, &rxChunk.data, 0);
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// --- Tâche : Audio Micro INMP441 -> Serveur (Core 1) ---
void micTask(void *pvParameters) {
  audio.initMic();

  for (;;) {
    // Si on est en mode Lecteur MP3, on suspend la capture micro pour économiser CPU/RAM
    if (currentState == OFFLINE_MP3) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    uint8_t* micBuffer = NULL;
    // Récupère un buffer libre depuis la pool
    if (xQueueReceive(micFreeQueue, &micBuffer, 10 / portTICK_PERIOD_MS) == pdPASS) {
      size_t bytesRead = audio.readMic((int16_t*)micBuffer, MIC_BUFFER_SIZE);

      if (bytesRead > 0) {
        AudioChunk chunk;
        chunk.data = micBuffer;
        chunk.length = bytesRead;

        // Envoyer à networkTask de manière sécurisée
        if (xQueueSend(audioRxQueue, &chunk, 0) != pdPASS) {
          xQueueSend(micFreeQueue, &micBuffer, 0); // Queue pleine = on drop le paquet, retourne à la pool
        }
      } else {
        xQueueSend(micFreeQueue, &micBuffer, 0); // Rien lu, retourne à la pool
      }
    } else {
      // Plus de buffer disponible
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
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
        // Remettre le buffer dans la queue libre
        xQueueSend(spkFreeQueue, &txChunk.data, 0);
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
  audioTxQueue = xQueueCreate(SPK_BUFFER_COUNT, sizeof(AudioChunk));
  audioRxQueue = xQueueCreate(MIC_BUFFER_COUNT, sizeof(AudioChunk));
  micFreeQueue = xQueueCreate(MIC_BUFFER_COUNT, sizeof(uint8_t*));
  spkFreeQueue = xQueueCreate(SPK_BUFFER_COUNT, sizeof(uint8_t*));

  if (emotionQueue == NULL || audioTxQueue == NULL || audioRxQueue == NULL || commandQueue == NULL || micFreeQueue == NULL || spkFreeQueue == NULL) {
    Serial.println("Erreur critique: Création des Queues FreeRTOS échouée.");
    while (1);
  }

  // Initialiser les free queues avec les pointeurs vers les buffers statiques
  for (int i = 0; i < MIC_BUFFER_COUNT; i++) {
    uint8_t* ptr = micBuffers[i];
    xQueueSend(micFreeQueue, &ptr, 0);
  }
  for (int i = 0; i < SPK_BUFFER_COUNT; i++) {
    uint8_t* ptr = spkBuffers[i];
    xQueueSend(spkFreeQueue, &ptr, 0);
  }

  // Lancement de la tâche Réseau sur le Core 0
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, NULL, 0);

  // Lancement de la tâche d'Orchestration (State Machine) sur Core 1
  xTaskCreatePinnedToCore(orchestratorTask, "OrchestratorTask", 2048, NULL, 1, NULL, 1);

  // Lancement des tâches Matérielles sur le Core 1
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(touchTask, "TouchTask", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(micTask, "MicTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(speakerTask, "SpeakerTask", 4096, NULL, 3, NULL, 1); // Plus haute priorité pour l'audio

  Serial.println("Toutes les tâches FreeRTOS sont lancées !");
}

void loop() {
  // Le main.cpp est désormais vide, FreeRTOS gère tout via ses tâches.
  vTaskDelete(NULL); // Détruit la tâche loop() par défaut pour économiser de la RAM
}
