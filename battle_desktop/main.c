/*
 * main.c - Desktop battle engine entry point
 *
 * Sets up two teams and runs a complete Pokémon battle using the authentic
 * Generation III battle engine extracted from the pokeemerald decomp.
 *
 * Usage:
 *   battle_desktop [player_team] [opponent_team]
 *   (no arguments uses the default teams defined below)
 *
 * How to configure teams:
 *   1. Call CreateMon() to create a Pokémon in the party array
 *   2. Use SetMonData() to set species, moves, EVs, IVs, etc.
 *   3. Call CalcLevel() and CalculateMonStats() to compute stats
 *
 * Battle types supported:
 *   BATTLE_TYPE_TRAINER - single trainer battle (AI opponent, player input)
 *   BATTLE_TYPE_WILD    - wild Pokémon encounter
 *
 * The battle runs fully in text mode; all graphical events are no-ops.
 */

#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "pokemon.h"
#include "random.h"
#include "task.h"
#include "malloc.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ===========================================================================
 * Forward declarations
 * =========================================================================== */

static void SetupPlayerTeam(void);
static void SetupOpponentTeam(void);
static void InitBattle(void);
static void RunBattleLoop(void);
static void PrintBattleResult(void);
static void InitSaveBlock(void);
extern void SetControllerToConsole(void);

/* ===========================================================================
 * Default team setup
 *
 * Modify these functions to configure the teams you want to battle.
 * Each call to CreateMon() creates a party Pokémon.
 * =========================================================================== */

static void SetupPlayerTeam(void)
{
    /* Slot 0: Blaziken level 50 — single-mon team for now so one faint ends the battle */
    CreateMon(&gPlayerParty[0], SPECIES_BLAZIKEN, 50, 15, FALSE, 0, OT_ID_PLAYER_ID, 0);
    {
        u16 move;
        move = MOVE_BLAZE_KICK;     SetMonData(&gPlayerParty[0], MON_DATA_MOVE1, &move);
        move = MOVE_BRICK_BREAK;    SetMonData(&gPlayerParty[0], MON_DATA_MOVE2, &move);
        move = MOVE_SLASH;          SetMonData(&gPlayerParty[0], MON_DATA_MOVE3, &move);
        move = MOVE_BULK_UP;        SetMonData(&gPlayerParty[0], MON_DATA_MOVE4, &move);
        u8 pp;
        pp = 8;  SetMonData(&gPlayerParty[0], MON_DATA_PP1, &pp);
        pp = 15; SetMonData(&gPlayerParty[0], MON_DATA_PP2, &pp);
        pp = 20; SetMonData(&gPlayerParty[0], MON_DATA_PP3, &pp);
        pp = 20; SetMonData(&gPlayerParty[0], MON_DATA_PP4, &pp);
    }
}

