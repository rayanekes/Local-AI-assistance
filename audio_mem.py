with open("firmware/src/main.cpp", "r") as f:
    content = f.read()

import re

search1 = r'''// On alloue un buffer pour chaque lecture
    int16_t\* micBuffer = \(int16_t\*\)malloc\(bufferSize\);

    if \(micBuffer != NULL\) \{
      size_t bytesRead = audio.readMic\(micBuffer, bufferSize\);

      if \(bytesRead > 0\) \{
        AudioChunk chunk;
        chunk.data = \(uint8_t\*\)micBuffer;
        chunk.length = bytesRead;

        // Envoyer à networkTask de manière sécurisée
        if \(xQueueSend\(audioRxQueue, &chunk, 0\) != pdPASS\) \{
          free\(micBuffer\); // Queue pleine = on drop le paquet
        \}
      \} else \{
        free\(micBuffer\);
      \}
    \}'''

replace1 = r'''    // Utilisation des buffers statiques via FreeRTOS
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
    }'''

content = re.sub(search1, replace1, content)

search2 = r'''// --- Tâche : Réseau Wi-Fi & WebSocket \(Core 0\) ---
// Gère la connexion et l'envoi thread-safe des flux montants
void networkTask\(void \*pvParameters\) \{
  network.initWiFi\(ssid, password\);
  network.initWebSocket\(ws_server_ip, ws_server_port\);

  AudioChunk rxChunk;

  for \(;;\) \{
    network.loop\(\);

    // Vérifier si la tâche micro a envoyé de l'audio à transmettre
    if \(xQueueReceive\(audioRxQueue, &rxChunk, 0\) == pdPASS\) \{
      if \(rxChunk.data != NULL\) \{
        network.sendAudio\(rxChunk.data, rxChunk.length\);
        free\(rxChunk.data\); // Libérer la mémoire allouée par micTask
      \}
    \}

    vTaskDelay\(10 / portTICK_PERIOD_MS\);
  \}
\}'''

replace2 = r'''// --- Tâche : Réseau Wi-Fi & WebSocket (Core 0) ---
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
}'''

content = re.sub(search2, replace2, content)

search3 = r'''// --- Tâche : Serveur -> Haut-Parleur MAX98357A \(Core 1\) ---
void speakerTask\(void \*pvParameters\) \{
  audio.initSpeaker\(\);

  AudioChunk txChunk;

  for \(;;\) \{
    // Attendre qu'un paquet audio arrive depuis le réseau
    if \(xQueueReceive\(audioTxQueue, &txChunk, portMAX_DELAY\) == pdPASS\) \{
      if \(txChunk.data != NULL\) \{
        // Écrire la taille exacte reçue du serveur
        audio.writeSpeaker\(txChunk.data, txChunk.length\);
        free\(txChunk.data\);
      \}
    \}
  \}
\}'''

replace3 = r'''// --- Tâche : Serveur -> Haut-Parleur MAX98357A (Core 1) ---
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
}'''

content = re.sub(search3, replace3, content)

search4 = r'''// --- Files d'attente \(Queues\) FreeRTOS \(IPC\) ---
QueueHandle_t emotionQueue;
QueueHandle_t audioTxQueue; // Serveur -> ESP32 \(Haut-parleur\)
QueueHandle_t audioRxQueue; // ESP32 \(Micro\) -> Serveur'''

replace4 = r'''// --- Files d'attente (Queues) FreeRTOS (IPC) ---
QueueHandle_t emotionQueue;
QueueHandle_t audioTxQueue; // Serveur -> ESP32 (Haut-parleur)
QueueHandle_t audioRxQueue; // ESP32 (Micro) -> Serveur
QueueHandle_t micFreeQueue;
QueueHandle_t spkFreeQueue;'''

content = re.sub(search4, replace4, content)

search5 = r'''  // Initialisation des Files d'Attente \(IPC\)
  emotionQueue = xQueueCreate\(5, sizeof\(char\[32\]\)\);
  commandQueue = xQueueCreate\(5, sizeof\(char\[32\]\)\);
  audioTxQueue = xQueueCreate\(10, sizeof\(AudioChunk\)\);
  audioRxQueue = xQueueCreate\(10, sizeof\(AudioChunk\)\);

  if \(emotionQueue == NULL \|\| audioTxQueue == NULL \|\| audioRxQueue == NULL \|\| commandQueue == NULL\) \{
    Serial.println\("Erreur critique: Création des Queues FreeRTOS échouée."\);
    while \(1\);
  \}'''

replace5 = r'''  // Initialisation des Files d'Attente (IPC)
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
  }'''

content = re.sub(search5, replace5, content)

with open("firmware/src/main.cpp", "w") as f:
    f.write(content)
