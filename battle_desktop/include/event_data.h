#ifndef GUARD_EVENT_DATA_H
#define GUARD_EVENT_DATA_H
#include "gba/types.h"
static inline u16 VarGet(u16 varId) { return 0; }
static inline void VarSet(u16 varId, u16 value) {}
static inline bool8 FlagGet(u16 flagId) { return FALSE; }
static inline void FlagSet(u16 flagId) {}
static inline void FlagClear(u16 flagId) {}
extern u16 gSpecialVar_Result;
extern u16 gSpecialVar_0x8004;
extern u16 gSpecialVar_0x8005;
extern u16 gSpecialVar_0x8006;
extern u16 gSpecialVar_0x8007;
extern u16 gSpecialVar_0x8008;
extern u16 gSpecialVar_MonBoxId;
extern u16 gSpecialVar_MonBoxPos;
#endif
