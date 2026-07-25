#ifndef GUARD_PC_PLATFORM_H
#define GUARD_PC_PLATFORM_H

#include "gba/types.h"

typedef void (*PcInterruptCallback)(void);

bool32 PcPlatformInit(const char *sharedPath);
void PcPlatformShutdown(void);
void PcPlatformRecordExit(int status);
const char *PcPlatformGetDefaultSavePath(void);
const char *PcPlatformGetSavePath(void);
const char *PcPlatformGetLinkServer(void);
bool32 PcPlatformShouldResumeMainMenu(void);
void PcPlatformSwitchProfile(const char *savePath, const char *storagePath) __attribute__((noreturn));
void PcPlatformWaitForFrame(void);
void PcPlatformPresentFrame(PcInterruptCallback hblankCallback);
u32 PcPlatformGetOverworldViewportWidth(void);
u32 PcPlatformGetOverworldViewportHeight(void);
bool32 PcPlatformIsOverworldViewportActive(void);
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
