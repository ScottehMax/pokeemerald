#ifndef GUARD_RANDOM_H
#define GUARD_RANDOM_H

#include "gba/types.h"

/* Linear congruential RNG matching GBA implementation */
extern u32 gRngValue;
extern u32 gRng2Value;

u16 Random(void);
u16 Random2(void);
void SeedRng(u16 seed);
void SeedRng2(u16 seed);

#define RandomPercent(n)    ((Random() & 0x3FF) < (u32)((n) * 1024 / 100))
#define Random32()          ((u32)(Random()) | ((u32)(Random()) << 16))
#define ISO_RANDOMIZE1(val) (1103515245 * (val) + 24691)

#endif // GUARD_RANDOM_H
