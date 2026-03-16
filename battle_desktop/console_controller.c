/*
 * console_controller.c - Text-based battle controller for desktop
 *
 * This replaces the GBA player and opponent controllers. It handles all
 * CONTROLLER_* messages from the battle engine:
 *
 *   - PRINTSTRING: decodes the GF-encoded string and prints to stdout
 *   - CHOOSEACTION: presents a text menu; player picks from stdin
 *   - CHOOSEMOVE: lists available moves; player picks from stdin
 *   - CHOOSEPOKEMON: lists party; player picks from stdin
 *   - All animation/visual commands: immediate no-op completion
 *
 * Battler 0 (B_POSITION_PLAYER_LEFT) always uses stdin for input.
 * Battler 1 (B_POSITION_OPPONENT_LEFT) uses the AI system.
 *
 * The controller replaces both SetControllerToPlayer and SetControllerToOpponent.
 */

#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_message.h"
#include "battle_ai_script_commands.h"
#include "battle_ai_switch_items.h"
#include "battle_main.h"
#include "pokemon.h"
#include "string_util.h"
#include "util.h"
#include "data.h"
#include "party_menu.h"
#include "task.h"
#include "constants/party_menu.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/characters.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* =========================================================================
 * Link-mode buffer loopback
 *
 * In BATTLE_TYPE_LINK mode, BtlController_Emit* routes all command data
 * to gLinkBattleSendBuffer (via PrepareBufferDataTransferLink) rather than
 * to gBattleBufferA[battler] directly.  On real GBA the link hardware would
 * send the data to remote players who copy it into gBattleBufferA.  On
 * desktop there is no link hardware, so we do the copy ourselves.
 *
 * Layout of each message in gLinkBattleSendBuffer (from battle_controllers.c):
 *   [0] LINK_BUFF_BUFFER_ID         — B_COMM_TO_CONTROLLER or B_COMM_TO_ENGINE
 *   [1] LINK_BUFF_ACTIVE_BATTLER    — which battler this is for
 *   [2] LINK_BUFF_ATTACKER
 *   [3] LINK_BUFF_TARGET
 *   [4] LINK_BUFF_SIZE_LO           — alignedSize low byte
 *   [5] LINK_BUFF_SIZE_HI           — alignedSize high byte
 *   [6] LINK_BUFF_ABSENT_BATTLER_FLAGS
 *   [7] LINK_BUFF_EFFECT_BATTLER
 *   [8..8+alignedSize-1] data payload
 *
 * We know (from battle_controllers.c task creation order, with ResetTasks()
 * called before InitBattle):
 *   slot 0 = Task_WaitForLinkPlayerConnection (no-op stub)
 *   slot 1 = Task_HandleSendLinkBuffersData   → sLinkSendTaskId = 1
 * gTasks[1].data[14] = tCurrentBlock_End (next-write position in the buffer).
 * ========================================================================= */

enum {
    LBUF_BUFFER_ID = 0,
    LBUF_ACTIVE_BATTLER,
    LBUF_ATTACKER,
    LBUF_TARGET,
    LBUF_SIZE_LO,
    LBUF_SIZE_HI,
    LBUF_ABSENT_FLAGS,
    LBUF_EFFECT_BATTLER,
    LBUF_DATA,            /* first byte of payload */
};
#define LINK_SEND_TASK_END_IDX 14  /* data[14] = tCurrentBlock_End */
#define LINK_SEND_TASK_WRAP_IDX 12 /* data[12] = tCurrentBlock_WrapFrom */

/*
 * Scan gLinkBattleSendBuffer for the message addressed to `battler` and
 * copy its payload to gBattleBufferA[battler].
 *
 * Instead of reading a task's data[14] to know the buffer end, we scan
 * linearly: each message's header contains the payload size (alignSz).
 * We advance by (alignSz + LBUF_DATA) per message.  A zero alignSz
 * marks the end (the buffer is zeroed after each frame by
 * Desktop_ResetLinkSendBuffer).
 *
 * Called from ConsoleBufferRunCommand before dispatching the command.
 */
static void Desktop_CopyLinkMessageForBattler(u8 battler)
{
    u16 pos = 0;

    while (pos + LBUF_DATA < BATTLE_BUFFER_LINK_SIZE)
    {
        u16 alignSz = (u16)gLinkBattleSendBuffer[pos + LBUF_SIZE_LO]
                     | ((u16)gLinkBattleSendBuffer[pos + LBUF_SIZE_HI] << 8);

        if (alignSz == 0)
            break;

        if (gLinkBattleSendBuffer[pos + LBUF_ACTIVE_BATTLER] == battler)
        {
            u8  bufId = gLinkBattleSendBuffer[pos + LBUF_BUFFER_ID];
            /* Only copy B_COMM_TO_CONTROLLER messages (→ gBattleBufferA).
             * B_COMM_TO_ENGINE messages (controller responses) are handled
             * by Desktop_ResetLinkSendBuffer after all controllers run.
             * Scan ALL messages without early return so the LAST
             * B_COMM_TO_CONTROLLER command for this battler wins — e.g.
             * CHOOSEACTION is emitted after DRAWPARTYSTATUSSUMMARY/PRINTSTRING
             * in the same frame and must not be shadowed by them. */
            if (bufId != B_COMM_TO_ENGINE)
            {
                u8 *dst = gBattleBufferA[battler];
                const u8 *src = &gLinkBattleSendBuffer[pos + LBUF_DATA];
                u16 i;
                for (i = 0; i < alignSz && i < 0x200; i++)
                    dst[i] = src[i];
            }
        }

        pos += (u16)(alignSz + LBUF_DATA);
    }
}

