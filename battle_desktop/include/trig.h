#ifndef GUARD_TRIG_H
#define GUARD_TRIG_H
#include "gba/types.h"
#include <math.h>
extern const s16 gSineTable[];
static inline s16 Sin(s16 index, s16 amplitude) { return (s16)(sinf(index * 3.14159265f / 128.0f) * amplitude); }
static inline s16 Cos(s16 index, s16 amplitude) { return (s16)(cosf(index * 3.14159265f / 128.0f) * amplitude); }
static inline s32 Sin2(u16 index) { return (s32)(sinf(index * 3.14159265f / 32768.0f) * 4096.0f); }
static inline s32 Cos2(u16 index) { return (s32)(cosf(index * 3.14159265f / 32768.0f) * 4096.0f); }
#endif
