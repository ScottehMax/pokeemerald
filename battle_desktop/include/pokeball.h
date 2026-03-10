#ifndef GUARD_POKEBALL_H
#define GUARD_POKEBALL_H
#include "gba/types.h"
#define POKEBALL_COUNT 12
static inline u8 GetBallId(u16 itemId) { return 0; }
static inline void GetBallOamData(u8 *dest, u8 ballId) {}
static inline void DoPokeballThrowAnim(u8 ballId) {}
#endif
