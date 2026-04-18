#include "network_ws.h"
#include "audio_chunk.h"

// Variables externes FreeRTOS définies dans main.cpp
extern QueueHandle_t emotionQueue;
extern QueueHandle_t commandQueue;
extern QueueHandle_t audioTxQueue;

// Définition de l'instance statique
WebSocketsClient Network_WS::webSocket;

void Network_WS::initWiFi(const char* ssid, const char* password) {
    Serial.print("Connexion au Wi-Fi ");
    Serial.println(ssid);
    // [Correction] Amorcer la connexion sans bloquer infiniment avec un "while"
    WiFi.begin(ssid, password);
}

void Network_WS::initWebSocket(const char* server_ip, uint16_t server_port) {
    webSocket.begin(server_ip, server_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000); // Reconnexion auto après 5s
    Serial.println("WebSocket Client initialisé.");
}

void Network_WS::loop() {
    // [Correction] Maintenir le Wi-Fi en vie (Reconnexion persistante non-bloquante)
    if (WiFi.status() != WL_CONNECTED) {
        // Optionnel: On pourrait ajouter un timer non bloquant pour tenter WiFi.reconnect()
        // Mais en général, WiFi.begin gère la reconnexion auto dans le background sur ESP32.
        return;
    }
    webSocket.loop();
}

void Network_WS::sendAudio(const uint8_t *payload, size_t length) {
    webSocket.sendBIN(payload, length);
}

void Network_WS::handleJsonMessage(uint8_t * payload) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
        Serial.print("Erreur de parsing JSON depuis WS: ");
        Serial.println(error.c_str());
        return;
    }

    // --- Gestion des commandes (Dual-State Machine) ---
    if (doc.containsKey("command")) {
        const char* command = doc["command"];
        char cmdToSend[32];
        strncpy(cmdToSend, command, sizeof(cmdToSend) - 1);
        cmdToSend[sizeof(cmdToSend) - 1] = '\0';

        if (commandQueue != NULL) {
            xQueueSend(commandQueue, &cmdToSend, 0);
        }
    }

    // --- Gestion de l'affichage (Émotions normales) ---
    if (doc.containsKey("emotion")) {
        const char* emotion = doc["emotion"];
        char emotionToSend[32];
        strncpy(emotionToSend, emotion, sizeof(emotionToSend) - 1);
        emotionToSend[sizeof(emotionToSend) - 1] = '\0';

        if (emotionQueue != NULL) {
            xQueueSend(emotionQueue, &emotionToSend, 0);
        }
    }

    // --- Gestion de la latence (Ruse de l'état "Thinking") ---
    if (doc.containsKey("status")) {
        const char* status = doc["status"];
        if (strcmp(status, "thinking") == 0) {
            // Le serveur est en train de réfléchir (latence IA)
            char emotionToSend[32] = "reflexion";
            if (emotionQueue != NULL) {
                xQueueSend(emotionQueue, &emotionToSend, 0);
            }
        } else if (strcmp(status, "speaking") == 0) {
            // Le serveur commence à parler (TTS)
            char emotionToSend[32] = "parle";
            if (emotionQueue != NULL) {
                xQueueSend(emotionQueue, &emotionToSend, 0);
            }
        } else if (strcmp(status, "idle") == 0) {
            // Le serveur a fini de parler
            char emotionToSend[32] = "idle";
            if (emotionQueue != NULL) {
                xQueueSend(emotionQueue, &emotionToSend, 0);
            }
        }
    }
}

void Network_WS::webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Déconnecté du serveur.");
            break;

        case WStype_CONNECTED:
            Serial.println("[WS] Connecté au serveur !");
            break;

        case WStype_TEXT:
            // Le serveur a envoyé un JSON
            handleJsonMessage(payload);
            break;

        case WStype_BIN:
            // Le serveur a envoyé de l'audio TTS
            if (audioTxQueue != NULL) {
                extern QueueHandle_t spkFreeQueue; // Accès à la free queue
                uint8_t* audioData = NULL;

                size_t offset = 0;
                while (offset < length) {
                    uint8_t* audioData = NULL;
                    if (xQueueReceive(spkFreeQueue, &audioData, 0) == pdPASS) {
                        size_t remaining = length - offset;
                        size_t copyLength = remaining > SPK_BUFFER_SIZE ? SPK_BUFFER_SIZE : remaining;
                        memcpy(audioData, payload + offset, copyLength);

                        AudioChunk chunk;
                        chunk.data = audioData;
                        chunk.length = copyLength;

                        if (xQueueSend(audioTxQueue, &chunk, 0) != pdPASS) {
                            xQueueSend(spkFreeQueue, &audioData, 0);
                            Serial.println("[WS] Erreur: audioTxQueue pleine !");
                            break;
                        }
                        offset += copyLength;
                    } else {
                        Serial.println("[WS] Erreur: Plus de buffers disponibles pour audioTxQueue !");
                        break;
                    }
                }
            }
            break;

        default:
            break;
    }
}
