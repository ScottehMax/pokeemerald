/*
 * programmatic_controller.c - Non-blocking battle controller for ML/API use
 *
 * This is a variant of console_controller.c that, instead of blocking on
 * fgets() for human input, sets a "waiting" flag and returns control to
 * the caller (battle_step in battle_api.c). The caller inspects the
 * pending action request, then calls battle_submit_action() which writes
 * the decision into the engine's buffers and clears the waiting flag.
 *
 * On the next call to battle_step(), the controller re-enters, sees the
 * action has been provided, emits the appropriate return values, and
 * completes execution.
 *
 * Battlers NOT controlled programmatically still use the console controller
 * (with AI decision-making).
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
#include "battle_util.h"
#include "battle_desktop/battle_api.h"
#include <string.h>

/* =========================================================================
 * Shared state — read by battle_api.c
 * ========================================================================= */

/* Set to non-zero when a programmatic controller needs a decision.
 * battle_step() checks this after each frame. */
volatile int gProgrammaticWaiting = 0;

/* The pending request (what decision is needed) */
BattleActionRequest gPendingRequest;

/* Per-battler submitted action state (written by Programmatic_SubmitAction) */
static BattleAction sSubmittedAction[MAX_BATTLERS_COUNT];
static int sActionSubmitted[MAX_BATTLERS_COUNT] = {0};

/* Per-battler deferred action: when an agent picks a specific move or switch
 * at CHOOSE_ACTION time, we store it here so CHOOSE_MOVE / CHOOSE_POKEMON
 * can auto-resolve without yielding back to Python. */
static s8 sPendingMoveSlot[MAX_BATTLERS_COUNT];
static u8 sPendingTarget[MAX_BATTLERS_COUNT];
static s8 sPendingSwitchSlot[MAX_BATTLERS_COUNT];

/* =========================================================================
 * Forward declarations
 * ========================================================================= */
static void ProgBufferRunCommand(void);
static void ProgBufferExecCompleted(void);
static void ProgHandleChooseAction(void);
static void ProgHandleChooseMove(void);
static void ProgHandleChoosePokemon(void);
static void ProgHandlePrintString(void);
static void ProgHandleGetMonData(void);
static void ProgHandleSetMonData(void);
static void ProgHandleGetRawMonData(void);
static void ProgHandleSetRawMonData(void);
static void ProgHandleTwoReturnValues(void);
static void ProgHandleOneReturnValue(void);
static void ProgHandleChosenMonReturnValue(void);

/* from data_transfer.c */
extern u32 CopyPlayerMonData(u8 monId, u8 *dst);
extern u32 CopyOpponentMonData(u8 monId, u8 *dst);
extern void SetPlayerMonData(u8 monId);
extern void SetOpponentMonData(u8 monId);

/* from battle_api.c */
extern int ShouldControlBattler(u8 battler);

/* =========================================================================
 * Helpers
 * ========================================================================= */

static bool8 IsPlayerSideProg(void)
{
    return (GET_BATTLER_SIDE(gActiveBattler) == B_SIDE_PLAYER);
}

/* =========================================================================
 * Public: set a battler to use the programmatic controller
 * ========================================================================= */

void SetControllerToProgrammatic(void)
{
    gBattlerControllerFuncs[gActiveBattler] = ProgBufferRunCommand;
    if (gBattleControllerExecFlags & gBitTable[gActiveBattler])
        ProgBufferRunCommand();
}

/* =========================================================================
 * Public: submit an action from the API
 * ========================================================================= */

void Programmatic_SubmitAction(const BattleAction *action)
{
    u8 battler = gPendingRequest.battler;
    sSubmittedAction[battler] = *action;
    sActionSubmitted[battler] = 1;
    gProgrammaticWaiting = 0;
}

/* =========================================================================
 * Populate the pending request for CHOOSE_ACTION
 * ========================================================================= */

