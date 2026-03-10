#ifndef GUARD_BATTLE_PYRAMID_H
#define GUARD_BATTLE_PYRAMID_H

#include "constants/battle_pyramid.h"

static inline void CallBattlePyramidFunction(void) {}
static inline u16 LocalIdToPyramidTrainerId(u8 localId) { return 0; }
static inline bool8 GetBattlePyramidTrainerFlag(u8 eventId) { return FALSE; }
static inline void MarkApproachingPyramidTrainersAsBattled(void) {}
static inline void GenerateBattlePyramidWildMon(void) {}
static inline u8 GetPyramidRunMultiplier(void) { return 1; }
static inline u8 CurrentBattlePyramidLocation(void) { return 0; }
static inline bool8 InBattlePyramid_(void) { return FALSE; }
static inline void PausePyramidChallenge(void) {}
static inline void SoftResetInBattlePyramid(void) {}
static inline void CopyPyramidTrainerSpeechBefore(u16 trainerId) {}
static inline void CopyPyramidTrainerWinSpeech(u16 trainerId) {}
static inline void CopyPyramidTrainerLoseSpeech(u16 trainerId) {}
static inline u8 GetTrainerEncounterMusicIdInBattlePyramid(u16 trainerId) { return 0; }
static inline void GenerateBattlePyramidFloorLayout(u16 *backupMapData, bool8 setPlayerPosition) {}
static inline void LoadBattlePyramidObjectEventTemplates(void) {}
static inline void LoadBattlePyramidFloorObjectEventScripts(void) {}
static inline u8 GetNumBattlePyramidObjectEvents(void) { return 0; }
static inline u16 GetBattlePyramidPickupItemId(void) { return 0; }

#endif /* GUARD_BATTLE_PYRAMID_H */
