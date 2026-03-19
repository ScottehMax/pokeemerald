/*
 * battle_api.c - Implementation of the public C API for ML/RL use
 *
 * This module wraps the battle engine lifecycle (init, step, observe, act)
 * and coordinates with the programmatic controller to provide a yield/resume
 * interface suitable for calling from Python via ctypes.
 */

#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "battle_desktop/generated/battle_scripts.h"
#include "battle_desktop/team_parser.h"
#include "battle_desktop/battle_api.h"
#include "pokemon.h"
#include "random.h"
#include "task.h"
#include "malloc.h"
#include "link.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/items.h"
#include "constants/abilities.h"
#include "constants/characters.h"
#include "constants/trainers.h"
#include "constants/battle_script_commands.h"
#include "gba/io_reg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Externs from console_controller.c / programmatic_controller.c
 * ========================================================================= */
extern bool8 gDebugMode;
extern bool8 gPvpMode;
extern bool8 gBothAiMode;
extern void SetControllerToConsole(void);
extern void Desktop_ResetLinkSendBuffer(void);

/* From programmatic_controller.c */
extern void SetControllerToProgrammatic(void);
extern volatile int  gProgrammaticWaiting;
extern BattleActionRequest gPendingRequest;
extern void Programmatic_SubmitAction(const BattleAction *action);

/* =========================================================================
 * Module state
 * ========================================================================= */
static BattleConfig sConfig;
static int  sInitialised = 0;
static int  sBattleStarted = 0;

/* Exposed to console_controller for gating log output */
int gBattleVerbose = 0;

/* Forward declarations */
static void InitSaveBlockForApi(void);
int ShouldControlBattler(u8 battler);
void HandleYesNoBoxIfPendingApi(void);

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

BATTLE_API void battle_init(uint32_t seed)
{
    sInitialised = 1;
    sBattleStarted = 0;
    memset(&sConfig, 0, sizeof(sConfig));
    sConfig.controlSide[0] = 1; /* default: control player side */
    sConfig.controlSide[1] = 0; /* opponent uses AI */

    /* Suppress console output in library mode */
    gDebugMode = FALSE;
    gPvpMode = FALSE;
    gBothAiMode = FALSE;

    /* Seed RNG */
    srand(seed);
    SeedRng(seed & 0xFFFF);

    /* Init battle script variable address table */
    InitBattleScriptVarTable();

    /* Init save block */
    InitSaveBlockForApi();

    /* Init task system */
    ResetTasks();

    /* Zero parties */
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
}

BATTLE_API void battle_free(void)
{
    if (sBattleStarted) {
        FreeBattleResources();
        FreeBattleSpritesData();
        sBattleStarted = 0;
    }
    sInitialised = 0;
}

BATTLE_API void battle_reset(uint32_t seed)
{
    /* If first call, do full init instead of just reset */
    if (!sInitialised) {
        battle_init(seed);
        return;
    }

    if (sBattleStarted) {
        FreeBattleResources();
        FreeBattleSpritesData();
    }
    sBattleStarted = 0;

    /* Re-seed */
    srand(seed);
    SeedRng(seed & 0xFFFF);

    ResetTasks();
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
}

/* =========================================================================
 * Configuration
 * ========================================================================= */

BATTLE_API void battle_configure(const BattleConfig *config)
{
    sConfig = *config;
    gBattleVerbose = config->verbose;
}

/* =========================================================================
 * Team setup
 * ========================================================================= */

BATTLE_API int battle_set_team_showdown(int side, const char *text)
{
    struct Pokemon *party = (side == 0) ? gPlayerParty : gEnemyParty;
    u8 otIdType = (side == 0) ? OT_ID_PLAYER_ID : OT_ID_RANDOM_NO_SHINY;

    /* ParseTeamFile reads from a file, so we write to a temp file.
     * A future improvement would add a ParseTeamString function. */
    const char *tmpPath = (side == 0) ? "_tmp_team_p1.txt" : "_tmp_team_p2.txt";
    FILE *f = fopen(tmpPath, "w");
    if (!f) return 0;
    fputs(text, f);
    fclose(f);

    int result = ParseTeamFile(tmpPath, party, otIdType);
    remove(tmpPath);
    return result;
}

/* Defined in team_randomiser.c */
extern void RandomiseTeam(struct Pokemon *party, u8 otIdType, u32 seed);

BATTLE_API void battle_set_team_random(int side, uint32_t seed)
{
    struct Pokemon *party = (side == 0) ? gPlayerParty : gEnemyParty;
    u8 otIdType = (side == 0) ? OT_ID_PLAYER_ID : OT_ID_RANDOM_NO_SHINY;
    RandomiseTeam(party, otIdType, seed);
}

