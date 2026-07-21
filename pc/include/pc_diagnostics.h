#ifndef GUARD_PC_DIAGNOSTICS_H
#define GUARD_PC_DIAGNOSTICS_H

#include "global.h"
#include "pc_shared.h"

void PcDiagnosticsInit(struct PcSharedState *shared);
void PcDiagnosticsFrame(void);
void PcDiagnosticsEnterMain(u32 kind, const void *callback);
void PcDiagnosticsLeaveMain(void);
void PcDiagnosticsEnterTask(u8 taskId, const void *callback, const s16 *data);
void PcDiagnosticsLeaveTask(void);
void PcDiagnosticsEnterSprite(u8 spriteId, const void *callback, const s16 *data);
void PcDiagnosticsLeaveSprite(void);
void PcDiagnosticsEnterScript(const void *script, u32 command, const void *callback);
void PcDiagnosticsLeaveScript(void);
bool32 PcDiagnosticsIsExecutable(const void *callback);
void PcDiagnosticsInvalidCallback(u32 kind, u32 id, const void *callback) __attribute__((noreturn));
bool32 PcDiagnosticsCaptureCrash(u32 code, const void *nativeInfo, const void *nativeContext);
void PcDiagnosticsTriggerTestCrash(const char *kind) __attribute__((noreturn));

#endif
