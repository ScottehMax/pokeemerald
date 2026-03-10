#ifndef GUARD_SCANLINE_EFFECT_H
#define GUARD_SCANLINE_EFFECT_H
#include "gba/types.h"

/* Stub constants — DMA control values unused on desktop */
#define SCANLINE_EFFECT_DMACNT_16BIT  0
#define SCANLINE_EFFECT_DMACNT_32BIT  0
#define SCANLINE_EFFECT_REG_BG3HOFS   6

struct ScanlineEffectParams {
    volatile void *dmaDest;
    u32 dmaControl;
    u8 initState;
    u8 unused9;
};

struct ScanlineEffect {
    void *dmaSrcBuffers[2];
    volatile void *dmaDest;
    u32 dmaControl;
    void (*setFirstScanlineReg)(void);
    u8 srcBuffer;
    u8 state;
    u8 unused16;
    u8 unused17;
    u8 waveTaskId;
};

extern struct ScanlineEffect gScanlineEffect;
extern u16 gScanlineEffectRegBuffers[2][160];

static inline void ScanlineEffect_Clear(void) {}
static inline void ScanlineEffect_Stop(void) {}
static inline void ScanlineEffect_InitHBlankDmaTransfer(void) {}
static inline void ScanlineEffect_SetParams(struct ScanlineEffectParams params) {}
static inline u8 ScanlineEffect_InitWave(u8 a, u8 b, u8 c, u8 d, u8 e, u8 f, bool8 g) { return 0; }
#endif