static void FillChooseActionRequest(void)
{
    memset(&gPendingRequest, 0, sizeof(gPendingRequest));
    gPendingRequest.type = BATTLE_REQUEST_ACTION;
    gPendingRequest.battler = gActiveBattler;
    gPendingRequest.side = GET_BATTLER_SIDE(gActiveBattler);
    gPendingRequest.canFight = 1; /* the engine wouldn't ask if we couldn't fight */

    /* Fill per-move availability (accounts for Disable, Taunt, Torment etc.) */
    u8 unusable = CheckMoveLimitations(gActiveBattler, 0, MOVE_LIMITATIONS_ALL);
    u8 allUnusable = 1;
    for (int i = 0; i < MAX_MON_MOVES; i++) {
        u16 move = gBattleMons[gActiveBattler].moves[i];
        gPendingRequest.availableMoves[i] = move;
        gPendingRequest.movePp[i]  = (unusable & (1 << i)) ? 0 : gBattleMons[gActiveBattler].pp[i];
        gPendingRequest.moveMaxPp[i] = (move != MOVE_NONE)
            ? CalculatePPWithBonus(move, gBattleMons[gActiveBattler].ppBonuses, i)
            : 0;
        if (move != MOVE_NONE && !(unusable & (1 << i)))
            allUnusable = 0;
    }
    /* When every move is unusable (0 PP, Disabled, etc.) the engine will
     * use Struggle.  Signal this so the Python mask still allows "fight". */
    gPendingRequest.canStruggle = allUnusable;

    /* Determine which party mons are available for switching */
    struct Pokemon *party = IsPlayerSideProg() ? gPlayerParty : gEnemyParty;
    s32 partnerPartyIdx = -1;
    if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
        u8 partnerPos = BATTLE_PARTNER(GetBattlerPosition(gActiveBattler));
        u8 partner = GetBattlerAtPosition(partnerPos);
        if (partner != gActiveBattler && !(gAbsentBattlerFlags & gBitTable[partner]))
            partnerPartyIdx = gBattlerPartyIndexes[partner];
    }

    gPendingRequest.numAlive = 0;
    for (int j = 0; j < PARTY_SIZE; j++) {
        u16 sp = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
        u16 hp = GetMonData(&party[j], MON_DATA_HP, NULL);
        if (sp != SPECIES_NONE && hp > 0
            && j != gBattlerPartyIndexes[gActiveBattler]
            && j != partnerPartyIdx) {
            gPendingRequest.canSwitch[j] = 1;
            gPendingRequest.numAlive++;
        }
    }
}

/* =========================================================================
 * Populate the pending request for CHOOSE_MOVE
 * ========================================================================= */

static void FillChooseMoveRequest(void)
{
    struct ChooseMoveStruct *moveInfo =
        (struct ChooseMoveStruct *)(&gBattleBufferA[gActiveBattler][4]);

    memset(&gPendingRequest, 0, sizeof(gPendingRequest));
    gPendingRequest.type = BATTLE_REQUEST_MOVE;
    gPendingRequest.battler = gActiveBattler;
    gPendingRequest.side = GET_BATTLER_SIDE(gActiveBattler);

    for (int i = 0; i < MAX_MON_MOVES; i++) {
        gPendingRequest.availableMoves[i] = moveInfo->moves[i];
        gPendingRequest.movePp[i] = moveInfo->currentPp[i];
        gPendingRequest.moveMaxPp[i] = moveInfo->maxPp[i];
    }
}

/* =========================================================================
 * Populate the pending request for CHOOSE_POKEMON
 * ========================================================================= */

static void FillChoosePokemonRequest(void)
{
    memset(&gPendingRequest, 0, sizeof(gPendingRequest));
    gPendingRequest.type = BATTLE_REQUEST_SWITCH;
    gPendingRequest.battler = gActiveBattler;
    gPendingRequest.side = GET_BATTLER_SIDE(gActiveBattler);

    u8 caseId = gBattleBufferA[gActiveBattler][1] & 0xF;
    gPendingRequest.forced = (caseId == PARTY_ACTION_SEND_OUT);

    struct Pokemon *party = IsPlayerSideProg() ? gPlayerParty : gEnemyParty;
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

    gPendingRequest.numAlive = 0;
    for (int j = 0; j < PARTY_SIZE; j++) {
        u16 sp = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
        u16 hp = GetMonData(&party[j], MON_DATA_HP, NULL);
        if (sp != SPECIES_NONE && hp > 0
            && j != gBattlerPartyIndexes[gActiveBattler]
            && j != partnerPartyIdx
            && j != alreadyChosenIdx) {
            gPendingRequest.canSwitch[j] = 1;
            gPendingRequest.numAlive++;
        }
    }
}

/* =========================================================================
 * Command handlers
 * ========================================================================= */