/*
 * Flush all pending messages from gLinkBattleSendBuffer, then zero the
 * buffer and reset the write pointer.  Called from RunBattleLoop after
 * all battler controllers have run for the frame.
 *
 * B_COMM_TO_CONTROLLER messages were already copied to gBattleBufferA by
 * Desktop_CopyLinkMessageForBattler() inside ConsoleBufferRunCommand.
 *
 * B_COMM_TO_ENGINE messages are controller RESPONSES (chosen action, chosen
 * move, mon data, etc.).  The battle engine reads these from gBattleBufferB
 * on the NEXT frame.  We copy them here before clearing the buffer so the
 * engine sees them.
 */
void Desktop_ResetLinkSendBuffer(void)
{
    u16 pos = 0;
    u8 i;

    while (pos + LBUF_DATA < BATTLE_BUFFER_LINK_SIZE)
    {
        u8  battler  = gLinkBattleSendBuffer[pos + LBUF_ACTIVE_BATTLER];
        u16 alignSz  = (u16)gLinkBattleSendBuffer[pos + LBUF_SIZE_LO]
                     | ((u16)gLinkBattleSendBuffer[pos + LBUF_SIZE_HI] << 8);
        u8  bufId    = gLinkBattleSendBuffer[pos + LBUF_BUFFER_ID];

        if (alignSz == 0)
            break;

        if (bufId == B_COMM_TO_ENGINE && battler < MAX_BATTLERS_COUNT)
        {
            const u8 *src = &gLinkBattleSendBuffer[pos + LBUF_DATA];
            u16 j;
            for (j = 0; j < alignSz && j < 0x200; j++)
                gBattleBufferB[battler][j] = src[j];
        }

        pos += (u16)(alignSz + LBUF_DATA);
    }

    /* Zero the entire buffer so next frame's scan sees alignSz=0 at pos 0 */
    memset(gLinkBattleSendBuffer, 0, BATTLE_BUFFER_LINK_SIZE);

    /* Reset the write pointer in ALL candidate task slots.
     * sLinkSendTaskId is private to battle_controllers.c; rather than
     * guessing which slot it is, we reset all of them.  This is safe
     * because the send task is stuck at state 2 on desktop (it never
     * reaches the state that reads tCurrentBlock_End). */
    for (i = 0; i < 3; i++)
    {
        gTasks[i].data[LINK_SEND_TASK_END_IDX]  = 0;
        gTasks[i].data[LINK_SEND_TASK_WRAP_IDX] = 0;
    }
}

/* Set to TRUE by main() when --debug flag is passed */
bool8 gDebugMode = FALSE;

/* Set to TRUE by main() when --pvp flag is passed.
 * Both sides are human-controlled via stdin; BATTLE_TYPE_LINK is used. */
bool8 gPvpMode = FALSE;

/* Set to TRUE by main() when --ai flag is passed.
 * Both sides are AI-controlled; no stdin input is required. */
bool8 gBothAiMode = FALSE;

/* Forward declarations */
static void ConsoleBufferRunCommand(void);
static void ConsoleBufferExecCompleted(void);

/* From data_transfer.c */
extern u32 CopyPlayerMonData(u8 monId, u8 *dst);
extern u32 CopyOpponentMonData(u8 monId, u8 *dst);
extern void SetPlayerMonData(u8 monId);
extern void SetOpponentMonData(u8 monId);

/* =========================================================================
 * GF character encoding → ASCII decoder
 * =========================================================================
 * The Generation III character table maps encoded bytes to ASCII.
 * Space = 0x00, A-Z = 0xBB-0xD4, a-z = 0xD5-0xEE,
 * 0-9 = 0xA1-0xAA, punctuation as documented in charmap.txt.
 */

static const char sGfCharTable[256] = {
    /* 0x00 */ ' ', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x10 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x20 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x30 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x40 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x50 */ '?', '?', '?', 'P', 'K', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x60 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x70 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x80 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0x90 */ '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    /* 0xA0 */ '?', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '!', '?', '.', '-', '_',
    /* 0xB0 */ '.', '"', '"', '\'','\'','m','f','Y',',','x', '/', 'A', 'B', 'C', 'D', 'E',
    /* 0xC0 */ 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U',
    /* 0xD0 */ 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k',
    /* 0xE0 */ 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '?',
    /* 0xF0 */ ':', 'A', 'B', 'C', 'D', '?', '?', '?', '?', '?', '?', '?', '?', '?', '\n','\0'
    /* 0xFE=\n, 0xFF=\0 */
};

/* Decode a GF-encoded string into a C string (null-terminated ASCII).
 *
 * Handles two string formats that coexist in the desktop build:
 *   Modern (MODERN=1): _() macro stores strings as plain ASCII + 0xFF.
 *     Printable ASCII bytes (0x20-0x7E) are passed through directly.
 *   GF encoding: bytes 0x80+ are looked up in sGfCharTable.
 *     Used for player name, pokemon nicknames set via SetMonData, etc.
 */
static void DecodeGFString(const u8 *src, char *dst, size_t dstSize)
{
    size_t i = 0;
    while (i < dstSize - 1) {
        u8 c = *src++;
        if (c == EOS) break;           /* 0xFF = end of string */
        if (c == 0xFE) {               /* \n  = new line (GBA line wrap -> space) */
            dst[i++] = ' ';
            continue;
        }
        if (c == 0xFB) {               /* \p  = new paragraph */
            dst[i++] = '\n';
            continue;
        }
        if (c == 0xFA) {               /* \l  = scroll/line */
            dst[i++] = ' ';
            continue;
        }
        if (c == PLACEHOLDER_BEGIN) {  /* 0xFD = expanded placeholder */
            u8 kind = *src++;
            (void)kind;
            continue;
        }
        /* GF chars that map to multi-byte UTF-8 sequences */
        if (c == 0x06 && i + 2 < (int)dstSize - 1) { /* É */
            dst[i++] = (char)0xC3; dst[i++] = (char)0x89; continue;
        }
        if (c == 0x1B && i + 2 < (int)dstSize - 1) { /* é */
            dst[i++] = (char)0xC3; dst[i++] = (char)0xA9; continue;
        }
        if (c == 0xB0 && i + 3 < (int)dstSize - 1) { /* … */
            dst[i++] = (char)0xE2; dst[i++] = (char)0x80; dst[i++] = (char)0xA6; continue;
        }
        if (c >= 0x20 && c <= 0x7E) {  /* printable ASCII — modern _() strings */
            dst[i++] = (char)c;
        } else {
            char ch = sGfCharTable[c];
            if (ch != '\0')
                dst[i++] = ch;
        }
    }
    dst[i] = '\0';
}