/* =========================================================================
 * Battle start
 * ========================================================================= */

BATTLE_API void battle_start(void)
{
    gBattleOutcome = 0;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    if (sConfig.doubles)
        gBattleTypeFlags |= BATTLE_TYPE_DOUBLE;

    gTrainerBattleOpponent_A = 1;
    gPartnerTrainerId = 0;

    AllocateBattleResources();
    AllocateBattleSpritesData();
    SetUpBattleVarsAndBirchZigzagoon();

    if (sConfig.doubles) {
        gBattlersCount = 4;
        gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
        gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
        gBattlerPositions[2] = B_POSITION_PLAYER_RIGHT;
        gBattlerPositions[3] = B_POSITION_OPPONENT_RIGHT;
        gBattlerPartyIndexes[0] = 0;
        gBattlerPartyIndexes[1] = 0;
        gBattlerPartyIndexes[2] = 1;
        gBattlerPartyIndexes[3] = 1;
    } else {
        gBattlersCount = 2;
        gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
        gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
        gBattlerPartyIndexes[0] = 0;
        gBattlerPartyIndexes[1] = 0;
    }

    /* InitBattleControllers assigns SetControllerToPlayer / SetControllerToOpponent
     * as bootstrap functions. In the API build, those check ShouldControlBattler()
     * and route to either the programmatic or console controller. */
    InitBattleControllers();
    gBattleMainFunc = BeginBattleIntro;
    sBattleStarted = 1;
    gProgrammaticWaiting = 0;
}

/* =========================================================================
 * Stepping
 * ========================================================================= */

BATTLE_API int battle_step(void)
{
    u32 frameLimit = 10000; /* safety per step call */

    while (frameLimit-- > 0) {
        if (gBattleOutcome != 0)
            return BATTLE_STEP_DONE;

        /* Run one frame of the battle state machine */
        gBattleMainFunc();

        /* Check yes/no box (the script polls buttons directly) */
        HandleYesNoBoxIfPendingApi();

        /* Run each battler's controller */
        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
            gBattlerControllerFuncs[gActiveBattler]();

        if (gBattleTypeFlags & BATTLE_TYPE_LINK)
            Desktop_ResetLinkSendBuffer();

        RunTasks();

        /* Check if a programmatic controller is waiting for input */
        if (gProgrammaticWaiting)
            return BATTLE_STEP_DECIDE;
    }

    /* Ran too many frames without decision or end — shouldn't happen */
    return BATTLE_STEP_DONE;
}

/* =========================================================================
 * Observation
 * ========================================================================= */

