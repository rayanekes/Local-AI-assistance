// main.cpp - Hub Central et Orchestrateur FreeRTOS

#include <Arduino.h>
#include "display_tft.h"
#include "audio_i2s.h"
#include "network_ws.h"
#include "audio_chunk.h"
#include "audio_mp3.h"
#include "gui_spotify.h"

// Tentative d'inclusion de la configuration locale (exclue du Git)
#if __has_include("config.h")
  #include "config.h"
#else
  // Valeurs par défaut si config.h est absent
  #define WIFI_SSID "VOTRE_SSID"
  #define WIFI_PASSWORD "VOTRE_PASSWORD"
  #define WS_SERVER_IP "192.168.x.x"
  #define WS_SERVER_PORT 8765
#endif

// --- Variables Globales Mode Hors-Ligne ---
AudioMP3 mp3Player;
GuiSpotify spotifyUi;
bool isMp3ModeInitialized = false;

// --- Configuration Wi-Fi et Serveur ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* ws_server_ip = WS_SERVER_IP;
const uint16_t ws_server_port = WS_SERVER_PORT;

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
  if (isMp3ModeInitialized) return;
  Serial.println("[STATE] Basculement -> OFFLINE_MP3 (Allocating LVGL & Audio)");

  // Le pointeur TFT est public dans display_tft, on va devoir y accéder (ou faire un getter)
  // Pour la démo, on suppose que l'objet global tft.tft de display est dispo,
  // on ajoutera un getter dans display_tft.h si ça ne compile pas.
  spotifyUi.init(display.getTftPointer());
  spotifyUi.buildInterface();

  mp3Player.init();
  mp3Player.play("/music/test.mp3"); // Fichier test par défaut
  spotifyUi.updateTitle("Titre Test", "Artiste Local");

  isMp3ModeInitialized = true;
}

void destroy_lvgl_mp3_app() {
  if (!isMp3ModeInitialized) return;
  Serial.println("[STATE] Basculement -> ONLINE_AI (Destroying LVGL & Audio)");

  mp3Player.stop();
  spotifyUi.deinit();

  // Nettoyer l'écran après la fermeture de l'UI
  display.getTftPointer()->fillScreen(TFT_BLACK);

  isMp3ModeInitialized = false;
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
        // La gestion du basculement se fera dans displayTask pour éviter les conflits SPI et FreeRTOS
      } else if (cmd == "stop_mp3" && currentState != ONLINE_AI) {
        currentState = ONLINE_AI;
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

  SystemState lastHandledState = ONLINE_AI;

  // Par défaut, l'émotion de démarrage
  currentEmotion = "neutre";
  display.displayEmotion(currentEmotion, currentFrame);

  for (;;) {
    // Gestion des transitions d'état de manière thread-safe dans la tâche d'affichage
    if (currentState != lastHandledState) {
      if (currentState == OFFLINE_MP3) {
        load_lvgl_mp3_app();
      } else if (currentState == ONLINE_AI) {
        destroy_lvgl_mp3_app();

        // Reconfigurer le taux d'échantillonnage de l'I2S pour le TTS IA
        audio.initSpeaker();

        // Reset visuel (sans appeler display.init() pour éviter les fuites SD)
        currentEmotion = "neutre";
        display.displayEmotion(currentEmotion, 1);
      }
      lastHandledState = currentState;
    }

    // Si on est en mode MP3, on ne gère pas les visages AI mais on gère LVGL et l'Audio I2S
    if (currentState == OFFLINE_MP3) {
      if (isMp3ModeInitialized) {
          // Mise à jour de l'UI avec les temps audio
          uint32_t currentTime = mp3Player.getAudioCurrentTime();
          uint32_t duration = mp3Player.getAudioFileDuration();
          spotifyUi.updateProgress(currentTime, duration);
          spotifyUi.togglePlayPauseIcon(mp3Player.isPlaying());

          lv_task_handler(); // Gestion LVGL
          mp3Player.loop();  // Remplissage du buffer I2S depuis la SD
      }
      vTaskDelay(5 / portTICK_PERIOD_MS); // Tick rapide pour LVGL et I2S
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
    // Si on est en mode MP3, la bibliothèque externe gère l'I2S, on ignore les packets réseau
    if (currentState == OFFLINE_MP3) {
      if (xQueueReceive(audioTxQueue, &txChunk, 100 / portTICK_PERIOD_MS) == pdPASS) {
         if (txChunk.data != NULL) free(txChunk.data); // Drop silent
      }
      continue;
    }

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
