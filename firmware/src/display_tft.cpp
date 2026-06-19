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
    // Draw the static background once
    drawBmp("/base.bmp", 0, 0);
  }
}

void TFT_Display::displayEmotion(String emotion, int frame) {
  // Use vector graphics to draw the face instead of loading a BMP
  // Clean the facial region only (background color 20, 20, 30)
  tft.fillRect(40, 40, 240, 200, tft.color565(20, 20, 30));

  uint16_t mainColor;
  int eyeW, eyeH;
  int offsetY = 0;

  if (emotion == "neutre") {
    mainColor = tft.color565(0, 255, 255); // Cyan
    eyeW = 40; eyeH = 60;
  } else if (emotion == "joie") {
    mainColor = tft.color565(255, 200, 0); // Jaune vif
    eyeW = 45; eyeH = 45;
  } else if (emotion == "triste") {
    mainColor = tft.color565(100, 150, 255); // Bleu triste
    eyeW = 40; eyeH = 50;
  } else if (emotion == "parle") {
    mainColor = tft.color565(0, 255, 100); // Vert menthe
    eyeW = 40; eyeH = 60;
  } else if (emotion == "reflexion") {
    mainColor = tft.color565(255, 50, 255); // Magenta
    eyeW = 50; eyeH = 50;
  } else {
    // Default
    mainColor = tft.color565(0, 255, 255);
    eyeW = 40; eyeH = 60;
  }

  if (frame == 2 && emotion != "reflexion") {
    eyeH = 10;
    offsetY = 25;
  }

  // Draw cheeks
  if (frame == 1 && (emotion == "joie" || emotion == "parle" || emotion == "neutre")) {
    tft.fillEllipse(65, 125, 15, 15, tft.color565(255, 100, 150));
    tft.fillEllipse(255, 125, 15, 15, tft.color565(255, 100, 150));
  }

  // Draw eyes and mouth
  if (emotion == "neutre") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    // Mouth
    tft.fillRect(100, 174, 120, 12, mainColor); // Line width 12 -> rect height 12
  } else if (emotion == "joie") {
    if (frame == 2) {
      tft.fillRect(70, 72, 60, 15, mainColor);
      tft.fillRect(190, 72, 60, 15, mainColor);
    } else {
      // Arc simulation (simplified) for eyes - using filled circles and erasing half
      tft.fillCircle(100, 80, 30, mainColor);
      tft.fillCircle(100, 80 + 15, 30, tft.color565(20, 20, 30)); // Erase lower part
      tft.fillCircle(220, 80, 30, mainColor);
      tft.fillCircle(220, 80 + 15, 30, tft.color565(20, 20, 30));
    }
    // Grosse bouche souriante
    tft.fillCircle(160, 160, 60, mainColor);
    tft.fillCircle(160, 160 - 20, 60, tft.color565(20, 20, 30)); // Erase upper part
  } else if (emotion == "triste") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 5, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 5, mainColor);
    if (frame == 2) {
      // Larmes
      tft.fillEllipse(100, 140, 10, 10, tft.color565(50, 200, 255));
      tft.fillEllipse(220, 140, 10, 10, tft.color565(50, 200, 255));
    }
    // Sad mouth
    tft.fillCircle(160, 195, 60, mainColor);
    tft.fillCircle(160, 195 + 15, 60, tft.color565(20, 20, 30));
  } else if (emotion == "parle") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    // Bouche
    int mouthH = (frame == 1) ? 10 : 50;
    tft.fillEllipse(160, 160 + mouthH/2, 40, mouthH/2, tft.color565(255, 50, 50));
  } else if (emotion == "reflexion") {
    // Yeux
    tft.fillEllipse(100, 80, 30, 30, mainColor);
    tft.fillRect(200, 72, 50, 15, mainColor);

    // Bouche tordue
    // Simplified twisted mouth
    tft.drawLine(100, 190, 160, 190, mainColor);
    tft.drawLine(160, 190, 220, 170, mainColor);
    // thicker lines by drawing adjacent lines
    for(int i=-4; i<=5; i++) {
        tft.drawLine(100, 190+i, 160, 190+i, mainColor);
        tft.drawLine(160, 190+i, 220, 170+i, mainColor);
    }

    if (frame == 2) {
      tft.setTextSize(5);
      tft.setTextColor(tft.color565(255, 255, 0));
      tft.setCursor(250, 20);
      tft.print("?");
      tft.fillEllipse(57, 37, 7, 7, TFT_WHITE);
    } else {
      tft.setTextSize(4);
      tft.setTextColor(tft.color565(255, 200, 0));
      tft.setCursor(250, 10);
      tft.print("?");
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