/* Print a GF-encoded battle string to stdout */
static void PrintGFString(const u8 *str)
{
    char buf[512];
    DecodeGFString(str, buf, sizeof(buf));
    if (buf[0] != '\0' && buf[0] != '\n') {
        printf("%s\n", buf);
        fflush(stdout);
    }
}

/* =========================================================================
 * Controller registration and dispatch
 * =========================================================================
 */

void SetControllerToConsole(void)
{
    gBattlerControllerFuncs[gActiveBattler] = ConsoleBufferRunCommand;
    /* If a command is already pending (dispatched before InitBattleControllers ran its
     * SetControllerToX init), process it now while the link buffer still has the message. */
    if (gBattleControllerExecFlags & gBitTable[gActiveBattler])
        ConsoleBufferRunCommand();
}

/* Called by our replacements of SetControllerToPlayer and SetControllerToOpponent */
void SetControllerToPlayer(void)   { SetControllerToConsole(); }
void SetControllerToOpponent(void) { SetControllerToConsole(); }

/* =========================================================================
 * Controller completion
 * =========================================================================
 */

static void ConsoleBufferExecCompleted(void)
{
    gBattlerControllerFuncs[gActiveBattler] = ConsoleBufferRunCommand;
    /* Mark this battler's controller as done */
    gBattleControllerExecFlags &= ~gBitTable[gActiveBattler];
}

/* =========================================================================
 * Helper: is this battler on the player's side?
 * =========================================================================
 */

static bool8 IsPlayerSide(void)
{
    return (GET_BATTLER_SIDE(gActiveBattler) == B_SIDE_PLAYER);
}

/* Returns TRUE for any battler on the player's side.
 * In singles: only battler 0 (B_POSITION_PLAYER_LEFT).
 * In doubles: battlers 0 and 2 (both player positions) — the human controls both.
 * Opponents (battlers 1 and 3) use AI in normal mode.
 * In PvP mode (--pvp / BATTLE_TYPE_LINK), ALL battlers are human-controlled. */
static bool8 IsHumanControlled(void)
{
    if (gBothAiMode)
        return FALSE;
    if (gPvpMode)
        return TRUE;
    u8 position = GetBattlerPosition(gActiveBattler);
    return (position == B_POSITION_PLAYER_LEFT || position == B_POSITION_PLAYER_RIGHT);
}

/* Returns a short label for the active battler suitable for prompts.
 * Normal mode: "Your Left" / "Your Right" / etc.
 * PvP mode: "P1 Left" / "P1 Right" / "P2 Left" / "P2 Right". */
static const char *GetBattlerLabel(void)
{
    u8 position = GetBattlerPosition(gActiveBattler);
    if (gPvpMode) {
        switch (position) {
        case B_POSITION_PLAYER_LEFT:    return "P1 Left";
        case B_POSITION_PLAYER_RIGHT:   return "P1 Right";
        case B_POSITION_OPPONENT_LEFT:  return "P2 Left";
        case B_POSITION_OPPONENT_RIGHT: return "P2 Right";
        default:                        return "?";
        }
    }
    switch (position) {
    case B_POSITION_PLAYER_LEFT:    return "Your Left";
    case B_POSITION_PLAYER_RIGHT:   return "Your Right";
    case B_POSITION_OPPONENT_LEFT:  return "Opponent Left";
    case B_POSITION_OPPONENT_RIGHT: return "Opponent Right";
    default:                        return "?";
    }
}

/* =========================================================================
 * Individual command handlers
 * =========================================================================
 */

/* --- PRINTSTRING ---
 * The engine has already called BufferStringBattle() in the source file
 * that emitted this command; gDisplayedStringBattle contains the expanded text.
 */
static void ConsoleHandlePrintString(void)
{
    u16 stringId = *(u16 *)(&gBattleBufferA[gActiveBattler][2]);

    /* Expand the string into gDisplayedStringBattle */
    BufferStringBattle(stringId);

    /* Decode and print if non-empty */
    if (gDisplayedStringBattle[0] != EOS) {
        PrintGFString(gDisplayedStringBattle);
    }

    ConsoleBufferExecCompleted();
}

/* --- PRINTSTRING (selection only) --- */
static void ConsoleHandlePrintSelectionString(void)
{
    if (IsPlayerSide()) {
        ConsoleHandlePrintString();
    } else {
        ConsoleBufferExecCompleted();
    }
}

/* --- CHOOSEACTION ---
 * Player battler: present Fight/Bag/Pokemon/Run menu, read choice from stdin.
 * Opponent battler: delegate to AI (AI_TrySwitchOrUseItem sets gChosenActionByBattler).
 */
