#ifndef GUARD_FRONTIER_UTIL_H
#define GUARD_FRONTIER_UTIL_H
#include "gba/types.h"
static inline bool8 IsFrontierTrainer(u16 trainerId) { return FALSE; }
static inline u16 GetFrontierTrainerMonIconSpecies(u16 trainerId, u8 monId) { return 0; }
static inline void SetFrontierBattlePartyIds(void) {}
static inline void HandleTrainerBattleLost(void) {}
#endif