BATTLE_API void battle_get_state(BattleState *out)
{
    memset(out, 0, sizeof(*out));

    out->weather = gBattleWeather;
    out->turn = gBattleResults.battleTurnCounter;
    out->battlerCount = gBattlersCount;
    out->outcome = gBattleOutcome;

    /* Active battlers */
    for (int i = 0; i < gBattlersCount; i++) {
        BattleActiveMon *a = &out->active[i];
        struct BattlePokemon *m = &gBattleMons[i];

        a->species   = m->species;
        a->hp        = m->hp;
        a->maxHp     = m->maxHP;
        a->attack    = m->attack;
        a->defense   = m->defense;
        a->speed     = m->speed;
        a->spAttack  = m->spAttack;
        a->spDefense = m->spDefense;
        a->status1   = m->status1;
        a->status2   = m->status2;
        a->ability   = m->ability;
        a->item      = m->item;
        a->level     = m->level;
        a->types[0]  = m->types[0];
        a->types[1]  = m->types[1];
        a->isAlive   = (m->hp > 0 && m->species != SPECIES_NONE) ? 1 : 0;

        for (int j = 0; j < BATTLE_MAX_MOVES; j++) {
            a->moves[j] = m->moves[j];
            a->pp[j]    = m->pp[j];
        }
        for (int j = 0; j < BATTLE_NUM_STATS; j++)
            a->statStages[j] = m->statStages[j];
    }

    /* Party data */
    for (int side = 0; side < BATTLE_NUM_SIDES; side++) {
        struct Pokemon *party = (side == 0) ? gPlayerParty : gEnemyParty;
        for (int j = 0; j < BATTLE_PARTY_SIZE; j++) {
            BattlePartyMon *p = &out->party[side][j];
            p->species = GetMonData(&party[j], MON_DATA_SPECIES, NULL);
            p->hp      = GetMonData(&party[j], MON_DATA_HP, NULL);
            p->maxHp   = GetMonData(&party[j], MON_DATA_MAX_HP, NULL);
            p->item    = GetMonData(&party[j], MON_DATA_HELD_ITEM, NULL);
            p->status  = GetMonData(&party[j], MON_DATA_STATUS, NULL);
            p->level   = GetMonData(&party[j], MON_DATA_LEVEL, NULL);
            p->ability = GetMonData(&party[j], MON_DATA_ABILITY_NUM, NULL);
            p->isAlive = (p->species != SPECIES_NONE && p->hp > 0) ? 1 : 0;

            p->moves[0] = GetMonData(&party[j], MON_DATA_MOVE1, NULL);
            p->moves[1] = GetMonData(&party[j], MON_DATA_MOVE2, NULL);
            p->moves[2] = GetMonData(&party[j], MON_DATA_MOVE3, NULL);
            p->moves[3] = GetMonData(&party[j], MON_DATA_MOVE4, NULL);
            p->pp[0]    = GetMonData(&party[j], MON_DATA_PP1, NULL);
            p->pp[1]    = GetMonData(&party[j], MON_DATA_PP2, NULL);
            p->pp[2]    = GetMonData(&party[j], MON_DATA_PP3, NULL);
            p->pp[3]    = GetMonData(&party[j], MON_DATA_PP4, NULL);

            /* Types from species info — party mons don't store them directly */
            if (p->species != SPECIES_NONE && p->species < NUM_SPECIES) {
                p->types[0] = gSpeciesInfo[p->species].types[0];
                p->types[1] = gSpeciesInfo[p->species].types[1];
            }
        }
    }

    /* Side status */
    for (int side = 0; side < BATTLE_NUM_SIDES; side++) {
        BattleSideState *s = &out->sides[side];
        s->sideStatus       = gSideStatuses[side];
        s->reflectTimer     = gSideTimers[side].reflectTimer;
        s->lightscreenTimer = gSideTimers[side].lightscreenTimer;
        s->mistTimer        = gSideTimers[side].mistTimer;
        s->safeguardTimer   = gSideTimers[side].safeguardTimer;
        s->spikesAmount     = gSideTimers[side].spikesAmount;
    }
}

BATTLE_API void battle_get_action_request(BattleActionRequest *out)
{
    *out = gPendingRequest;
}

BATTLE_API int battle_get_outcome(void)
{
    return gBattleOutcome;
}

/* =========================================================================
 * Action submission
 * ========================================================================= */

BATTLE_API void battle_submit_action(const BattleAction *action)
{
    Programmatic_SubmitAction(action);
}

/* =========================================================================
 * Helpers
 * ========================================================================= */

/* Determine if a given battler should be programmatically controlled */
int ShouldControlBattler(u8 battler)
{
    u8 side = GET_BATTLER_SIDE(battler);
    return sConfig.controlSide[side];
}

static void InitSaveBlockForApi(void)
{
    memset(gSaveBlock1Ptr, 0, sizeof(*gSaveBlock1Ptr));
    memset(gSaveBlock2Ptr, 0, sizeof(*gSaveBlock2Ptr));

    gSaveBlock2Ptr->playerName[0] = 0xCC; /* R */
    gSaveBlock2Ptr->playerName[1] = 0xBF; /* E */
    gSaveBlock2Ptr->playerName[2] = 0xBE; /* D */
    gSaveBlock2Ptr->playerName[3] = 0xFF; /* EOS */

    gSaveBlock2Ptr->playerGender = MALE;
    gSaveBlock2Ptr->playerTrainerId[0] = (12345 >> 0) & 0xFF;
    gSaveBlock2Ptr->playerTrainerId[1] = (12345 >> 8) & 0xFF;
    gSaveBlock2Ptr->playerTrainerId[2] = 0;
    gSaveBlock2Ptr->playerTrainerId[3] = 0;

    gSaveBlock2Ptr->optionsBattleSceneOff = FALSE;
    gSaveBlock2Ptr->optionsBattleStyle    = OPTIONS_BATTLE_STYLE_SET; /* Set mode: no shift prompt */
}

/* Yes/No box handler for API mode:
 * If a programmatically-controlled side hits a yes/no prompt, we need to
 * handle it. In Set battle style there's no shift prompt, but we handle
 * it defensively. If it fires, auto-answer No. */
void HandleYesNoBoxIfPendingApi(void)
{
    if (gBattlescriptCurrInstr != NULL
        && *gBattlescriptCurrInstr == B_SCR_OP_YESNOBOX
        && gBattleCommunication[0] == 1)
    {
        /* Auto-answer No for ML mode (Set battle style means this shouldn't fire,
         * but if it does, declining is safe) */
        gMain.newKeys = B_BUTTON;
    }
}