static void ConsoleHandleChooseAction(void)
{
    if (!IsHumanControlled()) {
        /* Opponent or partner: use AI to decide */
        AI_TrySwitchOrUseItem();
        ConsoleBufferExecCompleted();
        return;
    }

    /* In doubles, the right-flank player can cancel back to redo the left-flank
     * player's action, matching the B_BUTTON behaviour from HandleInputChooseAction. */
    bool8 canCancelPartner = (gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
        && GetBattlerPosition(gActiveBattler) == B_POSITION_PLAYER_RIGHT
        && !(gBattleTypeFlags & BATTLE_TYPE_MULTI)
        && !(gAbsentBattlerFlags & gBitTable[GetBattlerAtPosition(B_POSITION_PLAYER_LEFT)]);

    /* Player: show a text menu */
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        printf("\n--- Choose Action (%s Pokemon) ---\n", GetBattlerLabel());
    } else {
        printf("\n--- Choose Action%s ---\n", gPvpMode ? (IsPlayerSide() ? " (P1)" : " (P2)") : "");
    }
    printf("  1. FIGHT\n");
    printf("  2. BAG\n");
    printf("  3. POKEMON\n");
    printf("  4. RUN\n");
    if (canCancelPartner)
        printf("  0. Back (redo Left Pokemon's action)\n");
    printf("Choice (%s1-4): ", canCancelPartner ? "0/" : "");
    fflush(stdout);

    int choice = 0;
    char line[64];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) { choice = 1; break; }
        choice = atoi(line);
        if (choice >= 1 && choice <= 4) break;
        if (canCancelPartner && choice == 0) break;
        printf("Invalid choice. Enter %s1-4: ", canCancelPartner ? "0/" : "");
        fflush(stdout);
    }

    if (canCancelPartner && choice == 0) {
        /* Cancel partner — engine resets battler 0 to STATE_BEFORE_ACTION_CHOSEN */
        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, B_ACTION_CANCEL_PARTNER, 0);
        ConsoleBufferExecCompleted();
        return;
    }

    u8 action;
    switch (choice) {
        case 2: action = B_ACTION_USE_ITEM;  break;
        case 3: action = B_ACTION_SWITCH;    break;
        case 4: action = B_ACTION_RUN;       break;
        default: action = B_ACTION_USE_MOVE; break;
    }
    /* Write the chosen action to gBattleBufferB so the engine can read it back */
    BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, action, 0);
    ConsoleBufferExecCompleted();
}

/* --- CHOOSEMOVE ---
 * Player battler: show move list, read choice.
 * Opponent battler: use AI.
 */
static void ConsoleHandleChooseMove(void)
{
    struct ChooseMoveStruct *moveInfo = (struct ChooseMoveStruct *)(&gBattleBufferA[gActiveBattler][4]);

    if (!IsHumanControlled()) {
        /* Opponent or partner: run AI */
        u8 chosenMoveId;
        if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_FIRST_BATTLE)) {
            BattleAI_SetupAIData(ALL_MOVES_MASK);
            chosenMoveId = BattleAI_ChooseMoveOrAction();
        } else {
            /* Wild mon: pick random valid move */
            do {
                chosenMoveId = Random() % MAX_MON_MOVES;
            } while (moveInfo->moves[chosenMoveId] == MOVE_NONE);
        }

        switch (chosenMoveId) {
            case AI_CHOICE_WATCH:
                BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, B_ACTION_SAFARI_WATCH_CAREFULLY, 0);
                break;
            case AI_CHOICE_FLEE:
                BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, B_ACTION_RUN, 0);
                break;
            case 6: /* AI_CHOICE_SWITCH */
                BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 15, gBattlerTarget);
                break;
            default: {
                u8 target;
                u16 moveTgt = gBattleMoves[moveInfo->moves[chosenMoveId]].target;
                if (moveTgt & (MOVE_TARGET_USER_OR_SELECTED | MOVE_TARGET_USER))
                    target = gActiveBattler;
                else if (moveTgt & MOVE_TARGET_BOTH) {
                    target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
                    if (gAbsentBattlerFlags & gBitTable[target])
                        target = GetBattlerAtPosition(BATTLE_PARTNER(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler))));
                } else {
                    target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
                }
                BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
                    (chosenMoveId) | (target << 8));
                break;
            }
        }
        ConsoleBufferExecCompleted();
        return;
    }

    /* Player: display available moves */
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        printf("\n--- Choose Move (%s Pokemon) ---\n", GetBattlerLabel());
    } else {
        printf("\n--- Choose Move%s ---\n", gPvpMode ? (IsPlayerSide() ? " (P1)" : " (P2)") : "");
    }
    u8 validMoves = 0;
    for (int i = 0; i < MAX_MON_MOVES; i++) {
        if (moveInfo->moves[i] != MOVE_NONE) {
            /* Decode move name from GF encoding */
            char moveName[24];
            const u8 *rawName = gMoveNames[moveInfo->moves[i]];
            DecodeGFString(rawName, moveName, sizeof(moveName));
            printf("  %d. %-15s PP: %d/%d\n",
                i + 1, moveName,
                moveInfo->currentPp[i], moveInfo->maxPp[i]);
            validMoves++;
        }
    }
    if (validMoves == 0) {
        /* No PP left - Struggle */
        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
            (MOVE_STRUGGLE) | (GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler))) << 8));
        ConsoleBufferExecCompleted();
        return;
    }

    printf("  0. Back\n");
    printf("Choice (0/%d): ", MAX_MON_MOVES);
    fflush(stdout);

    int choice = 0;
    char line[64];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) { choice = 1; break; }
        choice = atoi(line);
        if (choice == 0) break; /* Back */
        if (choice >= 1 && choice <= MAX_MON_MOVES
                && moveInfo->moves[choice - 1] != MOVE_NONE) break;
        printf("Invalid. Enter 0 to go back, or 1-%d: ", MAX_MON_MOVES);
        fflush(stdout);
    }

    if (choice == 0) {
        /* Go back to action selection for this battler (engine resets to STATE_BEFORE_ACTION_CHOSEN) */
        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10, 0xFFFF);
        ConsoleBufferExecCompleted();
        return;
    }

    u8 moveSlot = (u8)(choice - 1);
    u8 target;
    u16 moveTgt = gBattleMoves[moveInfo->moves[moveSlot]].target;

    if (moveTgt & MOVE_TARGET_USER) {
        target = gActiveBattler;
    } else if ((gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
               && !(moveTgt & (MOVE_TARGET_RANDOM | MOVE_TARGET_BOTH | MOVE_TARGET_DEPENDS
                               | MOVE_TARGET_FOES_AND_ALLY | MOVE_TARGET_OPPONENTS_FIELD))) {
        /* Doubles: build a list of valid targets and ask the player. */
        u8 tgtBattlers[4];
        const char *tgtNames[4];
        int tgtCount = 0;

        u8 oppLPos = BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler));
        u8 oppRPos = BATTLE_PARTNER(oppLPos);
        u8 oppL = GetBattlerAtPosition(oppLPos);
        u8 oppR = GetBattlerAtPosition(oppRPos);
        if (!(gAbsentBattlerFlags & gBitTable[oppL])) {
            tgtBattlers[tgtCount] = oppL;
            tgtNames[tgtCount]    = "Opponent-Left";
            tgtCount++;
        }
        if (!(gAbsentBattlerFlags & gBitTable[oppR])) {
            tgtBattlers[tgtCount] = oppR;
            tgtNames[tgtCount]    = "Opponent-Right";
            tgtCount++;
        }
        if (moveTgt & MOVE_TARGET_USER_OR_SELECTED) {
            tgtBattlers[tgtCount] = gActiveBattler;
            tgtNames[tgtCount]    = "Self";
            tgtCount++;
        }
        /* Ally target: partner across flank on the same side.
         * Valid for MOVE_TARGET_SELECTED and MOVE_TARGET_USER_OR_SELECTED. */
        u8 allyPos = BATTLE_PARTNER(GetBattlerPosition(gActiveBattler));
        u8 ally = GetBattlerAtPosition(allyPos);
        if (ally != gActiveBattler && !(gAbsentBattlerFlags & gBitTable[ally])) {
            tgtBattlers[tgtCount] = ally;
            tgtNames[tgtCount]    = "Ally";
            tgtCount++;
        }

        if (tgtCount == 0 || tgtCount == 1) {
            target = (tgtCount == 0)
                     ? GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)))
                     : tgtBattlers[0];
        } else {
            printf("\n--- Choose Target ---\n");
            for (int k = 0; k < tgtCount; k++)
                printf("  %d. %s\n", k + 1, tgtNames[k]);
            printf("Target (1-%d): ", tgtCount);
            fflush(stdout);
            char tline[32];
            int tgt = 1;
            while (1) {
                if (fgets(tline, sizeof(tline), stdin) == NULL) break;
                tgt = atoi(tline);
                if (tgt >= 1 && tgt <= tgtCount) break;
                printf("Invalid. Enter 1-%d: ", tgtCount);
                fflush(stdout);
            }
            target = tgtBattlers[tgt - 1];
        }
    } else if (moveTgt & MOVE_TARGET_USER_OR_SELECTED) {
        target = gActiveBattler;
    } else {
        target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
    }

    BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
        (moveSlot) | (target << 8));
    ConsoleBufferExecCompleted();
}