static void ProgHandleChooseAction(void)
{
    /* If action already submitted for this battler, process it */
    if (sActionSubmitted[gActiveBattler]) {
        sActionSubmitted[gActiveBattler] = 0;
        BattleAction *sa = &sSubmittedAction[gActiveBattler];

        if (sa->type == BATTLE_ACTION_SWITCH) {
            /* Store switch slot for CHOOSE_POKEMON auto-resolution */
            sPendingSwitchSlot[gActiveBattler] = sa->switchSlot;
            sPendingMoveSlot[gActiveBattler]   = -1;
            BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, B_ACTION_SWITCH, 0);
        } else {
            /* Store move slot for CHOOSE_MOVE auto-resolution */
            sPendingMoveSlot[gActiveBattler]   = sa->moveSlot;
            sPendingTarget[gActiveBattler]     = sa->target;
            sPendingSwitchSlot[gActiveBattler] = -1;
            BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, B_ACTION_USE_MOVE, 0);
        }
        ProgBufferExecCompleted();
        return;
    }

    /* Clear any stale pending state */
    sPendingMoveSlot[gActiveBattler]   = -1;
    sPendingSwitchSlot[gActiveBattler] = -1;

    /* Only claim the pending request if no other battler already has it */
    if (!gProgrammaticWaiting) {
        FillChooseActionRequest();
        gProgrammaticWaiting = 1;
    }
    /* Do NOT call ProgBufferExecCompleted — leave exec flags set.
     * The controller will be re-entered next frame. */
}

static void ProgHandleChooseMove(void)
{
    struct ChooseMoveStruct *moveInfo =
        (struct ChooseMoveStruct *)(&gBattleBufferA[gActiveBattler][4]);

    /* ---- Auto-Struggle (no PP left at all) ---- */
    u8 hasValidMove = 0;
    for (int i = 0; i < MAX_MON_MOVES; i++) {
        if (moveInfo->moves[i] != MOVE_NONE && moveInfo->currentPp[i] > 0) {
            hasValidMove = 1;
            break;
        }
    }
    if (!hasValidMove) {
        sPendingMoveSlot[gActiveBattler] = -1;
        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
            (MOVE_STRUGGLE) | (GetBattlerAtPosition(
                BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler))) << 8));
        ProgBufferExecCompleted();
        return;
    }

    /* ---- Auto-resolve if move was chosen at CHOOSE_ACTION time ---- */
    if (sPendingMoveSlot[gActiveBattler] >= 0) {
        u8 moveSlot = (u8)sPendingMoveSlot[gActiveBattler];
        u8 target   = sPendingTarget[gActiveBattler];
        sPendingMoveSlot[gActiveBattler] = -1;

        /* Validate — the move could have become unusable since CHOOSE_ACTION */
        if (moveSlot >= MAX_MON_MOVES
            || moveInfo->moves[moveSlot] == MOVE_NONE
            || moveInfo->currentPp[moveSlot] == 0) {
            /* Fall back to first usable move */
            for (int i = 0; i < MAX_MON_MOVES; i++) {
                if (moveInfo->moves[i] != MOVE_NONE && moveInfo->currentPp[i] > 0) {
                    moveSlot = i;
                    break;
                }
            }
        }

        u16 moveTgt = gBattleMoves[moveInfo->moves[moveSlot]].target;
        if (moveTgt & MOVE_TARGET_USER)
            target = gActiveBattler;
        else if (!(gBattleTypeFlags & BATTLE_TYPE_DOUBLE))
            target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
        else if (target >= gBattlersCount)
            target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));

        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10,
            (moveSlot) | (target << 8));
        ProgBufferExecCompleted();
        return;
    }

    /* ---- Fallback: yield to Python (shouldn't normally happen) ---- */
    if (sActionSubmitted[gActiveBattler]) {
        sActionSubmitted[gActiveBattler] = 0;
        u8 moveSlot = sSubmittedAction[gActiveBattler].moveSlot;
        if (moveSlot >= MAX_MON_MOVES || moveInfo->moves[moveSlot] == MOVE_NONE)
            moveSlot = 0;
        u8 target;
        u16 moveTgt = gBattleMoves[moveInfo->moves[moveSlot]].target;
        if (moveTgt & MOVE_TARGET_USER)
            target = gActiveBattler;
        else if (gBattleTypeFlags & BATTLE_TYPE_DOUBLE) {
            target = sSubmittedAction[gActiveBattler].target;
            if (target >= gBattlersCount)
                target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
        } else
            target = GetBattlerAtPosition(BATTLE_OPPOSITE(GetBattlerPosition(gActiveBattler)));
        BtlController_EmitTwoReturnValues(B_COMM_TO_ENGINE, 10, (moveSlot) | (target << 8));
        ProgBufferExecCompleted();
        return;
    }

    if (!gProgrammaticWaiting) {
        FillChooseMoveRequest();
        gProgrammaticWaiting = 1;
    }
}

