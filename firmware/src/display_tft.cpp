#include "display_tft.h"

TFT_Display::TFT_Display() : tft(TFT_eSPI()) {}

void TFT_Display::init() {
  // Initialisation du TFT
  tft.init();
  tft.setRotation(1); // Format paysage
  tft.fillScreen(TFT_BLACK);

  // Calibration touch data based on audit: {282, 3617, 336, 3458, 4}
  uint16_t calData[5] = {282, 3617, 336, 3458, 4};
  tft.setTouch(calData);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Robot AI Booting...");

  delay(500); // Give it a moment to display booting
}

void TFT_Display::displayEmotion(String emotion, int frame) {
  // Vector engine rendering for emotions
  tft.fillScreen(TFT_BLACK); // Nettoyer l'écran avant d'afficher

  int eyeWidth = 40;
  int eyeHeight = 60;
  int mouthWidth = 100;
  int mouthHeight = 10;
  int mouthOffset = 20;

  if (emotion == "joie") {
    eyeHeight = 40; // Squinted eyes
    mouthHeight = 30; // Open mouth
    mouthOffset = 10;
  } else if (emotion == "triste") {
    eyeHeight = 50; // Droopy eyes
    mouthHeight = 10; // Flat or frown
    mouthOffset = 30;
  } else if (emotion == "parle") {
    // Animate mouth based on frame
    mouthHeight = (frame % 2 == 0) ? 40 : 10;
  } else if (emotion == "reflexion") {
    // Animate eyes based on frame
    eyeHeight = (frame % 2 == 0) ? 60 : 20; // Blinking
  } else {
    // Neutre
    eyeHeight = 60;
    mouthHeight = 10;
  }

  drawFace(eyeWidth, eyeHeight, mouthWidth, mouthHeight, mouthOffset);
}

void TFT_Display::drawFace(int eyeWidth, int eyeHeight, int mouthWidth, int mouthHeight, int mouthOffset) {
  int centerX = tft.width() / 2;
  int centerY = tft.height() / 2;

  // Draw left eye
  tft.fillSmoothRoundRect(centerX - 80 - eyeWidth/2, centerY - 40 - eyeHeight/2, eyeWidth, eyeHeight, 10, TFT_WHITE, TFT_BLACK);

  // Draw right eye
  tft.fillSmoothRoundRect(centerX + 80 - eyeWidth/2, centerY - 40 - eyeHeight/2, eyeWidth, eyeHeight, 10, TFT_WHITE, TFT_BLACK);

  // Draw mouth
  tft.fillSmoothRoundRect(centerX - mouthWidth/2, centerY + 60 + mouthOffset - mouthHeight/2, mouthWidth, mouthHeight, 5, TFT_WHITE, TFT_BLACK);
}
