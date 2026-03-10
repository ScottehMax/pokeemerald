#ifndef GUARD_GBA_ISAGBPRINT_H
#define GUARD_GBA_ISAGBPRINT_H

#include <stdio.h>

/* On desktop, AGB debug print goes to stderr */
static inline void AGBPrintInit(void) {}
static inline void AGBPrint(const char *fmt, ...) {}
static inline void AGBPrintFlush(void) {}
static inline void mgba_printf(int level, const char *fmt, ...) {}
static inline void DebugPrintf(const char *fmt, ...) {}

#endif // GUARD_GBA_ISAGBPRINT_H
