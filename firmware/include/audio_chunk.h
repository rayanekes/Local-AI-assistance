#ifndef AUDIO_CHUNK_H
#define AUDIO_CHUNK_H

#include <Arduino.h>

struct AudioChunk {
    uint8_t* data;
    size_t length;
};


extern uint8_t micBuffers[10][1024];
extern uint8_t spkBuffers[10][4096];

#endif
