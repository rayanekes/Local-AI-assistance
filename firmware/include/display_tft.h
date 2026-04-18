#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <SD.h>

#define SD_CS 5

class TFT_Display {
public:
    TFT_Display();
    void init();
    void displayEmotion(String emotion, int frame);
    TFT_eSPI tft; // Make tft public so main can access it for LVGL

private:
    void drawFace(int eyeWidth, int eyeHeight, int mouthWidth, int mouthHeight, int mouthOffset);
};

#endif // TFT_DISPLAY_H
