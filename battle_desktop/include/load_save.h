#ifndef GUARD_LOAD_SAVE_H
#define GUARD_LOAD_SAVE_H
#include "gba/types.h"
/* SaveBlock1/2 structs and gSaveBlock1Ptr/gSaveBlock2Ptr come from include/global.h via #include_next */
static inline bool8 MoveSaveBlocks_ResetHeap(void) { return FALSE; }
static inline void SaveGame_SetSaveFileExists(void) {}
static inline bool32 IsGamePlayTimeNotMaxed(void) { return TRUE; }
#endif
