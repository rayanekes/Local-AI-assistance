#include "display_tft.h"

TFT_Display::TFT_Display() : tft(TFT_eSPI()) {}

void TFT_Display::init() {
  // Reset matériel forcé (indispensable pour certains ST7789)
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  delay(100);
  digitalWrite(4, HIGH);
  delay(100);

  // Initialisation du TFT
  tft.init();
  tft.setRotation(1); 
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Robot IA en ligne !");
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

      // Allocation du buffer de lecture (SD) et du buffer de couleurs (TFT)
      uint8_t* lineBuffer = (uint8_t*)malloc(w * 3 + padding);
      uint16_t* colorBuffer = (uint16_t*)malloc(w * sizeof(uint16_t));

      if (lineBuffer != NULL && colorBuffer != NULL) {
        // Définir la zone de dessin pour utiliser pushColors (optimisation DMA/SPI)
        tft.setWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) {
          bmpFS.read(lineBuffer, w * 3 + padding);
          uint8_t* bptr = lineBuffer;

          for (col = 0; col < w; col++) {
            b = *bptr++;
            g = *bptr++;
            r = *bptr++;
            colorBuffer[col] = tft.color565(r, g, b);
          }
          // Pousser la ligne complète vers l'écran de bas en haut (format BMP)
          // Note : TFT_eSPI n'a pas de pushColors inversé pour Y, on utilise pushImage par ligne
          tft.pushImage(x, y + h - 1 - row, w, 1, colorBuffer);
        }
        free(lineBuffer);
        free(colorBuffer);
      } else {
        if (lineBuffer) free(lineBuffer);
        if (colorBuffer) free(colorBuffer);
        Serial.println("Erreur d'allocation mémoire pour le buffer BMP");
      }
    } else {
      Serial.println("BMP format not supported.");
    }
  }
  bmpFS.close();
}
