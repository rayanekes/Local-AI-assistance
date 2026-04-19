#include "display_tft.h"

TFT_Display::TFT_Display() : tft(TFT_eSPI()), baseDrawn(false), bgColor(tft.color565(20, 20, 30)) {}

void TFT_Display::init() {
  baseDrawn = false;
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

void TFT_Display::resetBaseDrawn() {
  baseDrawn = false;
}

void TFT_Display::clearFaceArea() {
  // Clears the area where eyes and mouth are drawn, instead of redrawing the whole background
  tft.fillRect(40, 40, 240, 195, bgColor);
}

void TFT_Display::displayEmotion(String emotion, int frame) {
  if (!baseDrawn) {
    tft.fillScreen(bgColor); // Set background color to dark bluish gray
    drawBmp("/base.bmp", 0, 0); // Load base background if it exists
    baseDrawn = true;
  }

  // Double flash blink logic (Micro-seconde clignotement noir)
  // We trigger this for frame == 2 of the neutral state (or any state that has blinking)
  if (frame == 2 && emotion != "reflexion") {
    clearFaceArea();
    // Flash 1
    tft.fillRect(80, 85, 40, 10, TFT_BLACK);
    tft.fillRect(200, 85, 40, 10, TFT_BLACK);
    delay(30);
    clearFaceArea();
    delay(20);
    // Flash 2
    tft.fillRect(80, 85, 40, 10, TFT_BLACK);
    tft.fillRect(200, 85, 40, 10, TFT_BLACK);
    delay(30);
    // Erase blink to proceed with the normal frame drawing
    clearFaceArea();
  } else {
    // Standard clear before drawing vector graphics
    clearFaceArea();
  }

  uint16_t mainColor = TFT_WHITE;
  int eyeW = 40, eyeH = 60;

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
  }

  int offsetY = 0;
  if (frame == 2 && emotion != "reflexion") {
    eyeH = 10;
    offsetY = 25;
  }

  if (frame == 3 && emotion != "reflexion") {
    // Frame 3 variation (eyes slightly closed or different state)
    eyeH = 40;
    offsetY = 10;
  }

  // --- Joues roses ---
  if (frame == 1 && (emotion == "joie" || emotion == "parle" || emotion == "neutre")) {
    tft.fillEllipse(65, 125, 15, 15, tft.color565(255, 100, 150));
    tft.fillEllipse(255, 125, 15, 15, tft.color565(255, 100, 150));
  }

  // --- Dessin des yeux et bouche ---
  if (emotion == "neutre") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    tft.fillRect(100, 180, 120, 12, mainColor); // Ligne droite
  }
  else if (emotion == "joie") {
    if (frame == 2) {
      tft.fillRect(70, 80, 60, 15, mainColor);
      tft.fillRect(190, 80, 60, 15, mainColor);
    } else {
      // Arcs for eyes (approximated with filled semicircles/rectangles)
      tft.fillEllipse(100, 80, 30, 25, mainColor);
      tft.fillRect(70, 80, 60, 30, bgColor); // Cut bottom half
      tft.fillEllipse(220, 80, 30, 25, mainColor);
      tft.fillRect(190, 80, 60, 30, bgColor); // Cut bottom half
    }
    // Grosse bouche souriante
    tft.fillEllipse(160, 160, 60, 40, mainColor);
    tft.fillRect(100, 120, 120, 40, bgColor); // Cut top half
  }
  else if (emotion == "triste") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 5, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 5, mainColor);
    if (frame == 2 || frame == 3) {
      // Larmes animées
      int tearY = (frame == 3) ? 145 : 130;
      tft.fillEllipse(100, tearY, 10, 10, tft.color565(50, 200, 255));
      tft.fillEllipse(220, tearY, 10, 10, tft.color565(50, 200, 255));
    }
    // Bouche triste
    tft.fillEllipse(160, 195, 60, 35, mainColor);
    tft.fillRect(100, 195, 120, 40, bgColor); // Cut bottom half
  }
  else if (emotion == "parle") {
    tft.fillRoundRect(80, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    tft.fillRoundRect(200, 60 + offsetY, eyeW, eyeH, 10, mainColor);
    // Bouche qui s'anime
    int mouthH = (frame == 1) ? 10 : ((frame == 2) ? 30 : 50);
    tft.fillEllipse(160, 160 + mouthH/2, 40, mouthH/2, tft.color565(255, 50, 50));
  }
  else if (emotion == "reflexion") {
    // Yeux asymétriques
    tft.fillEllipse(100, 80, 30, 30, mainColor);
    tft.fillRect(200, 75, 50, 15, mainColor);

    // Bouche tordue
    tft.drawLine(100, 190, 160, 190, mainColor);
    tft.drawLine(101, 191, 161, 191, mainColor);
    tft.drawLine(102, 192, 162, 192, mainColor);
    tft.drawLine(160, 190, 220, 170, mainColor);
    tft.drawLine(161, 191, 221, 171, mainColor);
    tft.drawLine(162, 192, 222, 172, mainColor);

    if (frame == 2) {
      tft.fillEllipse(57, 37, 7, 7, TFT_WHITE);
      tft.setTextColor(tft.color565(255, 255, 0));
      tft.setTextSize(4);
      tft.setCursor(250, 20);
      tft.print("?");
    } else if (frame == 3) {
      tft.fillEllipse(70, 25, 10, 10, TFT_WHITE);
      tft.setTextColor(tft.color565(255, 150, 0));
      tft.setTextSize(5);
      tft.setCursor(255, 15);
      tft.print("?");
    } else {
      tft.fillEllipse(35, 55, 5, 5, tft.color565(200, 200, 200));
      tft.setTextColor(tft.color565(255, 200, 0));
      tft.setTextSize(3);
      tft.setCursor(250, 10);
      tft.print("?");
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
