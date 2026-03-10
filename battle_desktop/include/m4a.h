#ifndef GUARD_M4A_H
#define GUARD_M4A_H

#include "gba/types.h"

#define TRACKS_ALL 0xFFFF
struct MusicPlayerInfo { u32 status; };

typedef struct MusicPlayerInfo MPlayInfo_t;
extern MPlayInfo_t gMPlayInfo_BGM;
extern MPlayInfo_t gMPlayInfo_SE1;
extern MPlayInfo_t gMPlayInfo_SE2;
extern MPlayInfo_t gMPlayInfo_SE3;

struct SoundInfo { u32 unused[16]; };

static inline void m4aSoundInit(void) {}
static inline void m4aSoundMain(void) {}
static inline void m4aSoundVSync(void) {}
static inline void m4aSongNumStart(u16 n) {}
static inline void m4aSongNumStop(u16 n) {}
static inline void m4aSongNumContinue(u16 n) {}
static inline void m4aMPlayStart(MPlayInfo_t *info, void *song) {}
static inline void m4aMPlayStop(MPlayInfo_t *info) {}
static inline void m4aMPlayContinue(MPlayInfo_t *info) {}
static inline void m4aMPlayFadeOut(MPlayInfo_t *info, u16 speed) {}
static inline void m4aMPlayFadeIn(MPlayInfo_t *info, u16 speed, void *song) {}
static inline void m4aMPlayImmInit(MPlayInfo_t *info) {}
static inline void m4aTrackStop(MPlayInfo_t *info, u16 trackBits) {}
static inline void m4aMPlayTempoControl(MPlayInfo_t *info, u16 tempo) {}
static inline void m4aMPlayVolumeControl(MPlayInfo_t *info, u16 trackBits, u16 volume) {}
static inline void m4aMPlayPitchControl(MPlayInfo_t *info, u16 trackBits, s16 pitch) {}
static inline void m4aMPlayPanpotControl(MPlayInfo_t *info, u16 trackBits, s8 pan) {}
static inline void m4aMPlayModDepthSet(MPlayInfo_t *info, u16 trackBits, u8 modDepth) {}
static inline void m4aMPlayLFOSpeedSet(MPlayInfo_t *info, u16 trackBits, u8 lfoSpeed) {}

#endif // GUARD_M4A_H
