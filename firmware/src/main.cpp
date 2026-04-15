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

// Variable globale pour stocker l'émotion affichée
String currentEmotion = "";

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
    // Timeout court (50ms) pour vérifier fréquemment si c'est le moment d'animer l'écran
    if (xQueueReceive(emotionQueue, &receivedEmotion, 50 / portTICK_PERIOD_MS) == pdPASS) {
      String newEmotion = String(receivedEmotion);
      if (newEmotion != currentEmotion) {
        Serial.print("[Display] Nouvelle émotion: ");
        Serial.println(newEmotion);
        currentEmotion = newEmotion;
        currentFrame = 1; // Toujours commencer par la frame 1
        display.displayEmotion(currentEmotion, currentFrame);
        lastAnimTime = millis();
      }
    } else {
      // Si aucune nouvelle émotion n'est reçue, on gère l'animation de l'émotion actuelle
      unsigned long animDelay = (currentFrame == 1) ? 2000 : 300; // Yeux ouverts 2s, fermés 0.3s

      if (millis() - lastAnimTime > animDelay) {
        // Alterne la frame
        currentFrame = (currentFrame == 1) ? 2 : 1;
        display.displayEmotion(currentEmotion, currentFrame);
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
