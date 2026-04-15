#include "network_ws.h"
#include "audio_chunk.h"

// Variables externes FreeRTOS définies dans main.cpp
extern QueueHandle_t emotionQueue;
extern QueueHandle_t audioTxQueue;

// Définition de l'instance statique
WebSocketsClient Network_WS::webSocket;

void Network_WS::initWiFi(const char* ssid, const char* password) {
    Serial.print("Connexion au Wi-Fi ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi connecté !");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void Network_WS::initWebSocket(const char* server_ip, uint16_t server_port) {
    webSocket.begin(server_ip, server_port, "/");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000); // Reconnexion auto après 5s
    Serial.println("WebSocket Client initialisé.");
}

void Network_WS::loop() {
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

    if (doc.containsKey("emotion")) {
        const char* emotion = doc["emotion"];
        char emotionToSend[32];
        strncpy(emotionToSend, emotion, sizeof(emotionToSend) - 1);
        emotionToSend[sizeof(emotionToSend) - 1] = '\0';

        // Envoyer l'émotion à la tâche d'affichage
        if (emotionQueue != NULL) {
            xQueueSend(emotionQueue, &emotionToSend, 0); // Non-bloquant
        }
    }

    if (doc.containsKey("speech")) {
        const char* speech = doc["speech"];
        Serial.print("[Serveur IA] ");
        Serial.println(speech);
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
                // Allouer la mémoire pour le payload
                uint8_t* audioData = (uint8_t*)malloc(length);
                if (audioData != NULL) {
                    memcpy(audioData, payload, length);

                    // Créer la structure contenant le pointeur et la taille
                    AudioChunk chunk;
                    chunk.data = audioData;
                    chunk.length = length;

                    if (xQueueSend(audioTxQueue, &chunk, 0) != pdPASS) {
                        free(audioData);
                        Serial.println("[WS] Erreur: audioTxQueue pleine !");
                    }
                }
            }
            break;

        default:
            break;
    }
}
