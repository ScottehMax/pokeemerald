#ifndef GUARD_LINK_RFU_H
#define GUARD_LINK_RFU_H
#include "gba/types.h"
static inline void Task_WaitForLinkPlayerConnection(u8 taskId) {}
static inline void Task_WaitForLinkPlayerCountToMatch(u8 taskId) {}
static inline bool8 IsRfuRecvQueueFull(void) { return FALSE; }
#endif
