#ifndef GUARD_BATTLE_INTERFACE_H
#define GUARD_BATTLE_INTERFACE_H
#include "gba/types.h"
#define HEALTH_BAR  0
#define EXP_BAR     1
#define HP_CURRENT  0
#define HP_MAX      1
extern u8 gHealthboxSpriteIds[];
extern u8 gBattlerSpriteIds[];
static inline u8 CreateHealthboxSprite(u8 battler, void *mon, u8 req, u8 status) { return 0; }
static inline void UpdateHealthboxAttribute(u8 spriteId, void *mon, u8 req) {}
static inline void SetHealthboxSpriteVisible(u8 spriteId) {}
static inline void SetHealthboxSpriteInvisible(u8 spriteId) {}
static inline void UpdateHpTextInHealthbox(u8 spriteId, u16 hp, u8 type) {}
static inline s16 MoveBattleBar(u8 battler, u8 spriteId, u8 whichBar, u8 arg3) { return -1; }
static inline void TryShinyAnimation(u8 battler, void *mon) {}
#endif