static void ProgHandleChoosePokemon(void)
{
    s32 i;
    for (i = 0; i < (int)ARRAY_COUNT(gBattlePartyCurrentOrder); i++)
        gBattlePartyCurrentOrder[i] = gBattleBufferA[gActiveBattler][4 + i];

    /* Store fields for downstream code */
    *(&gBattleStruct->battlerPreventingSwitchout) = gBattleBufferA[gActiveBattler][1] >> 4;
    *(&gBattleStruct->prevSelectedPartySlot)      = gBattleBufferA[gActiveBattler][2];
    *(&gBattleStruct->abilityPreventingSwitchout) = gBattleBufferA[gActiveBattler][3];

    u8 caseId = gBattleBufferA[gActiveBattler][1] & 0xF;

    /* Can't switch (trapped) — auto-cancel */
    if (caseId == PARTY_ACTION_CANT_SWITCH || caseId == PARTY_ACTION_ABILITY_PREVENTS) {
        sPendingSwitchSlot[gActiveBattler] = -1;
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, PARTY_SIZE, gBattlePartyCurrentOrder);
        ProgBufferExecCompleted();
        return;
    }

    /* ---- Auto-resolve if switch was chosen at CHOOSE_ACTION time ---- */
    if (sPendingSwitchSlot[gActiveBattler] >= 0) {
        u8 slot = (u8)sPendingSwitchSlot[gActiveBattler];
        sPendingSwitchSlot[gActiveBattler] = -1;
        if (slot >= PARTY_SIZE) slot = 0;
        *(gBattleStruct->monToSwitchIntoId + gActiveBattler) = slot;
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, slot, gBattlePartyCurrentOrder);
        ProgBufferExecCompleted();
        return;
    }

    /* ---- Forced switch (e.g. faint) — yield to Python ---- */
    if (sActionSubmitted[gActiveBattler]) {
        sActionSubmitted[gActiveBattler] = 0;
        u8 slot = sSubmittedAction[gActiveBattler].switchSlot;
        if (slot >= PARTY_SIZE)
            slot = 0;
        *(gBattleStruct->monToSwitchIntoId + gActiveBattler) = slot;
        BtlController_EmitChosenMonReturnValue(B_COMM_TO_ENGINE, slot, gBattlePartyCurrentOrder);
        ProgBufferExecCompleted();
        return;
    }

    if (!gProgrammaticWaiting) {
        FillChoosePokemonRequest();
        gProgrammaticWaiting = 1;
    }
}

static void ProgHandlePrintString(void)
{
    u16 stringId = *(u16 *)(&gBattleBufferA[gActiveBattler][2]);
    BufferStringBattle(stringId);
    /* In API mode, we silently discard the string (no stdout) */
    ProgBufferExecCompleted();
}

static void ProgHandleGetMonData(void)
{
    u8 monData[sizeof(struct Pokemon) * 2 + 56];
    u32 size = 0;
    u8 monToCheck;
    s32 i;

    if (gBattleBufferA[gActiveBattler][2] == 0) {
        if (IsPlayerSideProg())
            size = CopyPlayerMonData(gBattlerPartyIndexes[gActiveBattler], monData);
        else
            size = CopyOpponentMonData(gBattlerPartyIndexes[gActiveBattler], monData);
    } else {
        monToCheck = gBattleBufferA[gActiveBattler][2];
        for (i = 0; i < PARTY_SIZE; i++) {
            if (monToCheck & 1) {
                if (IsPlayerSideProg())
                    size += CopyPlayerMonData(i, monData + size);
                else
                    size += CopyOpponentMonData(i, monData + size);
            }
            monToCheck >>= 1;
        }
    }
    BtlController_EmitDataTransfer(B_COMM_TO_ENGINE, size, monData);
    ProgBufferExecCompleted();
}

