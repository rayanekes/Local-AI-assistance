// main.cpp - Point d'entrée pour l'ESP32

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <ArduinoJson.h>

// Initialisation de l'écran TFT
TFT_eSPI tft = TFT_eSPI();

// Pin CS pour la carte SD (à adapter selon le câblage)
#define SD_CS 5

// Variable pour stocker l'émotion actuelle
String currentEmotion = "";

void drawBmp(const char *filename, int16_t x, int16_t y) {
  if ((x >= tft.width()) || (y >= tft.height())) return;

  File bmpFS;
  bmpFS = SD.open(filename, "r");

  if (!bmpFS) {
    Serial.print("File not found: ");
    Serial.println(filename);
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row, col;
  uint8_t  r, g, b;

  if (bmpFS.read() == 'B' && bmpFS.read() == 'M') {
    bmpFS.read(); bmpFS.read(); bmpFS.read(); bmpFS.read();
    bmpFS.read(); bmpFS.read(); bmpFS.read(); bmpFS.read();
    seekOffset = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);
    bmpFS.read(); bmpFS.read(); bmpFS.read(); bmpFS.read();

    w = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);
    h = bmpFS.read() | (bmpFS.read() << 8) | (bmpFS.read() << 16) | (bmpFS.read() << 24);

    bmpFS.read(); bmpFS.read();
    uint16_t depth = bmpFS.read() | (bmpFS.read() << 8);

    if (depth == 24) {
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {
        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t* bptr = lineBuffer;
        for (col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          tft.drawPixel(x + col, y + h - 1 - row, tft.color565(r, g, b));
        }
      }
    } else {
      Serial.println("BMP format not supported.");
    }
  }
  bmpFS.close();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Démarrage du robot...");

  // Initialisation du TFT
  tft.init();
  tft.setRotation(1); // Format paysage (ajuster selon le besoin)
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Robot AI Booting...");

  // Initialisation de la carte SD
  if (!SD.begin(SD_CS)) {
    Serial.println("Erreur: Carte SD introuvable ou erreur SPI.");
    tft.println("Erreur SD!");
  } else {
    Serial.println("Carte SD initialisée.");
    tft.println("SD OK!");

    // Exemple : Afficher l'image par défaut "neutre.bmp" si elle existe
    // drawBmp("/neutre.bmp", 0, 0);
  }
}

void parseAndHandleJSON(String jsonString) {
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, jsonString);

  if (error) {
    Serial.print("Erreur de parsing JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Extraire l'émotion
  if (doc.containsKey("emotion")) {
    const char* emotion = doc["emotion"];
    String newEmotion = String(emotion);

    if (newEmotion != currentEmotion) {
      Serial.print("Nouvelle émotion reçue: ");
      Serial.println(newEmotion);
      currentEmotion = newEmotion;

      // Charger l'image correspondante (ex: "/joie.bmp")
      String bmpPath = "/" + currentEmotion + ".bmp";
      tft.fillScreen(TFT_BLACK); // Nettoyer l'écran avant d'afficher
      drawBmp(bmpPath.c_str(), 0, 0);
    }
  }

  // Afficher le texte généré (optionnel pour le debug)
  if (doc.containsKey("speech")) {
    const char* speech = doc["speech"];
    Serial.print("Speech: ");
    Serial.println(speech);
  }
}

void loop() {
  // --- Simulation de réception JSON ---
  // Dans le code final, ce JSON viendra du WebSocket.
  // Ici on simule un changement d'émotion toutes les 5 secondes pour tester le parsing et l'affichage.

  static unsigned long lastUpdate = 0;
  static int state = 0;

  if (millis() - lastUpdate > 5000) {
    lastUpdate = millis();
    String fakeJson = "";

    if (state == 0) {
      fakeJson = "{\"speech\": \"Bonjour je suis content.\", \"emotion\": \"joie\"}";
      state = 1;
    } else if (state == 1) {
      fakeJson = "{\"speech\": \"Je ne sais pas.\", \"emotion\": \"neutre\"}";
      state = 2;
    } else {
      fakeJson = "{\"speech\": \"Oh non c'est dommage.\", \"emotion\": \"triste\"}";
      state = 0;
    }

    Serial.println("\n--- Simulation réception WebSocket ---");
    Serial.println(fakeJson);
    parseAndHandleJSON(fakeJson);
  }
}