static void SetupOpponentTeam(void)
{
    /* Slot 0: Metagross level 50 — single-mon team so one faint ends the battle */
    CreateMon(&gEnemyParty[0], SPECIES_METAGROSS, 50, 15, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    {
        u16 move;
        move = MOVE_METEOR_MASH;    SetMonData(&gEnemyParty[0], MON_DATA_MOVE1, &move);
        move = MOVE_PSYCHIC;        SetMonData(&gEnemyParty[0], MON_DATA_MOVE2, &move);
        move = MOVE_EARTHQUAKE;     SetMonData(&gEnemyParty[0], MON_DATA_MOVE3, &move);
        move = MOVE_SHADOW_BALL;    SetMonData(&gEnemyParty[0], MON_DATA_MOVE4, &move);
        u8 pp;
        pp = 10; SetMonData(&gEnemyParty[0], MON_DATA_PP1, &pp);
        pp = 10; SetMonData(&gEnemyParty[0], MON_DATA_PP2, &pp);
        pp = 10; SetMonData(&gEnemyParty[0], MON_DATA_PP3, &pp);
        pp = 15; SetMonData(&gEnemyParty[0], MON_DATA_PP4, &pp);
    }
}

/* ===========================================================================
 * Save block initialization
 * =========================================================================== */

static void InitSaveBlock(void)
{
    memset(gSaveBlock1Ptr, 0, sizeof(*gSaveBlock1Ptr));
    memset(gSaveBlock2Ptr, 0, sizeof(*gSaveBlock2Ptr));

    /* Player name: "RED" in GF encoding (R=CC, E=BF, D=BE, EOS=FF) */
    gSaveBlock2Ptr->playerName[0] = 0xCC; /* R */
    gSaveBlock2Ptr->playerName[1] = 0xBF; /* E */
    gSaveBlock2Ptr->playerName[2] = 0xBE; /* D */
    gSaveBlock2Ptr->playerName[3] = 0xFF; /* EOS */

    gSaveBlock2Ptr->playerGender = MALE;
    gSaveBlock2Ptr->playerTrainerId[0] = (12345 >> 0) & 0xFF;
    gSaveBlock2Ptr->playerTrainerId[1] = (12345 >> 8) & 0xFF;
    gSaveBlock2Ptr->playerTrainerId[2] = 0;
    gSaveBlock2Ptr->playerTrainerId[3] = 0;

    /* Battle options: set animation ON, style = SHIFT */
    gSaveBlock2Ptr->optionsBattleSceneOff = FALSE;
    gSaveBlock2Ptr->optionsBattleStyle    = OPTIONS_BATTLE_STYLE_SHIFT;
}

/* ===========================================================================
 * Battle initialization
 * =========================================================================== */

static void InitBattle(void)
{
    /* Set battle type: single trainer battle */
    gBattleTypeFlags = BATTLE_TYPE_TRAINER;

    /* Trainer ID for the opponent */
    gTrainerBattleOpponent_A = 1; /* arbitrary non-zero trainer ID */
    gPartnerTrainerId = 0;

    /* Initialize battle resources */
    AllocateBattleResources();
    AllocateBattleSpritesData();

    /* Setup controllers and initial state */
    SetUpBattleVarsAndBirchZigzagoon();

    /* Initialize battlers */
    gBattlersCount = 2; /* 1v1 single battle */
    gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
    gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
    gBattlerPartyIndexes[0] = 0;
    gBattlerPartyIndexes[1] = 0;

    /* Set controllers to our console controller */
    gActiveBattler = 0; SetControllerToConsole();
    gActiveBattler = 1; SetControllerToConsole();

    /* Initialize battle controllers and set party IDs */
    InitBattleControllers();

    /* Copy party data into battle mons (BattleIntroGetMonsData equivalent) */
    gBattleMainFunc = BeginBattleIntro;

    printf("==============================================\n");
    printf("   POKEMON BATTLE - DESKTOP ENGINE\n");
    printf("==============================================\n");
    printf("Player's team: %d Pokemon\n", 1);
    printf("Opponent's team: %d Pokemon\n", 1);
    printf("----------------------------------------------\n\n");
}

/* ===========================================================================
 * Battle loop
 * =========================================================================== */

static void RunBattleLoop(void)
{
    /* Run the battle engine state machine.
     * BattleMainCB1() advances gBattleMainFunc() once and then calls each
     * battler's controller function. We call it in a tight loop until the
     * battle ends (gBattleOutcome != 0).
     *
     * The battle engine is designed for a 60fps GBA where BattleMainCB1 is
     * called every frame. On desktop we simply call it as fast as possible
     * (the console controller's stdin reads block when input is needed).
     */
    u32 frameCount = 0;
    const u32 MAX_FRAMES = 1000000; /* safety limit against infinite loops */

    while (gBattleOutcome == 0 && frameCount < MAX_FRAMES)
    {
        /* Run the battle state machine */
        gBattleMainFunc();

        /* Run each battler's controller */
        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
            gBattlerControllerFuncs[gActiveBattler]();

        /* Run any pending tasks (health bar animations, etc. - all stubs on desktop) */
        RunTasks();

        frameCount++;
    }

    if (frameCount >= MAX_FRAMES)
        printf("\n[WARNING] Battle loop hit safety limit - possible infinite loop.\n");
}

/* ===========================================================================
 * Print battle result
 * =========================================================================== */

static void PrintBattleResult(void)
{
    printf("\n==============================================\n");
    printf("BATTLE RESULT: ");
    switch (gBattleOutcome)
    {
    case B_OUTCOME_WON:    printf("PLAYER WON!\n");    break;
    case B_OUTCOME_LOST:   printf("PLAYER LOST!\n");   break;
    case B_OUTCOME_DREW:   printf("DRAW!\n");          break;
    case B_OUTCOME_RAN:    printf("Player ran away.\n"); break;
    default:               printf("Battle ended (outcome=%d)\n", gBattleOutcome); break;
    }
    printf("==============================================\n");
}

/* ===========================================================================
 * Entry point
 * =========================================================================== */

int main(int argc, char **argv)
{
    /* Seed the RNG */
    srand((unsigned)time(NULL));
    SeedRng((u16)(rand() & 0xFFFF));

    /* Initialize save block (battle engine reads options from it) */
    InitSaveBlock();

    /* Initialize the task system */
    ResetTasks();

    /* Initialize party arrays */
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();

    /* Set up both teams */
    SetupPlayerTeam();
    SetupOpponentTeam();

    /* Initialize and run the battle */
    InitBattle();
    RunBattleLoop();
    PrintBattleResult();

    /* Clean up */
    FreeBattleResources();
    FreeBattleSpritesData();

    return 0;
}
