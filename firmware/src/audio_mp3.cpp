#include "audio_mp3.h"

// Définitions des broches I2S pour le MAX98357A
#define I2S_DOUT 22
#define I2S_BCLK 27
#define I2S_LRC 26

AudioMP3::AudioMP3() : playing(false) {}

void AudioMP3::init() {
    audioI2S.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audioI2S.setVolume(15); // Volume de 0 à 21
}

void AudioMP3::play(const char* filepath) {
    audioI2S.connecttoFS(SD, filepath);
    playing = true;
    Serial.print("Lecture MP3 : ");
    Serial.println(filepath);
}

void AudioMP3::pause() {
    audioI2S.pauseResume();
    playing = !playing; // bascule d'état (simplifié)
}

void AudioMP3::stop() {
    audioI2S.stopSong();
    playing = false;
}

void AudioMP3::loop() {
    audioI2S.loop();
}

bool AudioMP3::isPlaying() {
    return audioI2S.isRunning();
}
