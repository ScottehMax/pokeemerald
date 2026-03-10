#ifndef GUARD_GBA_SYSCALL_H
#define GUARD_GBA_SYSCALL_H

#include <string.h>
#include <math.h>
#include "gba/types.h"

/* GBA BIOS calls replaced with standard C equivalents */

static inline void CpuSet(const void *src, void *dst, u32 ctrl)
{
    /* ctrl bits: bit 24 = fill mode, bit 26 = 32-bit, bits 0-20 = count */
    u32 count = ctrl & 0x1FFFFF;
    if (ctrl & (1 << 24)) {
        /* fill mode */
        if (ctrl & (1 << 26))
            count *= 4; /* 32-bit */
        else
            count *= 2; /* 16-bit */
        memset(dst, *(const u8 *)src, count);
    } else {
        if (ctrl & (1 << 26))
            count *= 4;
        else
            count *= 2;
        memcpy(dst, src, count);
    }
}

static inline void CpuFastSet(const void *src, void *dst, u32 ctrl)
{
    u32 count = (ctrl & 0x1FFFFF) * 4; /* always 32-bit */
    if (ctrl & (1 << 24))
        memset(dst, *(const u8 *)src, count);
    else
        memcpy(dst, src, count);
}

static inline u32 Sqrt(u32 n)
{
    return (u32)sqrtf((float)n);
}

static inline s16 ArcTan(s16 a) { return (s16)(atanf((float)a / 16384.0f) * 16384.0f / 3.14159265f); }
static inline s16 ArcTan2(s16 a, s16 b) { return (s16)(atan2f((float)a, (float)b) * 32768.0f / 3.14159265f); }

/* CPU_SET constants */
#define CPU_SET_SRC_FIXED   (1 << 24)
#define CPU_SET_16BIT       0
#define CPU_SET_32BIT       (1 << 26)
#define CPU_FAST_SET_SRC_FIXED (1 << 24)

#endif // GUARD_GBA_SYSCALL_H
