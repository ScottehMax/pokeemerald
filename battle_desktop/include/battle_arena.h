#ifndef GUARD_BATTLE_ARENA_H
#define GUARD_BATTLE_ARENA_H

#include "constants/battle_arena.h"

static inline void CallBattleArenaFunction(void) {}
static inline u8 BattleArena_ShowJudgmentWindow(u8 *state) { return 0; }
static inline void BattleArena_InitPoints(void) {}
static inline void BattleArena_AddMindPoints(u8 battler) {}
static inline void BattleArena_AddSkillPoints(u8 battler) {}
static inline void BattleArena_DeductSkillPoints(u8 battler, u16 stringId) {}
static inline void DrawArenaRefereeTextBox(void) {}
static inline void EraseArenaRefereeTextBox(void) {}

#endif /* GUARD_BATTLE_ARENA_H */