/* --- CHOOSEPOKEMON ---
 * Player: show party, pick a mon to switch in.
 * Opponent: use AI.
 */
static void ConsoleHandleChoosePokemon(void)
{
    s32 i;
    for (i = 0; i < (int)ARRAY_COUNT(gBattlePartyCurrentOrder); i++)
        gBattlePartyCurrentOrder[i] = gBattleBufferA[gActiveBattler][4 + i];

    /* In doubles, the partner's party index is also unavailable for switching.
     * Additionally, gBattleBufferA[gActiveBattler][2] holds the partner's
     * already-chosen monToSwitchIntoId (PARTY_SIZE means none chosen). */
    s32 partnerPartyIdx = -1;
    s32 alreadyChosenIdx = -1;
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        u8 partnerPos = BATTLE_PARTNER(GetBattlerPosition(gActiveBattler));
        u8 partner = GetBattlerAtPosition(partnerPos);
        if (partner != gActiveBattler && !(gAbsentBattlerFlags & gBitTable[partner]))
            partnerPartyIdx = gBattlerPartyIndexes[partner];
        u8 slotId = gBattleBufferA[gActiveBattler][2];
        if (slotId != PARTY_SIZE)
            alreadyChosenIdx = slotId;
    }

    if (!IsHumanControlled()) {
        s32 chosenMonId;
        if (*(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) == PARTY_SIZE) {
            chosenMonId = GetMostSuitableMonToSwitchInto();
            if (chosenMonId == PARTY_SIZE) {
                /* Fallback: first alive non-active mon */
                chosenMonId = gBattlerPartyIndexes[gActiveBattler];
                for (int i = 0; i < PARTY_SIZE; i++) {
                    if (i != gBattlerPartyIndexes[gActiveBattler]
                            && i != partnerPartyIdx
                            && i != alreadyChosenIdx
                            && GetMonData(&gEnemyParty[i], MON_DATA_HP, NULL) != 0
                            && GetMonData(&gEnemyParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE) {
                        chosenMonId = i;
                        break;
                    }
                }
            }
        } else {
            chosenMonId = *(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler);
        }
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, chosenMonId, gBattlePartyCurrentOrder);
        ConsoleBufferExecCompleted();
        return;
    }

    /* Determine if this is a forced switch (fainted mon must be replaced) */
    u8 caseId = gBattleBufferA[gActiveBattler][1] & 0xF;
    bool8 forced = (caseId == PARTY_ACTION_SEND_OUT);

    struct Pokemon *party = IsPlayerSide() ? gPlayerParty : gEnemyParty;

    /* Count valid (switchable) mons */
    int validCount = 0;
    for (int j = 0; j < PARTY_SIZE; j++) {
        u16 sp = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
        u16 hp = GetMonData(&party[j], MON_DATA_HP, NULL);
        if (sp != SPECIES_NONE && hp > 0
            && j != gBattlerPartyIndexes[gActiveBattler]
            && j != partnerPartyIdx
            && j != alreadyChosenIdx)
            validCount++;
    }

    /* Voluntary switch with no valid targets — cancel immediately */
    if (!forced && validCount == 0) {
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, PARTY_SIZE, gBattlePartyCurrentOrder);
        ConsoleBufferExecCompleted();
        return;
    }

    /* Player: show party */
    printf("\n--- Choose Pokemon (%s) ---\n", GetBattlerLabel());
    for (int j = 0; j < PARTY_SIZE; j++) {
        u16 species = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
        if (species == SPECIES_NONE) continue;
        u16 hp  = GetMonData(&party[j], MON_DATA_HP, NULL);
        u16 mhp = GetMonData(&party[j], MON_DATA_MAX_HP, NULL);
        u8 lvl  = GetMonData(&party[j], MON_DATA_LEVEL, NULL);
        char nick[12];
        u8 gfNick[12];
        GetMonData(&party[j], MON_DATA_NICKNAME, gfNick);
        DecodeGFString(gfNick, nick, sizeof(nick));
        printf("  %d. %-10s Lv%-3d HP: %d/%d%s\n",
            j + 1, nick, lvl, hp, mhp,
            (j == gBattlerPartyIndexes[gActiveBattler]) ? " [active]" :
            (j == partnerPartyIdx) ? " [active]" :
            (j == alreadyChosenIdx) ? " [switching]" :
            (hp == 0) ? " [fainted]" : "");
    }
    if (!forced)
        printf("  0. Back\n");
    printf("Choice: ");
    fflush(stdout);

    int choice = -1;
    char line[64];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* EOF: cancel if voluntary, else pick first valid mon */
            if (!forced) {
                BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, PARTY_SIZE, gBattlePartyCurrentOrder);
                ConsoleBufferExecCompleted();
                return;
            }
            for (int j = 0; j < PARTY_SIZE; j++) {
                u16 sp = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
                u16 hp = GetMonData(&party[j], MON_DATA_HP, NULL);
                if (sp != SPECIES_NONE && hp > 0
                    && j != gBattlerPartyIndexes[gActiveBattler]
                    && j != partnerPartyIdx
                    && j != alreadyChosenIdx) {
                    choice = j + 1;
                    break;
                }
            }
            break;
        }
        choice = atoi(line);
        /* Cancel / back (voluntary only) */
        if (!forced && choice == 0) {
            BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, PARTY_SIZE, gBattlePartyCurrentOrder);
            ConsoleBufferExecCompleted();
            return;
        }
        if (choice >= 1 && choice <= PARTY_SIZE) {
            u16 sp = GetMonData(&party[choice - 1], MON_DATA_SPECIES, NULL);
            u16 hp = GetMonData(&party[choice - 1], MON_DATA_HP, NULL);
            if (sp != SPECIES_NONE && hp > 0
                && (choice - 1) != gBattlerPartyIndexes[gActiveBattler]
                && (choice - 1) != partnerPartyIdx
                && (choice - 1) != alreadyChosenIdx)
                break;
        }
        printf("Invalid choice (pick a healthy, non-active mon%s): ",
               forced ? "" : ", or 0 to go back");
        fflush(stdout);
        choice = -1;
    }

    BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, (u8)(choice - 1), gBattlePartyCurrentOrder);
    ConsoleBufferExecCompleted();
}

