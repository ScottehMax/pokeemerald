#ifndef GUARD_DECOMPRESS_H
#define GUARD_DECOMPRESS_H
#include "gba/types.h"
extern u8 gDecompressionBuffer[];
static inline void LZDecompressWram(const void *src, void *dst) {}
static inline void LZDecompressVram(const void *src, void *dst) {}
static inline u32 LoadCompressedSpriteSheetOverrideBuffer(const void *a, u16 b, u16 c, void *d) { return 0; }
static inline void DecompressDataWithHeaderWram(const u32 *src, void *dst) {}
static inline void DecompressDataWithHeaderVram(const u32 *src, void *dst) {}
static inline void CopyToVram(const void *src, u16 offset, u16 size) {}
static inline u32 DecompressDataWithHeader(const void *src) { return 0; }
#endif
