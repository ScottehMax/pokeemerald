#ifndef GUARD_DMA3_H
#define GUARD_DMA3_H
#include "gba/types.h"
#include <string.h>
static inline void Dma3CopyLarge16_(const void *src, void *dst, u32 size) { memcpy(dst, src, size); }
static inline void Dma3FillLarge16_(u16 value, void *dst, u32 size) { memset(dst, value & 0xFF, size); }
static inline void DmaCopyLarge16(const void *src, void *dst, u32 size) { memcpy(dst, src, size); }
static inline void Dma3CopyLarge32_(const void *src, void *dst, u32 size) { memcpy(dst, src, size); }
static inline void DmaFill8(u8 dmaNum, u8 value, void *dst, u32 size) { memset(dst, value, size); }
static inline void Dma3ManagerBusyLoop(void) {}
static inline void RequestDma3Copy(const void *src, void *dst, u16 size, u8 mode) {}
static inline void RequestDma3Fill(u16 value, void *dst, u16 size, u8 mode) {}
static inline void Dma3CopyLarge_(const void *src, void *dst, u32 size) { memcpy(dst, src, size); }
#endif
