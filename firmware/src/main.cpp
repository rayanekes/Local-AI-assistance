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

// --- Architecture Dual-State (Machine à États) ---
enum SystemState {
  OFFLINE_MP3, // Mode 1: Serveur IA injoignable. Lecteur MP3 Autonome (Carte SD -> I2S_1). Interface manuelle.
  ONLINE_AI    // Mode 2: Serveur IA connecté. Full-Duplex WebSockets (I2S_0 + I2S_1). Interface "Visage/Émotions".
};
SystemState currentState = OFFLINE_MP3;

/*
 * [JUSTIFICATION ARCHITECTURALE - JULES]
 * Décision concernant la librairie GUI pour le Mode MP3 (OFFLINE_MP3) :
 * -> OPTION A (TFT_eSPI pure) validée.
 *
 * Explication:
 * Bien que LVGL (Option B) soit plus esthétique, elle alloue dynamiquement des dizaines de Ko de RAM pour
 * ses buffers de dessin et ses arbres d'objets (Widgets). L'ESP32 ne disposant que de 520 Ko de SRAM statique,
 * jongler entre la destruction totale d'une interface LVGL et le démarrage des buffers I2S DMA + WebSockets
 * (MbedTLS) lors d'un basculement d'état risque très fortement de causer une fragmentation fatale du tas (Heap Fragmentation).
 *
 * En dessinant l'interface MP3 à la main avec TFT_eSPI (fillRoundRect, drawString), nous consommons 0 Ko
 * de RAM en mémoire persistante. Le nettoyage ("nettoyage strict") se résume à un simple `tft.fillScreen(BLACK)`
 * avant de lancer la boucle d'images BMP pour le mode IA. C'est la seule architecture garantie "Memory-Leak Free" ici.
 */

// Variables globales pour stocker l'état visuel
String currentEmotion = "";
bool isSpeaking = false;

// ==========================================
// TÂCHES FREERTOS
// ==========================================

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
  audioTxQueue = xQueueCreate(10, sizeof(AudioChunk));
  audioRxQueue = xQueueCreate(10, sizeof(AudioChunk));

  if (emotionQueue == NULL || audioTxQueue == NULL || audioRxQueue == NULL) {
    Serial.println("Erreur critique: Création des Queues FreeRTOS échouée.");
    while (1);
  }

  // Lancement de la tâche Réseau sur le Core 0
  xTaskCreatePinnedToCore(networkTask, "NetworkTask", 8192, NULL, 1, NULL, 0);

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
