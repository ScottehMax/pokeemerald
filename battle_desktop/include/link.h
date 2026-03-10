#ifndef GUARD_LINK_H
#define GUARD_LINK_H
#include "gba/types.h"
#define BLOCK_BUFFER_SIZE 0x100
#define MAX_RFU_PLAYERS 4
#define MAX_LINK_PLAYERS 4
extern u8 gWirelessCommType;
extern bool8 gReceivedRemoteLinkPlayers;
struct LinkPlayer {
    u16 version;
    u16 lp_field_2;
    u32 trainerId;
    u8 name[8];
    u8 progressFlags;
    u8 neverRead;
    u8 progressFlagsCopy;
    u8 gender;
    u32 linkType;
    u16 id;
    u16 language;
};
extern struct LinkPlayer gLinkPlayers[];
extern u16 gBlockRecvBuffer[MAX_RFU_PLAYERS][BLOCK_BUFFER_SIZE / 2];
static inline void OpenLink(void) {}
static inline void CloseLink(void) {}
static inline bool8 IsLinkMaster(void) { return FALSE; }
static inline bool8 GetLinkPlayerCount(void) { return 0; }
static inline bool8 IsLinkPlayerDataReceivedAllPlayers(void) { return FALSE; }
static inline bool8 IsLinkRecovery(void) { return FALSE; }
static inline void SetSuppressLinkErrorMessage(bool8 val) {}
static inline bool8 IsLinkConnectionEstablished(void) { return FALSE; }
static inline void SetWirelessCommType1(void) {}
#endif
