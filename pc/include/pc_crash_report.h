#ifndef GUARD_PC_CRASH_REPORT_H
#define GUARD_PC_CRASH_REPORT_H

#include <stddef.h>
#include <stdint.h>

struct PcSharedState;

int PcWriteCrashReport(const struct PcSharedState *shared,
                       const char *corePath,
                       const char *savePath,
                       const uint32_t *pixels,
                       int exitStatus,
                       char *reportPath,
                       size_t reportPathSize);

#endif
