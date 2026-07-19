#ifndef GUARD_PC_SHARED_H
#define GUARD_PC_SHARED_H

#include <stdint.h>

#define PC_SHARED_MAGIC 0x50454D45u
#define PC_FRAME_WIDTH 240
#define PC_FRAME_HEIGHT 160
#define PC_FRAME_BUFFER_COUNT 3
#define PC_PATH_MAX 1024
#define PC_AUDIO_RATE 48000
#define PC_AUDIO_BUFFER_FRAMES 32768
#define PC_CORE_EXIT_SOFT_RESET 100

struct PcSharedState
{
    uint32_t magic;
    uint32_t quit;
    uint32_t keys;
    uint32_t frameSequence;
    uint32_t frameBufferIndex;
    uint32_t coreReady;
    uint32_t coreError;
    char savePath[PC_PATH_MAX];
    uint32_t audioRead;
    uint32_t audioWrite;
    uint32_t audioFramesGenerated;
    uint32_t audioPeak;
    uint32_t audioSamplesNonzero;
    uint32_t audioSamplesClipped;
    uint32_t testBattleState;
    uint32_t testBattleOutcome;
    int16_t audio[PC_AUDIO_BUFFER_FRAMES * 2];
    uint32_t pixels[PC_FRAME_BUFFER_COUNT][PC_FRAME_WIDTH * PC_FRAME_HEIGHT];
};

#endif
