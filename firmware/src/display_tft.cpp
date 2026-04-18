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
  // Ne pas nettoyer l'écran pour éviter le scintillement (flickering)
  drawBmp(bmpPath.c_str(), 0, 0);
}

void TFT_Display::drawVoiceVisualizer(int micLevel, int speakerLevel, bool isSpeaking) {
  // Configurer la zone d'affichage du visualiseur (en bas de l'écran)
  int yOffset = tft.height() - 20;
  int barHeight = 10;
  int maxWidth = tft.width() - 40;

  int w = 0;
  uint16_t color = TFT_BLACK;

  if (isSpeaking) {
    w = map(speakerLevel, 0, 100, 0, maxWidth);
    color = TFT_BLUE;
  } else {
    w = map(micLevel, 0, 100, 0, maxWidth);
    color = TFT_GREEN;
  }

  if (w > maxWidth) w = maxWidth;

  // Dessiner la partie colorée
  tft.fillRect(20, yOffset, w, barHeight, color);

  // Dessiner la partie restante en noir (pour effacer sans scintillement)
  if (w < maxWidth) {
    tft.fillRect(20 + w, yOffset, maxWidth - w, barHeight, TFT_BLACK);
  }

  // Dessiner un contour
  tft.drawRect(20, yOffset, maxWidth, barHeight, TFT_WHITE);
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

      // Utiliser des blocs de lignes pour optimiser la lecture SD et les écritures SPI
      const uint8_t chunkLines = 8; // On lit 8 lignes à la fois si la mémoire le permet
      uint32_t lineBytes = w * 3 + padding;
      uint32_t chunkBytes = lineBytes * chunkLines;

      uint8_t* lineBuffer = (uint8_t*)malloc(chunkBytes);
      uint16_t* colorBuffer = (uint16_t*)malloc(w * chunkLines * sizeof(uint16_t));

      if (lineBuffer != NULL && colorBuffer != NULL) {
        // En mode swap_bytes si nécessaire, pushImage peut être plus rapide
        tft.setWindow(x, y, x + w - 1, y + h - 1);
        tft.setSwapBytes(true);

        for (row = 0; row < h; row += chunkLines) {
          uint8_t currentChunkLines = (row + chunkLines > h) ? (h - row) : chunkLines;
          bmpFS.read(lineBuffer, lineBytes * currentChunkLines);

          uint8_t* bptr = lineBuffer;
          uint16_t* cptr = colorBuffer;

          // BMP est lu du bas vers le haut
          for (uint8_t l = 0; l < currentChunkLines; l++) {
            for (col = 0; col < w; col++) {
              b = *bptr++;
              g = *bptr++;
              r = *bptr++;
              *cptr++ = tft.color565(r, g, b);
            }
            // Skip padding at the end of the line
            bptr += padding;
          }

          // pushImage peut pousser un bloc, mais BMP a les lignes inversées.
          // On doit l'écrire de bas en haut ligne par ligne pour éviter de complexifier le buffer.
          for (uint8_t l = 0; l < currentChunkLines; l++) {
            tft.pushImage(x, y + h - 1 - (row + l), w, 1, &colorBuffer[l * w]);
          }
        }
        tft.setSwapBytes(false); // Restore
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