/* --- GETMONDATA / SETMONDATA ---
 * These are data-only operations; they work identically on desktop since
 * gPlayerParty and gEnemyParty are normal C arrays.
 */
static void ConsoleHandleGetMonData(void)
{
    u8 monData[sizeof(struct Pokemon) * 2 + 56];
    u32 size = 0;
    u8 monToCheck;
    s32 i;
    struct Pokemon *party = IsPlayerSide() ? gPlayerParty : gEnemyParty;

    if (gDebugMode)
        fprintf(stderr, "[GETMONDATA] battler=%d request=%d monToCheck=%d partyIdx=%d side=%s\n",
                gActiveBattler,
                gBattleBufferA[gActiveBattler][1],
                gBattleBufferA[gActiveBattler][2],
                gBattlerPartyIndexes[gActiveBattler],
                IsPlayerSide() ? "player" : "opponent");

    if (gBattleBufferA[gActiveBattler][2] == 0) {
        /* Single mon */
        if (IsPlayerSide())
            size = CopyPlayerMonData(gBattlerPartyIndexes[gActiveBattler], monData);
        else
            size = CopyOpponentMonData(gBattlerPartyIndexes[gActiveBattler], monData);
    } else {
        monToCheck = gBattleBufferA[gActiveBattler][2];
        for (i = 0; i < PARTY_SIZE; i++) {
            if (monToCheck & 1) {
                if (IsPlayerSide())
                    size += CopyPlayerMonData(i, monData + size);
                else
                    size += CopyOpponentMonData(i, monData + size);
            }
            monToCheck >>= 1;
        }
    }
    BtlController_EmitDataTransfer(B_COMM_TO_ENGINE, size, monData);
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleSetMonData(void)
{
    if (IsPlayerSide())
        SetPlayerMonData(gBattlerPartyIndexes[gActiveBattler]);
    else
        SetOpponentMonData(gBattlerPartyIndexes[gActiveBattler]);
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleGetRawMonData(void)
{
    struct Pokemon *party = IsPlayerSide() ? gPlayerParty : gEnemyParty;
    u8 monId = gBattleBufferA[gActiveBattler][1];
    u8 offset = gBattleBufferA[gActiveBattler][2];
    u8 size   = gBattleBufferA[gActiveBattler][3];
    BtlController_EmitDataTransfer(B_COMM_TO_ENGINE, size,
        (u8 *)(&party[monId]) + offset);
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleSetRawMonData(void)
{
    struct Pokemon *party = IsPlayerSide() ? gPlayerParty : gEnemyParty;
    u8 monId = gBattleBufferA[gActiveBattler][1];
    u8 offset = gBattleBufferA[gActiveBattler][2];
    u8 size   = gBattleBufferA[gActiveBattler][3];
    memcpy((u8 *)(&party[monId]) + offset,
           &gBattleBufferA[gActiveBattler][4], size);
    ConsoleBufferExecCompleted();
}

/* --- HEALTHBARUPDATE ---
 * On desktop we print an HP update to stdout.
 */
static void ConsoleHandleHealthBarUpdate(void)
{
    ConsoleBufferExecCompleted();
}

/* --- EXPUPDATE --- */
static void ConsoleHandleExpUpdate(void)
{
    ConsoleBufferExecCompleted();
}

/* --- TWORETURNVALUES / ONERETURNVALUE --- */
static void ConsoleHandleTwoReturnValues(void)
{
    /* The engine placed data in gBattleBufferA; controller writes to gBattleBufferB */
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    gBattleBufferB[gActiveBattler][1] = gBattleBufferA[gActiveBattler][2];
    gBattleBufferB[gActiveBattler][2] = gBattleBufferA[gActiveBattler][3];
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleOneReturnValue(void)
{
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    gBattleBufferB[gActiveBattler][1] = gBattleBufferA[gActiveBattler][2];
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleChosenMonReturnValue(void)
{
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    ConsoleBufferExecCompleted();
}

/* --- DATA TRANSFER --- */
static void ConsoleHandleDataTransfer(void)
{
    /* Data was sent from engine to controller (or v.v.) via the buffer.
       On single-player desktop there's nothing to actually transfer. */
    ConsoleBufferExecCompleted();
}

/* --- YESNOBOX --- */
static void ConsoleHandleYesNoBox(void)
{
    if (IsPlayerSide()) {
        printf("Yes or No? (1=Yes, 2=No): ");
        fflush(stdout);
        char line[32];
        int choice = 2;
        if (fgets(line, sizeof(line), stdin)) choice = atoi(line);
        gBattleBufferB[gActiveBattler][0] = (choice == 1) ? 0 : 1;
    } else {
        gBattleBufferB[gActiveBattler][0] = 1; /* No */
    }
    ConsoleBufferExecCompleted();
}

/* --- FAINTANIMATION ---
 * Print a "Mon fainted" style note.
 */
static void ConsoleHandleFaintAnimation(void)
{
    ConsoleBufferExecCompleted();
}

/* --- SWITCHINANIM / RETURNMONTOBALL ---
 * Print a notification.
 */
static void ConsoleHandleSwitchInAnim(void)
{
    ConsoleBufferExecCompleted();
}

static void ConsoleHandleReturnMonToBall(void)
{
    ConsoleBufferExecCompleted();
}

/* ===========================================================================
 * Command dispatch table
 * ===========================================================================
 */

static void (*const sConsoleBufferCommands[CONTROLLER_CMDS_COUNT])(void) =
{
    [CONTROLLER_GETMONDATA]               = ConsoleHandleGetMonData,
    [CONTROLLER_GETRAWMONDATA]            = ConsoleHandleGetRawMonData,
    [CONTROLLER_SETMONDATA]               = ConsoleHandleSetMonData,
    [CONTROLLER_SETRAWMONDATA]            = ConsoleHandleSetRawMonData,
    [CONTROLLER_LOADMONSPRITE]            = ConsoleBufferExecCompleted,
    [CONTROLLER_SWITCHINANIM]             = ConsoleHandleSwitchInAnim,
    [CONTROLLER_RETURNMONTOBALL]          = ConsoleHandleReturnMonToBall,
    [CONTROLLER_DRAWTRAINERPIC]           = ConsoleBufferExecCompleted,
    [CONTROLLER_TRAINERSLIDE]             = ConsoleBufferExecCompleted,
    [CONTROLLER_TRAINERSLIDEBACK]         = ConsoleBufferExecCompleted,
    [CONTROLLER_FAINTANIMATION]           = ConsoleHandleFaintAnimation,
    [CONTROLLER_PALETTEFADE]              = ConsoleBufferExecCompleted,
    [CONTROLLER_SUCCESSBALLTHROWANIM]     = ConsoleBufferExecCompleted,
    [CONTROLLER_BALLTHROWANIM]            = ConsoleBufferExecCompleted,
    [CONTROLLER_PAUSE]                    = ConsoleBufferExecCompleted,
    [CONTROLLER_MOVEANIMATION]            = ConsoleBufferExecCompleted,
    [CONTROLLER_PRINTSTRING]              = ConsoleHandlePrintString,
    [CONTROLLER_PRINTSTRINGPLAYERONLY]    = ConsoleHandlePrintSelectionString,
    [CONTROLLER_CHOOSEACTION]             = ConsoleHandleChooseAction,
    [CONTROLLER_YESNOBOX]                 = ConsoleHandleYesNoBox,
    [CONTROLLER_CHOOSEMOVE]               = ConsoleHandleChooseMove,
    [CONTROLLER_OPENBAG]                  = ConsoleBufferExecCompleted,
    [CONTROLLER_CHOOSEPOKEMON]            = ConsoleHandleChoosePokemon,
    [CONTROLLER_23]                       = ConsoleBufferExecCompleted,
    [CONTROLLER_HEALTHBARUPDATE]          = ConsoleHandleHealthBarUpdate,
    [CONTROLLER_EXPUPDATE]                = ConsoleHandleExpUpdate,
    [CONTROLLER_STATUSICONUPDATE]         = ConsoleBufferExecCompleted,
    [CONTROLLER_STATUSANIMATION]          = ConsoleBufferExecCompleted,
    [CONTROLLER_STATUSXOR]                = ConsoleBufferExecCompleted,
    [CONTROLLER_DATATRANSFER]             = ConsoleHandleDataTransfer,
    [CONTROLLER_DMA3TRANSFER]             = ConsoleBufferExecCompleted,
    [CONTROLLER_PLAYBGM]                  = ConsoleBufferExecCompleted,
    [CONTROLLER_32]                       = ConsoleBufferExecCompleted,
    [CONTROLLER_TWORETURNVALUES]          = ConsoleHandleTwoReturnValues,
    [CONTROLLER_CHOSENMONRETURNVALUE]     = ConsoleHandleChosenMonReturnValue,
    [CONTROLLER_ONERETURNVALUE]           = ConsoleHandleOneReturnValue,
    [CONTROLLER_ONERETURNVALUE_DUPLICATE] = ConsoleHandleOneReturnValue,
    [CONTROLLER_CLEARUNKVAR]              = ConsoleBufferExecCompleted,
    [CONTROLLER_SETUNKVAR]                = ConsoleBufferExecCompleted,
    [CONTROLLER_CLEARUNKFLAG]             = ConsoleBufferExecCompleted,
    [CONTROLLER_TOGGLEUNKFLAG]            = ConsoleBufferExecCompleted,
    [CONTROLLER_HITANIMATION]             = ConsoleBufferExecCompleted,
    [CONTROLLER_CANTSWITCH]               = ConsoleBufferExecCompleted,
    [CONTROLLER_PLAYSE]                   = ConsoleBufferExecCompleted,
    [CONTROLLER_PLAYFANFAREORBGM]         = ConsoleBufferExecCompleted,
    [CONTROLLER_FAINTINGCRY]              = ConsoleBufferExecCompleted,
    [CONTROLLER_INTROSLIDE]               = ConsoleBufferExecCompleted,
    [CONTROLLER_INTROTRAINERBALLTHROW]    = ConsoleBufferExecCompleted,
    [CONTROLLER_DRAWPARTYSTATUSSUMMARY]   = ConsoleBufferExecCompleted,
    [CONTROLLER_HIDEPARTYSTATUSSUMMARY]   = ConsoleBufferExecCompleted,
    [CONTROLLER_ENDBOUNCE]                = ConsoleBufferExecCompleted,
    [CONTROLLER_SPRITEINVISIBILITY]       = ConsoleBufferExecCompleted,
    [CONTROLLER_BATTLEANIMATION]          = ConsoleBufferExecCompleted,
    [CONTROLLER_LINKSTANDBYMSG]           = ConsoleBufferExecCompleted,
    [CONTROLLER_RESETACTIONMOVESELECTION] = ConsoleBufferExecCompleted,
    [CONTROLLER_ENDLINKBATTLE]            = ConsoleBufferExecCompleted,
    [CONTROLLER_TERMINATOR_NOP]           = ConsoleBufferExecCompleted,
};

static const char *sCmdNames[] = {
    "GETMONDATA","GETRAWMONDATA","SETMONDATA","SETRAWMONDATA","LOADMONSPRITE",
    "SWITCHINANIM","RETURNMONTOBALL","DRAWTRAINERPIC","TRAINERSLIDE","TRAINERSLIDEBACK",
    "FAINTANIMATION","PALETTEFADE","SUCCESSBALLTHROWANIM","BALLTHROWANIM","PAUSE",
    "MOVEANIMATION","PRINTSTRING","PRINTSTRINGPLAYERONLY","CHOOSEACTION","YESNOBOX",
    "CHOOSEMOVE","OPENBAG","CHOOSEPOKEMON","CMD23","HEALTHBARUPDATE","EXPUPDATE",
    "STATUSICONUPDATE","STATUSANIMATION","STATUSXOR","DATATRANSFER","DMA3TRANSFER",
    "PLAYBGM","CMD32","TWORETURNVALUES","CHOSENMONRETURNVALUE","ONERETURNVALUE",
    "ONERETURNVALUE_DUP","CLEARUNKVAR","SETUNKVAR","CLEARUNKFLAG","TOGGLEUNKFLAG",
    "HITANIMATION","CANTSWITCH","PLAYSE","PLAYFANFAREORBGM","FAINTINGCRY",
    "INTROSLIDE","INTROTRAINERBALLTHROW","DRAWPARTYSTATUSSUMMARY","HIDEPARTYSTATUSSUMMARY",
    "ENDBOUNCE","SPRITEINVISIBILITY","BATTLEANIMATION","LINKSTANDBYMSG",
    "RESETACTIONMOVESELECTION","ENDLINKBATTLE","TERMINATOR_NOP",
};

static void ConsoleBufferRunCommand(void)
{
    if (gBattleControllerExecFlags & gBitTable[gActiveBattler]) {
        /* In link mode, BtlController_Emit* routes data to gLinkBattleSendBuffer
         * instead of gBattleBufferA.  Copy the message for this battler now. */
        if (gBattleTypeFlags & BATTLE_TYPE_LINK)
            Desktop_CopyLinkMessageForBattler(gActiveBattler);

        u8 cmd = gBattleBufferA[gActiveBattler][0];
        if (gDebugMode) {
            const char *name = (cmd < ARRAY_COUNT(sCmdNames)) ? sCmdNames[cmd] : "UNKNOWN";
            fprintf(stderr, "[DBG] battler=%d cmd=%d(%s)\n", gActiveBattler, cmd, name);
            fflush(stderr);
        }
        if (cmd < ARRAY_COUNT(sConsoleBufferCommands))
            sConsoleBufferCommands[cmd]();
        else
            ConsoleBufferExecCompleted();
    }
}

/* ===========================================================================
 * Stubs for functions called outside the controller but only relevant to GBA
 * ===========================================================================
 */

void BattleControllerDummy(void) {}
void SetBattleEndCallbacks(void) {}
void SpriteCB_FreePlayerSpriteLoadMonSprite(struct Sprite *sprite) {}
void Task_PlayerController_RestoreBgmAfterCry(u8 taskId) {}
void ActionSelectionCreateCursorAt(u8 cursorPosition, u8 baseTileNum) {}
void ActionSelectionDestroyCursorAt(u8 cursorPosition) {}
void InitMoveSelectionsVarsAndStrings(void) {}
void SetControllerToRecordedPlayer(void) { SetControllerToConsole(); }
void SetControllerToPlayerPartner(void)  { SetControllerToConsole(); }
void SetControllerToSafari(void)         { SetControllerToConsole(); }
void SetControllerToWally(void)          { SetControllerToConsole(); }
void SetControllerToRecordedOpponent(void) { SetControllerToConsole(); }
void SetControllerToLinkOpponent(void)   { SetControllerToConsole(); }
void SetControllerToLinkPartner(void)    { SetControllerToConsole(); }

/* HandleGetRawMonData referenced in the original player controller header */
void PlayerHandleGetRawMonData(void) {}
