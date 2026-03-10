#ifndef GUARD_PALETTE_H
#define GUARD_PALETTE_H

#include "gba/types.h"

/* Palette fade control */
struct PaletteFadeControl { bool8 active; u8 y; u8 targetY; u8 speed; };
extern struct PaletteFadeControl gPaletteFade;

#define RGB_BLACK    0
#define RGB_WHITE    0x7FFF
#define PALETTES_BG      0x0000FFFF
#define PALETTES_OBJECTS 0xFFFF0000
#define PALETTES_ALL     (PALETTES_BG | PALETTES_OBJECTS)
#define PLTT_BUFFER_SIZE (0x400 / 2)
extern u16 gPlttBufferUnfaded[PLTT_BUFFER_SIZE];
extern u16 gPlttBufferFaded[PLTT_BUFFER_SIZE];

#define PLTT_ID(n)        ((n) * 16)
#define BG_PLTT_OFFSET    0
#define BG_PLTT_ID(n)     (BG_PLTT_OFFSET + PLTT_ID(n))
#define OBJ_PLTT_OFFSET   256
#define OBJ_PLTT_ID(n)    (OBJ_PLTT_OFFSET + PLTT_ID(n))

static inline void ResetPaletteFadeControl(void) { gPaletteFade.active = FALSE; }
static inline void BeginNormalPaletteFade(u32 palettes, s8 delay, u8 startY, u8 targetY, u16 color) {}
static inline bool8 UpdatePaletteFade(void) { return FALSE; }
static inline void SetGpuReg_ForcedBlankPalette(void) {}
static inline void LoadPalette(const void *src, u16 offset, u16 size) {}
static inline void FillPalette(u16 value, u16 offset, u16 size) {}
static inline void CpuCopy16Palette(const void *src, void *dst, u16 size) {}
static inline void CpuSet16Palette(u16 value, void *dst, u16 size) {}
static inline void TransferPlttBuffer(void) {}
static inline void WriteFastPalette(u16 index, u16 color) {}
static inline void WriteSequentialPalette(u16 index, const u16 *src, u16 count) {}
static inline void AnimateSpritePalette(u8 paletteIndex) {}
static inline void TrySwitchToSoftwareSpritePalette(bool8 isSoftware) {}
static inline u8 GetSpritePaletteByTag(u16 tag) { return 0; }
static inline void LoadCompressedPalette(const u32 *src, u16 offset, u16 size) {}
static inline void SetPaletteEntries(const u16 *palette, u16 offset, u16 size) {}

#endif // GUARD_PALETTE_H