static void ProgHandleSetMonData(void)
{
    if (IsPlayerSideProg())
        SetPlayerMonData(gBattlerPartyIndexes[gActiveBattler]);
    else
        SetOpponentMonData(gBattlerPartyIndexes[gActiveBattler]);
    ProgBufferExecCompleted();
}

static void ProgHandleGetRawMonData(void)
{
    struct Pokemon *party = IsPlayerSideProg() ? gPlayerParty : gEnemyParty;
    u8 monId = gBattleBufferA[gActiveBattler][1];
    u8 offset = gBattleBufferA[gActiveBattler][2];
    u8 size   = gBattleBufferA[gActiveBattler][3];
    BtlController_EmitDataTransfer(B_COMM_TO_ENGINE, size,
        (u8 *)(&party[monId]) + offset);
    ProgBufferExecCompleted();
}

static void ProgHandleSetRawMonData(void)
{
    struct Pokemon *party = IsPlayerSideProg() ? gPlayerParty : gEnemyParty;
    u8 monId = gBattleBufferA[gActiveBattler][1];
    u8 offset = gBattleBufferA[gActiveBattler][2];
    u8 size   = gBattleBufferA[gActiveBattler][3];
    memcpy((u8 *)(&party[monId]) + offset,
           &gBattleBufferA[gActiveBattler][4], size);
    ProgBufferExecCompleted();
}

static void ProgHandleTwoReturnValues(void)
{
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    gBattleBufferB[gActiveBattler][1] = gBattleBufferA[gActiveBattler][2];
    gBattleBufferB[gActiveBattler][2] = gBattleBufferA[gActiveBattler][3];
    ProgBufferExecCompleted();
}

static void ProgHandleOneReturnValue(void)
{
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    gBattleBufferB[gActiveBattler][1] = gBattleBufferA[gActiveBattler][2];
    ProgBufferExecCompleted();
}

static void ProgHandleChosenMonReturnValue(void)
{
    gBattleBufferB[gActiveBattler][0] = gBattleBufferA[gActiveBattler][1];
    ProgBufferExecCompleted();
}

/* =========================================================================
 * Completion
 * ========================================================================= */

static void ProgBufferExecCompleted(void)
{
    gBattlerControllerFuncs[gActiveBattler] = ProgBufferRunCommand;
    gBattleControllerExecFlags &= ~gBitTable[gActiveBattler];
}

/* =========================================================================
 * Command dispatch table
 * ========================================================================= */

