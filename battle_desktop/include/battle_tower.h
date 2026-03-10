#ifndef GUARD_BATTLE_TOWER_H
#define GUARD_BATTLE_TOWER_H

/* Desktop stub for battle_tower.h - frontier functions are no-ops */

#include "global.h"

struct BattleFrontierTrainer
{
    u8 facilityClass;
    u8 filler1[3];
    u8 trainerName[PLAYER_NAME_LENGTH + 1];
    u16 speechBefore[EASY_CHAT_BATTLE_WORDS_COUNT];
    u16 speechWin[EASY_CHAT_BATTLE_WORDS_COUNT];
    u16 speechLose[EASY_CHAT_BATTLE_WORDS_COUNT];
    const u16 *monSet;
};

struct FacilityMon
{
    u16 species;
    u16 moves[MAX_MON_MOVES];
    u8 itemTableId;
    u8 evSpread;
    u8 nature;
};

extern const u8 gTowerMaleFacilityClasses[30];
extern const u8 gTowerMaleTrainerGfxIds[30];
extern const u8 gTowerFemaleFacilityClasses[20];
extern const u8 gTowerFemaleTrainerGfxIds[20];
extern const u16 gBattleFrontierHeldItems[];
extern const struct FacilityMon gBattleFrontierMons[];
extern const struct BattleFrontierTrainer gBattleFrontierTrainers[];
extern const struct FacilityMon gSlateportBattleTentMons[];
extern const struct BattleFrontierTrainer gSlateportBattleTentTrainers[];
extern u16 gFrontierTempParty[];
extern const struct BattleFrontierTrainer *gFacilityTrainers;
extern const struct FacilityMon *gFacilityTrainerMons;

static inline void CallBattleTowerFunc(void) {}
static inline u16 GetRandomScaledFrontierTrainerId(u8 challengeNum, u8 battleNum) { return 0; }
static inline void SetBattleFacilityTrainerGfxId(u16 trainerId, u8 tempVarId) {}
static inline void SetEReaderTrainerGfxId(void) {}
static inline u8 GetBattleFacilityTrainerGfxId(u16 trainerId) { return 0; }
static inline void PutNewBattleTowerRecord(struct EmeraldBattleTowerRecord *newRecordEm) {}
static inline u8 GetFrontierTrainerFrontSpriteId(u16 trainerId) { return 0; }
static inline u8 GetFrontierOpponentClass(u16 trainerId) { return 0; }
static inline void GetFrontierTrainerName(u8 *dst, u16 trainerId) {}
static inline void FillFrontierTrainerParty(u8 monsCount) {}
static inline void FillFrontierTrainersParties(u8 monsCount) {}
static inline u16 GetRandomFrontierMonFromSet(u16 trainerId) { return 0; }
static inline void FrontierSpeechToString(const u16 *words) {}
static inline void DoSpecialTrainerBattle(void) {}
static inline void CalcEmeraldBattleTowerChecksum(struct EmeraldBattleTowerRecord *record) {}
static inline void CalcRubyBattleTowerChecksum(struct RSBattleTowerRecord *record) {}
static inline u16 GetCurrentBattleTowerWinStreak(u8 lvlMode, u8 battleMode) { return 0; }
static inline u8 GetEreaderTrainerFrontSpriteId(void) { return 0; }
static inline u8 GetEreaderTrainerClassId(void) { return 0; }
static inline void GetEreaderTrainerName(u8 *dst) {}
static inline void ValidateEReaderTrainer(void) {}
static inline void ClearEReaderTrainer(struct BattleTowerEReaderTrainer *ereaderTrainer) {}
static inline void CopyEReaderTrainerGreeting(void) {}
static inline void TryHideBattleTowerReporter(void) {}
static inline bool32 RubyBattleTowerRecordToEmerald(struct RSBattleTowerRecord *src, struct EmeraldBattleTowerRecord *dst) { return FALSE; }
static inline bool32 EmeraldBattleTowerRecordToRuby(struct EmeraldBattleTowerRecord *src, struct RSBattleTowerRecord *dst) { return FALSE; }
static inline void CalcApprenticeChecksum(struct Apprentice *apprentice) {}
static inline void GetBattleTowerTrainerLanguage(u8 *dst, u16 trainerId) {}
static inline u8 SetFacilityPtrsGetLevel(void) { return 50; }
static inline u8 GetFrontierEnemyMonLevel(u8 lvlMode) { return 50; }
static inline s32 GetHighestLevelInPlayerParty(void) { return 50; }
static inline u8 FacilityClassToGraphicsId(u8 facilityClass) { return 0; }
static inline bool32 ValidateBattleTowerRecord(u8 recordId) { return FALSE; }
static inline void TrySetLinkBattleTowerEnemyPartyLevel(void) {}

#endif /* GUARD_BATTLE_TOWER_H */
