#ifndef GUARD_GRAPHICS_H
#define GUARD_GRAPHICS_H
#include "gba/types.h"
/* Graphics data stubs - all GFX arrays are declared extern here but defined as
   minimal stubs in stubs.c. The battle engine only uses them for loading sprites
   which is a no-op on desktop. */
extern const u32 gBattleTerrainTable[];
extern const u16 gPPTextPalette[];
extern const u8 gBattleTextboxTiles[];
extern const u16 gBattleTextboxPalette[];
extern const u8 gBattleInterface_BallDisplayGfx[];
#endif
