#ifndef AUDIO_CHUNK_H
#define AUDIO_CHUNK_H

#include <Arduino.h>

#define NUM_MIC_BUFFERS 10
#define MIC_BUFFER_SIZE 1024

#define NUM_SPK_BUFFERS 10
#define SPK_BUFFER_SIZE 4096

extern uint8_t micBuffers[NUM_MIC_BUFFERS][MIC_BUFFER_SIZE];
extern uint8_t spkBuffers[NUM_SPK_BUFFERS][SPK_BUFFER_SIZE];

extern QueueHandle_t micFreeQueue;
extern QueueHandle_t spkFreeQueue;

struct AudioChunk {
    uint8_t* data;
    size_t length;
};

#endif
