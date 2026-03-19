/*
 * main.c - Desktop battle engine entry point
 *
 * Sets up two teams and runs a complete Pokémon battle using the authentic
 * Generation III battle engine extracted from the pokeemerald decomp.
 *
 * Usage:
 *   battle_desktop [flags] [--team1 FILE] [--team2 FILE]
 *
 * Flags:
 *   --pvp   / -p   Player-vs-player: both sides controlled via stdin (no AI).
 *                  Uses BATTLE_TYPE_LINK internally (link-battle rules apply).
 *   --ai    / -a   AI-vs-AI: both sides controlled by the AI (no stdin input).
 *   --double / -2  Doubles battle. Default is singles.
 *   --debug / -d   Verbose debug output to stderr.
 *   --team1 FILE   Load the player's team from FILE instead of the built-in team.
 *   --team2 FILE   Load the opponent's team from FILE instead of the built-in team.
 *
 * Team file format uses the Showdown format.
 *
 *   Example:
 * # team1.txt
 * Foom (Kyogre) @ Starf Berry
 * Ability: Drought  
 * Level: 50  
 * EVs: 252 HP / 252 SpA / 4 SpD  
 * Modest Nature  
 * - Metronome
 * - Thunder  
 * - Ice Beam  
 * - Calm Mind  
 *
 * How to configure built-in teams (if no --team file is given):
 *   1. Call CreateMon() to create a Pokémon in the party array
 *   2. Use SetMonData() to set species, moves, EVs, IVs, etc.
 *   3. Call CalcLevel() and CalculateMonStats() to compute stats
 *
 * The battle runs fully in text mode; all graphical events are no-ops.
 */

#include "global.h"
#include "battle.h"
#include "battle_main.h"
#include "battle_controllers.h"
#include "battle_setup.h"
#include "battle_desktop/generated/battle_scripts.h"
#include "battle_desktop/team_parser.h"
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
#include "link.h"
#include "constants/characters.h"
#include "constants/trainers.h"
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
extern bool8 gDebugMode;    /* Set by --debug flag; defined in console_controller.c */
extern bool8 gPvpMode;      /* Set by --pvp flag;   defined in console_controller.c */
extern bool8 gBothAiMode;   /* Set by --ai flag;    defined in console_controller.c */
extern void Desktop_ResetLinkSendBuffer(void); /* Flush link send buffer each frame */

static bool8  sDoubleBattle = FALSE; /* Set by --double flag */
static const char *sTeam1File = NULL; /* Set by --team1 flag */
static const char *sTeam2File = NULL; /* Set by --team2 flag */

/* ===========================================================================
 * Default team setup
 *
 * Modify these functions to configure the teams you want to battle.
 * Each call to CreateMon() creates a party Pokémon.
 * =========================================================================== */

static void SetupPlayerTeam(void)
{
    CreateMon(&gPlayerParty[0], SPECIES_KYOGRE, 50, 15, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[1], SPECIES_SKARMORY, 50, 15, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gPlayerParty[2], SPECIES_BRELOOM, 50, 15, FALSE, 0, OT_ID_PLAYER_ID, 0);
    {
        u16 move;
        u8 pp;
        move = MOVE_TOXIC; SetMonData(&gPlayerParty[0], MON_DATA_MOVE1, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP1, &pp);
        move = MOVE_BODY_SLAM; SetMonData(&gPlayerParty[0], MON_DATA_MOVE2, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP2, &pp);
        move = MOVE_SHEER_COLD; SetMonData(&gPlayerParty[0], MON_DATA_MOVE3, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP3, &pp);
        move = MOVE_METRONOME; SetMonData(&gPlayerParty[0], MON_DATA_MOVE4, &move); pp = gBattleMoves[move].pp; SetMonData(&gPlayerParty[0], MON_DATA_PP4, &pp);
    }
}

