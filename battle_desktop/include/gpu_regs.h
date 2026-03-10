#ifndef GUARD_GPU_REGS_H
#define GUARD_GPU_REGS_H

#include "gba/types.h"

static inline void SetGpuReg(u8 offset, u16 value) {}
static inline void SetGpuRegBits(u8 offset, u16 bits) {}
static inline void ClearGpuRegBits(u8 offset, u16 bits) {}
static inline u16 GetGpuReg(u8 offset) { return 0; }
static inline void SetGpuReg_ForcedBlank(u8 offset, u16 value) {}
static inline void EnableInterrupts(u16 flags) {}
static inline void DisableInterrupts(u16 flags) {}

#endif // GUARD_GPU_REGS_H
