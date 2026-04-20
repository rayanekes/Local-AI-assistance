with open("firmware/src/network_ws.cpp", "r") as f:
    content = f.read()

import re

search = r'''        case WStype_BIN:
            // Le serveur a envoyé de l'audio TTS
            if \(audioTxQueue != NULL\) \{
                // Allouer la mémoire pour le payload
                uint8_t\* audioData = \(uint8_t\*\)malloc\(length\);
                if \(audioData != NULL\) \{
                    memcpy\(audioData, payload, length\);

                    // Créer la structure contenant le pointeur et la taille
                    AudioChunk chunk;
                    chunk.data = audioData;
                    chunk.length = length;

                    if \(xQueueSend\(audioTxQueue, &chunk, 0\) != pdPASS\) \{
                        free\(audioData\);
                        Serial.println\("\[WS\] Erreur: audioTxQueue pleine !"\);
                    \}
                \}
            \}
            break;'''

replace = r'''        case WStype_BIN:
            // Le serveur a envoyé de l'audio TTS
            extern QueueHandle_t spkFreeQueue;
            if (audioTxQueue != NULL && spkFreeQueue != NULL) {
                uint8_t* audioData;
                if (xQueueReceive(spkFreeQueue, &audioData, 0) == pdPASS) {
                    size_t copyLen = length > 4096 ? 4096 : length;
                    memcpy(audioData, payload, copyLen);

                    // Créer la structure contenant le pointeur et la taille
                    AudioChunk chunk;
                    chunk.data = audioData;
                    chunk.length = copyLen;

                    if (xQueueSend(audioTxQueue, &chunk, 0) != pdPASS) {
                        xQueueSend(spkFreeQueue, &audioData, 0);
                        Serial.println("[WS] Erreur: audioTxQueue pleine !");
                    }
                }
            }
            break;'''

content = re.sub(search, replace, content)

with open("firmware/src/network_ws.cpp", "w") as f:
    f.write(content)
