#ifndef GUARD_PARTY_MENU_H
#define GUARD_PARTY_MENU_H
#include "gba/types.h"
static inline void UpdatePartyMenu(void) {}
static inline void DrawPartyStatusSummary(void *hpAndStatus, u8 flags, u8 battlerId) {}
static inline void HidePartyStatusSummary(u8 battlerId) {}
static inline void SetPartyStatusSummaryBar(u8 battlerId, u8 barType, u8 battlerCount) {}
#define PARTY_SIZE 6
extern u8 gBattlePartyCurrentOrder[PARTY_SIZE / 2];
#endif
