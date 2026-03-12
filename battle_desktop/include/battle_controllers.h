/*
 * battle_controllers.h - Desktop shadow header
 *
 * Chains to the real include/battle_controllers.h then overrides the two
 * GBA link-IPC macros that require hardware we don't have on desktop.
 *
 * On GBA with BATTLE_TYPE_LINK:
 *   MarkBattlerForControllerExec sets gBitTable[battler] << 28 ("outbound")
 *   instead of gBitTable[battler] ("local").  MarkBattlerReceivedLinkData
 *   (called by the link layer after all remotes ack) then clears those bits
 *   and sets the local bits.  IS_BATTLE_CONTROLLER_ACTIVE_OR_PENDING_SYNC_ANYWHERE
 *   blocks any battler while the (0xF<<28) word is non-zero.
 *
 * On desktop there is no link layer, so the outbound bits are set but never
 * cleared → infinite stall.  We redefine:
 *   MARK_BATTLE_CONTROLLER_MESSAGE_OUTBOUND_OVER_LINK  →  local bit (immediate)
 *   IS_BATTLE_CONTROLLER_ACTIVE_OR_PENDING_SYNC_ANYWHERE  →  drop (0xF<<28)
 *
 * All other BATTLE_TYPE_LINK game logic (no bag, no shift style, link battle
 * outcome strings, etc.) is completely untouched.
 */
#include_next "battle_controllers.h"

/* --- Link IPC flag overrides -------------------------------------------- */

#undef  MARK_BATTLE_CONTROLLER_MESSAGE_OUTBOUND_OVER_LINK
#define MARK_BATTLE_CONTROLLER_MESSAGE_OUTBOUND_OVER_LINK(battler) \
    MARK_BATTLE_CONTROLLER_ACTIVE_ON_LOCAL(battler)

#undef  IS_BATTLE_CONTROLLER_ACTIVE_OR_PENDING_SYNC_ANYWHERE
#define IS_BATTLE_CONTROLLER_ACTIVE_OR_PENDING_SYNC_ANYWHERE(battler) \
   (gBattleControllerExecFlags & ( \
      (gBitTable[battler])       \
    | (gBitTable[battler] << 4)  \
    | (gBitTable[battler] << 8)  \
    | (gBitTable[battler] << 12) \
   ))
