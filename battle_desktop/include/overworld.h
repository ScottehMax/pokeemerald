#ifndef GUARD_OVERWORLD_H
#define GUARD_OVERWORLD_H

/* Desktop stub for overworld.h */

static inline void DoWhiteOut(void) {}
static inline void Overworld_ResetStateAfterFly(void) {}
static inline void Overworld_ResetStateAfterTeleport(void) {}
static inline void Overworld_ResetStateAfterDigEscRope(void) {}
static inline void ResetGameStats(void) {}
static inline void IncrementGameStat(u8 index) {}
static inline u32 GetGameStat(u8 index) { return 0; }
static inline void SetGameStat(u8 index, u32 value) {}
static inline void ApplyNewEncryptionKeyToGameStats(u32 newKey) {}
static inline void LoadObjEventTemplatesFromHeader(void) {}
static inline void LoadSaveblockObjEventScripts(void) {}
static inline void SetObjEventTemplateCoords(u8 localId, s16 x, s16 y) {}
static inline void SetObjEventTemplateMovementType(u8 localId, u8 movementType) {}
static inline void ApplyCurrentWarp(void) {}
static inline void WarpIntoMap(void) {}
static inline void SetWarpDestination(s8 mapGroup, s8 mapNum, s8 warpId, s8 x, s8 y) {}
static inline void SetWarpDestinationToMapWarp(s8 mapGroup, s8 mapNum, s8 warpId) {}
static inline void SetDynamicWarp(s32 unused, s8 mapGroup, s8 mapNum, s8 warpId) {}
static inline void SetDynamicWarpWithCoords(s32 unused, s8 mapGroup, s8 mapNum, s8 warpId, s8 x, s8 y) {}
static inline void SetWarpDestinationToDynamicWarp(u8 unusedWarpId) {}
static inline void SetWarpDestinationToHealLocation(u8 healLocationId) {}

#endif /* GUARD_OVERWORLD_H */
