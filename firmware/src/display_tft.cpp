#include "display_tft.h"

TFT_Display::TFT_Display() : tft(TFT_eSPI()) {}

void TFT_Display::init() {
  // Initialisation du TFT
  tft.init();
  tft.setRotation(1); // Format paysage
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
  }
}

void TFT_Display::displayEmotion(String emotion, int frame) {
  // Construit le chemin avec le numéro de frame (ex: "/joie_1.bmp")
  String bmpPath = "/" + emotion + "_" + String(frame) + ".bmp";
  tft.fillScreen(TFT_BLACK); // Nettoyer l'écran avant d'afficher
  drawBmp(bmpPath.c_str(), 0, 0);
}

void TFT_Display::drawBmp(const char *filename, int16_t x, int16_t y) {
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
      // Utilisation de malloc plutôt qu'un VLA pour éviter de surcharger la stack FreeRTOS
      uint8_t* lineBuffer = (uint8_t*)malloc(w * 3 + padding);

      if (lineBuffer != NULL) {
        for (row = 0; row < h; row++) {
          bmpFS.read(lineBuffer, w * 3 + padding);
          uint8_t* bptr = lineBuffer;
          for (col = 0; col < w; col++) {
            b = *bptr++;
            g = *bptr++;
            r = *bptr++;
            tft.drawPixel(x + col, y + h - 1 - row, tft.color565(r, g, b));
          }
        }
        free(lineBuffer);
      } else {
        Serial.println("Erreur d'allocation mémoire pour le buffer BMP");
      }
    } else {
      Serial.println("BMP format not supported.");
    }
  }
  bmpFS.close();
}
