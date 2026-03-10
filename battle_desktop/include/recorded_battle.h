#ifndef GUARD_RECORDED_BATTLE_H
#define GUARD_RECORDED_BATTLE_H
#include "gba/types.h"
#define B_RECORD_MODE_RECORDING 0
#define B_RECORD_MODE_PLAYBACK  1
extern u32 gRecordedBattleRngSeed;
extern u32 gBattlePalaceMoveSelectionRngValue;
extern u8 gRecordedBattleMultiplayerId;
static inline void RecordedBattle_Init(u8 mode) {}
static inline void RecordedBattle_SaveParties(void) {}
static inline void RecordedBattle_ClearFrontierPassFlag(void) {}
static inline bool8 RecordedBattle_CanStopPlayback(void) { return FALSE; }
static inline bool8 GetBattleSceneInRecordedBattle(void) { return FALSE; }
static inline void RecordedBattle_SetActionByBattler(u8 battler, u8 action) {}
static inline void RecordedBattle_RecordAllBattlerData(u8 *data) {}
static inline bool8 RecordedBattle_IsRecordingBattle(void) { return FALSE; }
#endif
