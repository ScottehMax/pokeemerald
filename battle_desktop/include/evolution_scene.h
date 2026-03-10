#ifndef GUARD_EVOLUTION_SCENE_H
#define GUARD_EVOLUTION_SCENE_H
#include "gba/types.h"
static inline void EvolutionScene(void *mon, u16 species, bool8 canStopEvo, u8 partyId) {}
static inline void TradeEvolutionScene(void *mon, u16 species, u8 spriteId, u8 partyId) {}
extern void (*gCB2_AfterEvolution)(void);
#endif
