with open("firmware/include/display_tft.h", "r") as f:
    content = f.read()

content = content.replace("private:", "private:\n    bool baseDrawn = false;\n    uint16_t bgColor = 0;\n")

with open("firmware/include/display_tft.h", "w") as f:
    f.write(content)

display_cpp = """#include "display_tft.h"

TFT_Display::TFT_Display() : tft(TFT_eSPI()) {}

void TFT_Display::init() {
  // Initialisation du TFT
  tft.init();
  tft.setRotation(1); // Format paysage
  bgColor = tft.color565(20, 20, 30);
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
  if (!baseDrawn) {
      tft.fillScreen(bgColor);
      drawBmp("/base.bmp", 0, 0);
      baseDrawn = true;
  }

  // Effacer la zone des yeux et de la bouche (au lieu de redessiner tout le BMP)
  // tft.fillRect(x, y, w, h, bgColor);
  tft.fillRect(40, 40, 240, 180, bgColor);

  uint16_t main_color = TFT_CYAN;
  int eye_w = 40;
  int eye_h = 60;
  int offset_y = 0;

  if (emotion == "neutre") {
      main_color = tft.color565(0, 255, 255); // Cyan
      eye_w = 40; eye_h = 60;
  } else if (emotion == "joie") {
      main_color = tft.color565(255, 200, 0); // Jaune vif
      eye_w = 45; eye_h = 45;
  } else if (emotion == "triste") {
      main_color = tft.color565(100, 150, 255); // Bleu triste
      eye_w = 40; eye_h = 50;
  } else if (emotion == "parle") {
      main_color = tft.color565(0, 255, 100); // Vert menthe
      eye_w = 40; eye_h = 60;
  } else if (emotion == "reflexion") {
      main_color = tft.color565(255, 50, 255); // Magenta
      eye_w = 50; eye_h = 50;
  }

  if (frame == 2 && emotion != "reflexion") {
      eye_h = 10;
      offset_y = 25;
  }

  // JOUES ROSES
  if (frame == 1 && (emotion == "joie" || emotion == "parle" || emotion == "neutre")) {
      tft.fillEllipse(65, 125, 15, 15, tft.color565(255, 100, 150));
      tft.fillEllipse(255, 125, 15, 15, tft.color565(255, 100, 150));
  }

  if (emotion == "neutre") {
      tft.fillRoundRect(80, 60 + offset_y, eye_w, eye_h, 10, main_color);
      tft.fillRoundRect(200, 60 + offset_y, eye_w, eye_h, 10, main_color);
      tft.fillRect(100, 174, 120, 12, main_color);
  } else if (emotion == "joie") {
      if (frame == 2) {
          tft.fillRect(70, 75, 60, 10, main_color);
          tft.fillRect(190, 75, 60, 10, main_color);
      } else {
          tft.fillEllipse(100, 80, 30, 30, main_color);
          tft.fillEllipse(100, 85, 30, 30, bgColor);
          tft.fillEllipse(220, 80, 30, 30, main_color);
          tft.fillEllipse(220, 85, 30, 30, bgColor);
      }
      tft.fillEllipse(160, 160, 60, 40, main_color);
      tft.fillEllipse(160, 150, 60, 40, bgColor);
  } else if (emotion == "triste") {
      tft.fillRoundRect(80, 60 + offset_y, eye_w, eye_h, 5, main_color);
      tft.fillRoundRect(200, 60 + offset_y, eye_w, eye_h, 5, main_color);
      if (frame == 2) {
          tft.fillEllipse(100, 140, 10, 10, tft.color565(50, 200, 255));
          tft.fillEllipse(220, 140, 10, 10, tft.color565(50, 200, 255));
      }
      tft.fillEllipse(160, 195, 60, 35, main_color);
      tft.fillEllipse(160, 205, 60, 35, bgColor);
  } else if (emotion == "parle") {
      tft.fillRoundRect(80, 60 + offset_y, eye_w, eye_h, 10, main_color);
      tft.fillRoundRect(200, 60 + offset_y, eye_w, eye_h, 10, main_color);
      int mouth_h = (frame == 1) ? 10 : 50;
      tft.fillEllipse(160, 160 + mouth_h/2, 40, mouth_h/2, tft.color565(255, 50, 50));
  } else if (emotion == "reflexion") {
      tft.fillEllipse(100, 80, 30, 30, main_color);
      tft.fillRect(200, 75, 50, 10, main_color);

      tft.fillRect(100, 185, 60, 10, main_color);
      tft.drawLine(160, 190, 220, 170, main_color);

      if (frame == 2) {
          tft.setTextColor(tft.color565(255, 255, 0));
          tft.drawString("?", 250, 20, 4);
          tft.fillEllipse(57, 37, 7, 7, TFT_WHITE);
      } else {
          tft.setTextColor(tft.color565(255, 200, 0));
          tft.drawString("?", 250, 10, 4);
          tft.fillEllipse(35, 55, 5, 5, tft.color565(200, 200, 200));
      }
  }
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
"""

with open("firmware/src/display_tft.cpp", "w") as f:
    f.write(display_cpp)
