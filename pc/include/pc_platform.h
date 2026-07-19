#ifndef GUARD_PC_PLATFORM_H
#define GUARD_PC_PLATFORM_H

#include "gba/types.h"

typedef void (*PcInterruptCallback)(void);

bool32 PcPlatformInit(const char *sharedPath);
void PcPlatformShutdown(void);
void PcPlatformWaitForFrame(void);
void PcPlatformPresentFrame(PcInterruptCallback hblankCallback);
void PcPlatformSoftReset(void) __attribute__((noreturn));
void PcPlatformQueueAudio(const s16 *samples, u32 frameCount);
void PcPlatformRunTestHooks(void);
void PcPlatformStartTimer1(void);
u16 PcPlatformStopTimer1(void);

void PcDmaSet(u8 channel, const void *src, void *dest, u32 control);
void PcDmaStop(u8 channel);
void PcDmaRunVBlank(void);
void PcDmaRunHBlank(void);

#endif
