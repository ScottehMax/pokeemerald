#ifndef GUARD_BATTLE_SETUP_H
#define GUARD_BATTLE_SETUP_H

#include "gba/types.h"

#define REMATCHES_COUNT 5
#define REMATCH_TABLE_ENTRIES 70

struct RematchTrainer { u16 trainerIds[REMATCHES_COUNT]; u16 mapGroup; u16 mapNum; };
extern const struct RematchTrainer gRematchTable[];
extern u16 gTrainerBattleOpponent_A;
extern u16 gTrainerBattleOpponent_B;
extern u16 gPartnerTrainerId;

static inline u8 BattleSetup_GetEnvironmentId(void) { return 0; }
static inline u8 GetSpecialBattleTransition(s32 id) { return 0; }
static inline bool8 GetTrainerFlag(void) { return FALSE; }
static inline bool8 HasTrainerBeenFought(u16 trainerId) { return FALSE; }
static inline void SetTrainerFlag(u16 trainerId) {}
static inline const u8 *GetTrainerALoseText(void) { return NULL; }
static inline const u8 *GetTrainerBLoseText(void) { return NULL; }
static inline const u8 *GetTrainerWonSpeech(void) { return NULL; }
static inline void UpdateRematchIfDefeated(s32 rematchTableId) {}
static inline void PlayTrainerEncounterMusic(void) {}
static inline void ResetTrainerOpponentIds(void) {}
static inline void BattleSetup_StartTrainerBattle(void) {}
static inline void BattleSetup_StartWildBattle(void) {}
static inline const u8 *BattleSetup_GetScriptAddrAfterBattle(void) { return NULL; }
static inline const u8 *BattleSetup_GetTrainerPostBattleScript(void) { return NULL; }
static inline void ShowTrainerIntroSpeech(void) {}
static inline void ShowTrainerCantBattleSpeech(void) {}

#endif // GUARD_BATTLE_SETUP_H
