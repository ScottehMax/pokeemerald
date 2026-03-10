#ifndef GUARD_FIELD_SPECIALS_H
#define GUARD_FIELD_SPECIALS_H

static inline u8 GetLeadMonIndex(void) { return 0; }
static inline bool8 IsDestinationBoxFull(void) { return FALSE; }
static inline u16 GetPCBoxToSendMon(void) { return 0; }
static inline bool8 InMultiPartnerRoom(void) { return FALSE; }
static inline void UpdateTrainerFansAfterLinkBattle(void) {}
static inline void IncrementBirthIslandRockStepCount(void) {}
static inline bool8 AbnormalWeatherHasExpired(void) { return FALSE; }
static inline bool8 ShouldDoBrailleRegicePuzzle(void) { return FALSE; }
static inline bool32 ShouldDoWallyCall(void) { return FALSE; }
static inline bool32 ShouldDoScottFortreeCall(void) { return FALSE; }
static inline bool32 ShouldDoScottBattleFrontierCall(void) { return FALSE; }
static inline bool32 ShouldDoRoxanneCall(void) { return FALSE; }
static inline bool32 ShouldDoRivalRayquazaCall(void) { return FALSE; }
static inline bool32 CountSSTidalStep(u16 delta) { return FALSE; }
static inline u8 GetSSTidalLocation(s8 *mapGroup, s8 *mapNum, s16 *x, s16 *y) { return 0; }
static inline void ShowScrollableMultichoice(void) {}
static inline void FrontierGamblerSetWonOrLost(bool8 won) {}
static inline u8 TryGainNewFanFromCounter(u8 incrementId) { return 0; }
static inline bool8 InPokemonCenter(void) { return FALSE; }
static inline void SetShoalItemFlag(u16 unused) {}
static inline void UpdateFrontierManiac(u16 daysSince) {}
static inline void UpdateFrontierGambler(u16 daysSince) {}
static inline void ResetCyclingRoadChallengeData(void) {}
static inline bool8 UsedPokemonCenterWarp(void) { return FALSE; }

#endif /* GUARD_FIELD_SPECIALS_H */
