#ifndef GUARD_PC_PPU_H
#define GUARD_PC_PPU_H

#include <stdint.h>
#include "pc_platform.h"

void PcPpuRender(uint32_t *pixels, PcInterruptCallback hblankCallback);

#endif
