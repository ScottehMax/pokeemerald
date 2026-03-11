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
#include "battle_desktop/generated/battle_scripts.h"
#include "pokemon.h"
#include "random.h"
#include "task.h"
#include "malloc.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/abilities.h"
#include "constants/items.h"
#include "constants/battle_script_commands.h"
#include "gba/io_reg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

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
    CreateMon(&gPlayerParty[1], SPECIES_SKARMORY, 50, 15, FALSE, 0, OT_ID_PLAYER_ID, 0);
    {
        u16 move;
        u8 pp;
        move = MOVE_BEAT_UP;  SetMonData(&gPlayerParty[0], MON_DATA_MOVE1, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP1, &pp);
        // move = MOVE_BRICK_BREAK; SetMonData(&gPlayerParty[0], MON_DATA_MOVE2, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP2, &pp);
        // move = MOVE_SLASH;       SetMonData(&gPlayerParty[0], MON_DATA_MOVE3, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP3, &pp);
        // move = MOVE_BULK_UP;     SetMonData(&gPlayerParty[0], MON_DATA_MOVE4, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP4, &pp);
    }
}

static void SetupOpponentTeam(void)
{
    /* Slot 0: Metagross level 50 — single-mon team so one faint ends the battle */
    CreateMon(&gEnemyParty[0], SPECIES_METAGROSS, 50, 15, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    {
        u16 move;
        u8 pp;
        move = MOVE_FIRE_BLAST; SetMonData(&gEnemyParty[0], MON_DATA_MOVE1, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP1, &pp);
        move = MOVE_TOXIC;     SetMonData(&gEnemyParty[0], MON_DATA_MOVE2, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP2, &pp);
        move = MOVE_TOXIC;  SetMonData(&gEnemyParty[0], MON_DATA_MOVE3, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP3, &pp);
        move = MOVE_TOXIC; SetMonData(&gEnemyParty[0], MON_DATA_MOVE4, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP4, &pp);
    }
    CreateMon(&gEnemyParty[1], SPECIES_AGGRON, 50, 15, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);

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

/* Detect the battle-script yesnobox command (opcode 0x67) when it is waiting
 * for a button press (case 1), ask the player via stdin, and prime gMain.newKeys
 * with A_BUTTON (Yes) or B_BUTTON (No).
 *
 * Cmd_yesnobox in battle_script_commands.c polls GBA buttons directly instead
 * of going through the controller command path.  Since both the caller and
 * the function it calls (BattleCreateYesNoCursorAt) live in the same .c file,
 * linker --wrap cannot intercept them.  Detecting the state in the loop is the
 * only portable hook point.
 *
 * gBattleCommunication[0] == 1  means the yesnobox is in "wait for input" state.
 * *gBattlescriptCurrInstr == B_SCR_OP_YESNOBOX (0x67) confirms the opcode.
 */
static void HandleYesNoBoxIfPending(bool8 *askedOut)
{
    if (!(*askedOut)
        && gBattlescriptCurrInstr != NULL
        && *gBattlescriptCurrInstr == B_SCR_OP_YESNOBOX
        && gBattleCommunication[0] == 1)
    {
        *askedOut = TRUE;
        printf("(1=Yes / 2=No): ");
        fflush(stdout);

        char line[64];
        int choice = 1; /* default Yes on EOF */
        if (fgets(line, sizeof(line), stdin))
            choice = atoi(line);

        gMain.newKeys = (choice == 2) ? B_BUTTON : A_BUTTON;
    }
    else if (*askedOut && (gBattlescriptCurrInstr == NULL || *gBattlescriptCurrInstr != B_SCR_OP_YESNOBOX))
    {
        /* Script has advanced past the yesnobox; clear state for the next one */
        *askedOut = FALSE;
        gMain.newKeys = 0;
    }
}

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
    bool8 yesNoAsked = FALSE;

    while (gBattleOutcome == 0 && frameCount < MAX_FRAMES)
    {
        /* Run the battle state machine */
        gBattleMainFunc();

        /* Inject yes/no input for the battle-script yesnobox command */
        HandleYesNoBoxIfPending(&yesNoAsked);

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
    /* Set console to UTF-8 so accented characters (é, etc.) display correctly */
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    /* Seed the RNG */
    srand((unsigned)time(NULL));
    SeedRng((u16)(rand() & 0xFFFF));

    /* Initialize battle script variable address table (Option A) */
    InitBattleScriptVarTable();

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
