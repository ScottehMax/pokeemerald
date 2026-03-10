#ifndef GUARD_RTC_UTIL_H
#define GUARD_RTC_UTIL_H

/* struct Time defined in global.h */
#include "siirtc.h"
extern struct Time gLocalTime;
static inline void RtcInit(void) {}
static inline void RtcGetInfo(struct Time *dest) {}
static inline u16 RtcGetErrorFlags(void) { return 0; }
static inline void RtcCalcLocalTime(void) {}
static inline void FormatDecimalTime(u8 *dest, s32 hours, s32 minutes, s32 seconds) {}

#endif /* GUARD_RTC_UTIL_H */
