#ifndef GUARD_GBA_MACRO_H
#define GUARD_GBA_MACRO_H

#include <string.h>

/* Replace GBA DMA-based memory operations with standard C equivalents */

#define CpuFill16(value, dest, size)  memset((dest), (value) & 0xFF, (size))
#define CpuFill32(value, dest, size)  memset((dest), (value) & 0xFF, (size))
#define CpuFastFill(value, dest, size) memset((dest), (value) & 0xFF, (size))
#define CpuFastFill16(value, dest, size) memset((dest), (value) & 0xFF, (size))
#define CpuFastFill8(value, dest, size)  memset((dest), (u8)(value), (size))

#define CpuCopy16(src, dest, size)  memcpy((dest), (src), (size))
#define CpuCopy32(src, dest, size)  memcpy((dest), (src), (size))
#define CpuFastCopy(src, dest, size) memcpy((dest), (src), (size))

/* CPU_FILL / CPU_COPY used in battle code */
#define CPU_FILL(value, dest, size, bit) CpuFill##bit(value, dest, size)
#define CPU_COPY(src, dest, size, bit)   CpuCopy##bit(src, dest, size)
#define CPU_FILL_UNCHECKED(value, dest, size, bit) CPU_FILL(value, dest, size, bit)
#define CPU_COPY_UNCHECKED(src, dest, size, bit)   CPU_COPY(src, dest, size, bit)

/* DMA macros - no-op on desktop (graphics memory not used) */
#define DmaSetUnchecked(dmaNum, src, dest, control) do {} while(0)
#define DmaSet(dmaNum, src, dest, control) do {} while(0)
#define DmaClear(dmaNum, dest, size) memset((void *)(dest), 0, (size))
#define DmaCopy16Defn(dmaNum, src, dest, size) memcpy((void *)(dest), (const void *)(src), (size))
#define DmaCopy32Defn(dmaNum, src, dest, size) memcpy((void *)(dest), (const void *)(src), (size))
#define DmaCopy16(dmaNum, src, dest, size) memcpy((void *)(dest), (const void *)(src), (size))
#define DmaCopy32(dmaNum, src, dest, size) memcpy((void *)(dest), (const void *)(src), (size))
#define DmaFill16(dmaNum, value, dest, size) memset((void *)(dest), (value) & 0xFF, (size))
#define DmaFill32(dmaNum, value, dest, size) memset((void *)(dest), (value) & 0xFF, (size))

/* Q-format macros (used in some math) */
#define Q_8_8(n)   ((s16)((n) * 256))
#define Q_4_12(n)  ((s16)((n) * 4096))
#define Q_24_8(n)  ((s32)((n) << 8))
#define Q_8_8_TO_INT(n)  ((int)((n) / 256))
#define Q_4_12_TO_INT(n) ((int)((n) / 4096))
#define Q_24_8_TO_INT(n) ((int)((n) >> 8))

#endif // GUARD_GBA_MACRO_H
