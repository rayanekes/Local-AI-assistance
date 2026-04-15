// main.cpp - Point d'entrée pour l'ESP32

#include <Arduino.h>
#include <ArduinoJson.h>
#include "TFT_Display.h"

// Instance de l'écran TFT
TFT_Display display;

// File d'attente (Queue) FreeRTOS pour communiquer les émotions entre les cœurs
QueueHandle_t emotionQueue;

// Variable globale pour stocker l'émotion actuelle et éviter les rafraîchissements inutiles
String currentEmotion = "";

// Tâche FreeRTOS exécutée sur le Core 0 pour gérer exclusivement l'écran
void displayTask(void *pvParameters) {
  display.init();

  char receivedEmotion[32];

  for (;;) {
    // On attend indéfiniment jusqu'à recevoir une nouvelle émotion dans la queue
    if (xQueueReceive(emotionQueue, &receivedEmotion, portMAX_DELAY) == pdPASS) {
      String newEmotion = String(receivedEmotion);

      if (newEmotion != currentEmotion) {
        Serial.print("[Core 0] Affichage de la nouvelle émotion: ");
        Serial.println(newEmotion);

        display.displayEmotion(newEmotion);
        currentEmotion = newEmotion;
      }
    }
  }
}

// Fonction appelée par la boucle principale (Core 1) lors de la réception d'un JSON
void parseAndHandleJSON(String jsonString) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("Erreur de parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Si le JSON contient une émotion, on l'envoie à la tâche d'affichage via la Queue
  if (doc.containsKey("emotion")) {
    const char* emotion = doc["emotion"];
    char emotionToSend[32];
    strncpy(emotionToSend, emotion, sizeof(emotionToSend) - 1);
    emotionToSend[sizeof(emotionToSend) - 1] = '\0';

    // Envoyer l'émotion dans la file d'attente
    xQueueSend(emotionQueue, &emotionToSend, portMAX_DELAY);
  }

  if (doc.containsKey("speech")) {
    const char* speech = doc["speech"];
    Serial.print("[Core 1] Speech généré: ");
    Serial.println(speech);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Démarrage du robot...");

  // Création de la file d'attente (jusqu'à 5 émotions de 32 caractères max)
  emotionQueue = xQueueCreate(5, sizeof(char[32]));

  if (emotionQueue == NULL) {
    Serial.println("Erreur: Impossible de créer la file d'attente FreeRTOS.");
    while (1); // Bloquer l'exécution en cas d'erreur critique
  }

  // Création de la tâche dédiée à l'écran sur le Core 0
  xTaskCreatePinnedToCore(
    displayTask,    /* Fonction de la tâche */
    "DisplayTask",  /* Nom de la tâche */
    4096,           /* Taille de la pile (Stack size) en mots (words) */
    NULL,           /* Paramètres */
    1,              /* Priorité de la tâche (1 = basse) */
    NULL,           /* Handle de la tâche */
    0               /* Core 0 (0 pour l'écran, 1 pour l'I2S/Wi-Fi) */
  );

  Serial.println("Initialisation FreeRTOS terminée.");
}

void loop() {
  // --- Simulation de réception JSON ---
  // Le Core 1 gérera ici la réception WebSocket et l'audio I2S.
  // Pour l'instant, on simule l'arrivée d'un JSON toutes les 5 secondes.

  static unsigned long lastUpdate = 0;
  static int state = 0;

  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    String fakeJson = "";

    if (state == 0) {
      fakeJson = "{\"speech\": \"Bonjour je suis content.\", \"emotion\": \"joie\"}";
      state = 1;
    } else if (state == 1) {
      fakeJson = "{\"speech\": \"Je suis en attente.\", \"emotion\": \"neutre\"}";
      state = 2;
    } else {
      fakeJson = "{\"speech\": \"Oh non c'est dommage.\", \"emotion\": \"triste\"}";
      state = 0;
    }

    Serial.println("\n[Core 1] Simulation réception WebSocket...");
    parseAndHandleJSON(fakeJson);
  }
}