static void (*const sProgBufferCommands[CONTROLLER_CMDS_COUNT])(void) =
{
    [CONTROLLER_GETMONDATA]               = ProgHandleGetMonData,
    [CONTROLLER_GETRAWMONDATA]            = ProgHandleGetRawMonData,
    [CONTROLLER_SETMONDATA]               = ProgHandleSetMonData,
    [CONTROLLER_SETRAWMONDATA]            = ProgHandleSetRawMonData,
    [CONTROLLER_LOADMONSPRITE]            = ProgBufferExecCompleted,
    [CONTROLLER_SWITCHINANIM]             = ProgBufferExecCompleted,
    [CONTROLLER_RETURNMONTOBALL]          = ProgBufferExecCompleted,
    [CONTROLLER_DRAWTRAINERPIC]           = ProgBufferExecCompleted,
    [CONTROLLER_TRAINERSLIDE]             = ProgBufferExecCompleted,
    [CONTROLLER_TRAINERSLIDEBACK]         = ProgBufferExecCompleted,
    [CONTROLLER_FAINTANIMATION]           = ProgBufferExecCompleted,
    [CONTROLLER_PALETTEFADE]              = ProgBufferExecCompleted,
    [CONTROLLER_SUCCESSBALLTHROWANIM]     = ProgBufferExecCompleted,
    [CONTROLLER_BALLTHROWANIM]            = ProgBufferExecCompleted,
    [CONTROLLER_PAUSE]                    = ProgBufferExecCompleted,
    [CONTROLLER_MOVEANIMATION]            = ProgBufferExecCompleted,
    [CONTROLLER_PRINTSTRING]              = ProgHandlePrintString,
    [CONTROLLER_PRINTSTRINGPLAYERONLY]    = ProgBufferExecCompleted,
    [CONTROLLER_CHOOSEACTION]             = ProgHandleChooseAction,
    [CONTROLLER_YESNOBOX]                 = ProgBufferExecCompleted,
    [CONTROLLER_CHOOSEMOVE]               = ProgHandleChooseMove,
    [CONTROLLER_OPENBAG]                  = ProgBufferExecCompleted,
    [CONTROLLER_CHOOSEPOKEMON]            = ProgHandleChoosePokemon,
    [CONTROLLER_23]                       = ProgBufferExecCompleted,
    [CONTROLLER_HEALTHBARUPDATE]          = ProgBufferExecCompleted,
    [CONTROLLER_EXPUPDATE]                = ProgBufferExecCompleted,
    [CONTROLLER_STATUSICONUPDATE]         = ProgBufferExecCompleted,
    [CONTROLLER_STATUSANIMATION]          = ProgBufferExecCompleted,
    [CONTROLLER_STATUSXOR]                = ProgBufferExecCompleted,
    [CONTROLLER_DATATRANSFER]             = ProgBufferExecCompleted,
    [CONTROLLER_DMA3TRANSFER]             = ProgBufferExecCompleted,
    [CONTROLLER_PLAYBGM]                  = ProgBufferExecCompleted,
    [CONTROLLER_32]                       = ProgBufferExecCompleted,
    [CONTROLLER_TWORETURNVALUES]          = ProgHandleTwoReturnValues,
    [CONTROLLER_CHOSENMONRETURNVALUE]     = ProgHandleChosenMonReturnValue,
    [CONTROLLER_ONERETURNVALUE]           = ProgHandleOneReturnValue,
    [CONTROLLER_ONERETURNVALUE_DUPLICATE] = ProgHandleOneReturnValue,
    [CONTROLLER_CLEARUNKVAR]              = ProgBufferExecCompleted,
    [CONTROLLER_SETUNKVAR]                = ProgBufferExecCompleted,
    [CONTROLLER_CLEARUNKFLAG]             = ProgBufferExecCompleted,
    [CONTROLLER_TOGGLEUNKFLAG]            = ProgBufferExecCompleted,
    [CONTROLLER_HITANIMATION]             = ProgBufferExecCompleted,
    [CONTROLLER_CANTSWITCH]               = ProgBufferExecCompleted,
    [CONTROLLER_PLAYSE]                   = ProgBufferExecCompleted,
    [CONTROLLER_PLAYFANFAREORBGM]         = ProgBufferExecCompleted,
    [CONTROLLER_FAINTINGCRY]              = ProgBufferExecCompleted,
    [CONTROLLER_INTROSLIDE]               = ProgBufferExecCompleted,
    [CONTROLLER_INTROTRAINERBALLTHROW]    = ProgBufferExecCompleted,
    [CONTROLLER_DRAWPARTYSTATUSSUMMARY]   = ProgBufferExecCompleted,
    [CONTROLLER_HIDEPARTYSTATUSSUMMARY]   = ProgBufferExecCompleted,
    [CONTROLLER_ENDBOUNCE]                = ProgBufferExecCompleted,
    [CONTROLLER_SPRITEINVISIBILITY]       = ProgBufferExecCompleted,
    [CONTROLLER_BATTLEANIMATION]          = ProgBufferExecCompleted,
    [CONTROLLER_LINKSTANDBYMSG]           = ProgBufferExecCompleted,
    [CONTROLLER_RESETACTIONMOVESELECTION] = ProgBufferExecCompleted,
    [CONTROLLER_ENDLINKBATTLE]            = ProgBufferExecCompleted,
    [CONTROLLER_TERMINATOR_NOP]           = ProgBufferExecCompleted,
};

/* =========================================================================
 * Main dispatch
 * ========================================================================= */

static void ProgBufferRunCommand(void)
{
    if (gBattleControllerExecFlags & gBitTable[gActiveBattler]) {
        if (gBattleTypeFlags & BATTLE_TYPE_LINK) {
            extern void Desktop_CopyLinkMessageForBattler(u8 battler);
            Desktop_CopyLinkMessageForBattler(gActiveBattler);
        }

        u8 cmd = gBattleBufferA[gActiveBattler][0];
        if (cmd < ARRAY_COUNT(sProgBufferCommands))
            sProgBufferCommands[cmd]();
        else
            ProgBufferExecCompleted();
    }
}
