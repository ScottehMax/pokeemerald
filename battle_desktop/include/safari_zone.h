#ifndef GUARD_SAFARI_ZONE_H
#define GUARD_SAFARI_ZONE_H
#include "gba/types.h"
extern u8 gNumSafariBalls;
static inline u16 GetSafariZoneBallCount(void) { return 0; }
static inline void SetSafariZoneBallCount(u16 count) {}
static inline bool8 IsPlayerInSafariZone(void) { return FALSE; }
#endif
