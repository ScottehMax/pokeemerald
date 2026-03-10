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
#include "constants/party_menu.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/characters.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
 * Helper: is this battler controlled by the player?
 * =========================================================================
 */

static bool8 IsPlayerSide(void)
{
    return (GET_BATTLER_SIDE(gActiveBattler) == B_SIDE_PLAYER);
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
    if (!IsPlayerSide()) {
        /* Opponent: use AI to decide */
        AI_TrySwitchOrUseItem();
        ConsoleBufferExecCompleted();
        return;
    }

    /* Player: show a text menu */
    printf("\n--- Choose Action ---\n");
    printf("  1. FIGHT\n");
    printf("  2. BAG\n");
    printf("  3. POKEMON\n");
    printf("  4. RUN\n");
    printf("Choice (1-4): ");
    fflush(stdout);

    int choice = 0;
    char line[64];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) { choice = 1; break; }
        choice = atoi(line);
        if (choice >= 1 && choice <= 4) break;
        printf("Invalid choice. Enter 1-4: ");
        fflush(stdout);
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

    if (!IsPlayerSide()) {
        /* Opponent: run AI */
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
            default: {
                u8 target = gBattlerTarget;
                u16 moveTgt = gBattleMoves[moveInfo->moves[chosenMoveId]].target;
                if (moveTgt & (MOVE_TARGET_USER_OR_SELECTED | MOVE_TARGET_USER))
                    target = gActiveBattler;
                else if (moveTgt & MOVE_TARGET_BOTH)
                    target = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
                BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
                    (chosenMoveId) | (target << 8));
                break;
            }
        }
        ConsoleBufferExecCompleted();
        return;
    }

    /* Player: display available moves */
    printf("\n--- Choose Move ---\n");
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
            (MOVE_STRUGGLE) | (GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT) << 8));
        ConsoleBufferExecCompleted();
        return;
    }

    printf("Choice (1-%d): ", MAX_MON_MOVES);
    fflush(stdout);

    int choice = 0;
    char line[64];
    while (1) {
        if (fgets(line, sizeof(line), stdin) == NULL) { choice = 1; break; }
        choice = atoi(line);
        if (choice >= 1 && choice <= MAX_MON_MOVES
                && moveInfo->moves[choice - 1] != MOVE_NONE) break;
        printf("Invalid move. Enter 1-%d: ", MAX_MON_MOVES);
        fflush(stdout);
    }

    u8 moveSlot = (u8)(choice - 1);
    u8 target;
    u16 moveTgt = gBattleMoves[moveInfo->moves[moveSlot]].target;
    if (moveTgt & (MOVE_TARGET_USER_OR_SELECTED | MOVE_TARGET_USER))
        target = gActiveBattler;
    else
        target = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);

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

    if (!IsPlayerSide()) {
        s32 chosenMonId;
        if (*(gBattleStruct->AI_monToSwitchIntoId + gActiveBattler) == PARTY_SIZE) {
            chosenMonId = GetMostSuitableMonToSwitchInto();
            if (chosenMonId == PARTY_SIZE) {
                /* Fallback: first alive non-active mon */
                chosenMonId = gBattlerPartyIndexes[gActiveBattler];
                for (int i = 0; i < PARTY_SIZE; i++) {
                    if (i != gBattlerPartyIndexes[gActiveBattler]
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

    struct Pokemon *party = gPlayerParty;

    /* Count valid (switchable) mons */
    int validCount = 0;
    for (int j = 0; j < PARTY_SIZE; j++) {
        u16 sp = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
        u16 hp = GetMonData(&party[j], MON_DATA_HP, NULL);
        if (sp != SPECIES_NONE && hp > 0 && j != gBattlerPartyIndexes[gActiveBattler])
            validCount++;
    }

    /* Voluntary switch with no valid targets — cancel immediately */
    if (!forced && validCount == 0) {
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, PARTY_SIZE, gBattlePartyCurrentOrder);
        ConsoleBufferExecCompleted();
        return;
    }

    /* Player: show party */
    printf("\n--- Choose Pokemon ---\n");
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
                if (sp != SPECIES_NONE && hp > 0 && j != gBattlerPartyIndexes[gActiveBattler]) {
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
            if (sp != SPECIES_NONE && hp > 0 && (choice - 1) != gBattlerPartyIndexes[gActiveBattler])
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

static void ConsoleBufferRunCommand(void)
{
    if (gBattleControllerExecFlags & gBitTable[gActiveBattler]) {
        u8 cmd = gBattleBufferA[gActiveBattler][0];
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
