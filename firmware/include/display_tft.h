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
    void resetBaseDrawn();

private:
    TFT_eSPI tft;
    bool baseDrawn;
    uint16_t bgColor;
    void drawBmp(const char *filename, int16_t x, int16_t y);
    void clearFaceArea();
};

#endif // TFT_DISPLAY_H
