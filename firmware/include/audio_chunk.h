#ifndef AUDIO_CHUNK_H
#define AUDIO_CHUNK_H

#include <Arduino.h>

#define MIC_BUFFER_COUNT 10
#define MIC_BUFFER_SIZE 1024

#define SPK_BUFFER_COUNT 10
#define SPK_BUFFER_SIZE 1024

struct AudioChunk {
    uint8_t* data;
    size_t length;
};

#endif