static void SetupOpponentTeam(void)
{
    CreateMon(&gEnemyParty[0], SPECIES_GROUDON, 50, 15, TRUE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    {
        u16 move;
        u8 pp;
        move = MOVE_FIRE_BLAST; SetMonData(&gEnemyParty[0], MON_DATA_MOVE1, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP1, &pp);
        move = MOVE_ICE_BEAM;   SetMonData(&gEnemyParty[0], MON_DATA_MOVE2, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP2, &pp);
        move = MOVE_EARTHQUAKE; SetMonData(&gEnemyParty[0], MON_DATA_MOVE3, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP3, &pp);
        move = MOVE_THUNDER;    SetMonData(&gEnemyParty[0], MON_DATA_MOVE4, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[0], MON_DATA_PP4, &pp);
    }
    CreateMon(&gEnemyParty[1], SPECIES_AGGRON, 50, 15, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    {
        u16 move;
        u8 pp;
        move = MOVE_IRON_TAIL;   SetMonData(&gEnemyParty[1], MON_DATA_MOVE1, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[1], MON_DATA_PP1, &pp);
        move = MOVE_ROCK_SLIDE;  SetMonData(&gEnemyParty[1], MON_DATA_MOVE2, &move); pp = gBattleMoves[move].pp; SetMonData(&gEnemyParty[1], MON_DATA_PP2, &pp);
    }
    CreateMon(&gEnemyParty[2], SPECIES_KECLEON, 50, 15, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
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
    /* Battle type flags:
     *   Normal:  BATTLE_TYPE_TRAINER — AI opponent, standard trainer rules
     *   PvP:     BATTLE_TYPE_LINK | BATTLE_TYPE_IS_MASTER — both sides human,
     *            no AI, link-battle rules (no shift prompt, no EXP, can't run)
     * Either can be combined with BATTLE_TYPE_DOUBLE for doubles. */
    if (gPvpMode)
    {
        gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER | BATTLE_TYPE_IS_MASTER;

        /* Initialize gLinkPlayers so GetBattlerMultiplayerId returns valid
         * indices and link-battle string placeholders expand correctly.
         * Without this, OOB reads from gLinkPlayers overflow gDisplayedStringBattle
         * into gEnemyParty, corrupting opponent Pokémon data. */
        memset(gLinkPlayers, 0, MAX_RFU_PLAYERS * sizeof(struct LinkPlayer));
        gLinkPlayers[0].id = 0; /* Player (master) = battler 0 */
        memcpy(gLinkPlayers[0].name, gSaveBlock2Ptr->playerName, PLAYER_NAME_LENGTH + 1);
        gLinkPlayers[0].gender = gSaveBlock2Ptr->playerGender;
        gLinkPlayers[0].version = VERSION_EMERALD;
        gLinkPlayers[0].language = LANGUAGE_ENGLISH;

        gLinkPlayers[1].id = 1; /* Opponent = battler 1 */
        /* Default opponent name: "BLUE" in GF encoding */
        gLinkPlayers[1].name[0] = CHAR_B;
        gLinkPlayers[1].name[1] = CHAR_L;
        gLinkPlayers[1].name[2] = CHAR_U;
        gLinkPlayers[1].name[3] = CHAR_E;
        gLinkPlayers[1].name[4] = EOS;
        gLinkPlayers[1].gender = MALE;
        gLinkPlayers[1].version = VERSION_EMERALD;
        gLinkPlayers[1].language = LANGUAGE_ENGLISH;
    }
    else
        gBattleTypeFlags = BATTLE_TYPE_TRAINER;

    if (sDoubleBattle)
        gBattleTypeFlags |= BATTLE_TYPE_DOUBLE;

    /* Trainer ID: use TRAINER_LINK_OPPONENT for PvP so the battle message
     * system selects link-style strings (e.g. "BLUE withdrew" instead of
     * "CHAMPION STEVEN withdrew"). */
    gTrainerBattleOpponent_A = gPvpMode ? TRAINER_LINK_OPPONENT : 1;
    gPartnerTrainerId = 0;

    /* Initialize battle resources */
    AllocateBattleResources();
    AllocateBattleSpritesData();

    /* Setup controllers and initial state */
    SetUpBattleVarsAndBirchZigzagoon();

    /* Set placeholder battler layout — overwritten by InitBattleControllers below.
     * We set it here so the pre-init SetControllerToConsole calls have valid state. */
    if (sDoubleBattle) {
        gBattlersCount = 4;
        gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
        gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
        gBattlerPositions[2] = B_POSITION_PLAYER_RIGHT;
        gBattlerPositions[3] = B_POSITION_OPPONENT_RIGHT;
        gBattlerPartyIndexes[0] = 0;
        gBattlerPartyIndexes[1] = 0;
        gBattlerPartyIndexes[2] = 1;
        gBattlerPartyIndexes[3] = 1;
        gActiveBattler = 0; SetControllerToConsole();
        gActiveBattler = 1; SetControllerToConsole();
        gActiveBattler = 2; SetControllerToConsole();
        gActiveBattler = 3; SetControllerToConsole();
    } else {
        gBattlersCount = 2;
        gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
        gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
        gBattlerPartyIndexes[0] = 0;
        gBattlerPartyIndexes[1] = 0;
        gActiveBattler = 0; SetControllerToConsole();
        gActiveBattler = 1; SetControllerToConsole();
    }

    /* Initialize battle controllers (sets up gBattlerControllerFuncs, positions, etc.) */
    InitBattleControllers();

    gBattleMainFunc = BeginBattleIntro;

    printf("==============================================\n");
    printf("   POKEMON BATTLE - DESKTOP ENGINE\n");
    printf("==============================================\n");
    printf("Mode: %s %s\n",
           gBothAiMode ? "AI vs AI" : (gPvpMode ? "PvP (Player vs Player)" : "Trainer (Player vs AI)"),
           sDoubleBattle ? "| Doubles" : "| Singles");
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
        if (gBothAiMode) {
            /* Both-AI mode: auto-answer "No" (decline shift) */
            gMain.newKeys = B_BUTTON;
            return;
        }
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
        if (gDebugMode && frameCount < 2000)
            fprintf(stderr, "[FRAME %u] execFlags=%08X comm=%d,%d,%d,%d,%d battlers=%d func=%p outcome=%d\n",
                    frameCount, gBattleControllerExecFlags,
                    gBattleCommunication[0], gBattleCommunication[1],
                    gBattleCommunication[2], gBattleCommunication[3],
                    gBattleCommunication[4],
                    gBattlersCount,
                    (void*)gBattleMainFunc, gBattleOutcome);
        gBattleMainFunc();
        if (gDebugMode && gBattleOutcome != 0)
            fprintf(stderr, "[OUTCOME SET] frame=%u outcome=%d func=%p\n",
                    frameCount, gBattleOutcome, (void*)gBattleMainFunc);

        /* Inject yes/no input for the battle-script yesnobox command */
        HandleYesNoBoxIfPending(&yesNoAsked);

        /* Run each battler's controller */
        for (gActiveBattler = 0; gActiveBattler < gBattlersCount; gActiveBattler++)
            gBattlerControllerFuncs[gActiveBattler]();

        /* In link mode, reset the send-buffer write pointer so next frame's
         * BtlController_Emit* calls start at offset 0 in gLinkBattleSendBuffer. */
        if (gBattleTypeFlags & BATTLE_TYPE_LINK)
            Desktop_ResetLinkSendBuffer();

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
    /* Parse flags:
     *   --debug / -d   : enable verbose debug output
     *   --pvp   / -p   : player-vs-player (both sides use stdin, no AI)
     *   --ai    / -a   : AI-vs-AI (both sides use AI, no stdin input)
     *   --double / -2  : doubles battle (default: singles)
     */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0)
            gDebugMode = TRUE;
        else if (strcmp(argv[i], "--pvp") == 0 || strcmp(argv[i], "-p") == 0)
            gPvpMode = TRUE;
        else if (strcmp(argv[i], "--ai") == 0 || strcmp(argv[i], "-a") == 0)
            gBothAiMode = TRUE;
        else if (strcmp(argv[i], "--double") == 0 || strcmp(argv[i], "-2") == 0)
            sDoubleBattle = TRUE;
        else if (strcmp(argv[i], "--team1") == 0 && i + 1 < argc)
            sTeam1File = argv[++i];
        else if (strcmp(argv[i], "--team2") == 0 && i + 1 < argc)
            sTeam2File = argv[++i];
    }

    /* Set console to UTF-8 so accented characters (é, etc.) display correctly */
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    /* Seed the RNG */
    srand((unsigned)time(NULL));
    SeedRng(rand() & 0xFFFF);

    /* Initialize battle script variable address table (Option A) */
    InitBattleScriptVarTable();

    /* Initialize save block (battle engine reads options from it) */
    InitSaveBlock();

    /* Initialize the task system */
    ResetTasks();

    /* Initialize party arrays */
    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();

    /* Set up both teams (from file if --team1/--team2 were given, else built-in) */
    if (sTeam1File)
    {
        if (!ParseTeamFile(sTeam1File, gPlayerParty, OT_ID_PLAYER_ID))
            return 1;
    }
    else
        SetupPlayerTeam();

    if (sTeam2File)
    {
        if (!ParseTeamFile(sTeam2File, gEnemyParty, OT_ID_RANDOM_NO_SHINY))
            return 1;
    }
    else
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
