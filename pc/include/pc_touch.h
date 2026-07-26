#ifndef GUARD_PC_TOUCH_H
#define GUARD_PC_TOUCH_H

#include "gba/types.h"

u16 PcTouchGetKeys(u16 physicalKeys);
bool32 PcTouchConsumeTap(s32 *x, s32 *y);

#endif
