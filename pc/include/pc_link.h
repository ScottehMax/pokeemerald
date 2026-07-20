#ifndef GUARD_PC_LINK_H
#define GUARD_PC_LINK_H

#include "global.h"
#include "link.h"
#include "pc_link_protocol.h"

void PcLinkSetCode(const u8 *code);
bool32 PcLinkOpen(void);
void PcLinkClose(void);
u32 PcLinkMain(u16 *sendCmd, u16 (*recvCmds)[CMD_LENGTH]);
u8 PcLinkGetId(void);
bool32 PcLinkHasError(void);

#endif
