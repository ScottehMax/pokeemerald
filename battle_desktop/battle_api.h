/*
 * battle_api.h - Public C API for the pokeemerald desktop battle engine
 *
 * This header exposes the battle engine as a library for programmatic control,
 * designed for use from Python (via ctypes) for ML/RL experiments.
 *
 * Usage:
 *   1. battle_init(seed)           — initialise the engine
 *   2. battle_set_team_*(side,...) — configure teams
 *   3. battle_start()              — begin the battle
 *   4. Loop:
 *        rc = battle_step()        — advance until decision or end
 *        if rc == BATTLE_STEP_DECIDE:
 *          battle_get_action_request(&req)
 *          battle_get_state(&state)
 *          battle_submit_action(&act)
 *        if rc == BATTLE_STEP_DONE:
 *          outcome = battle_get_outcome()
 *   5. battle_free()               — clean up
 */

#ifndef BATTLE_API_H
#define BATTLE_API_H

#ifdef _WIN32
  #ifdef BATTLE_API_BUILD
    #define BATTLE_API __declspec(dllexport)
  #else
    #define BATTLE_API __declspec(dllimport)
  #endif
#else
  #define BATTLE_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* =========================================================================
 * Constants
 * ========================================================================= */

#define BATTLE_MAX_BATTLERS  4
#define BATTLE_PARTY_SIZE    6
#define BATTLE_MAX_MOVES     4
#define BATTLE_NUM_SIDES     2
#define BATTLE_NUM_STATS     8   /* ATK/DEF/SPD/SPATK/SPDEF/ACC/EVA/unused */

/* Return codes from battle_step() */
#define BATTLE_STEP_CONTINUE  0   /* internal: keep looping (not returned to caller) */
#define BATTLE_STEP_DECIDE    1   /* a battler needs a decision */
#define BATTLE_STEP_DONE      2   /* battle is over */

/* Action request types */
#define BATTLE_REQUEST_NONE          0
#define BATTLE_REQUEST_ACTION        1  /* choose: fight / switch */
#define BATTLE_REQUEST_MOVE          2  /* choose which move (+ target in doubles) */
#define BATTLE_REQUEST_SWITCH        3  /* choose which pokemon to send out */
#define BATTLE_REQUEST_YES_NO        4  /* yes/no prompt (shift switch) */

/* Action types (what the agent submits) */
#define BATTLE_ACTION_FIGHT   0
#define BATTLE_ACTION_SWITCH  1
#define BATTLE_ACTION_YES     2
#define BATTLE_ACTION_NO      3

/* Battle outcome */
#define BATTLE_OUTCOME_NONE   0
#define BATTLE_OUTCOME_WON    1
#define BATTLE_OUTCOME_LOST   2
#define BATTLE_OUTCOME_DREW   3

/* =========================================================================
 * Observation structs
 * ========================================================================= */

/* Active battler on the field */
typedef struct {
    uint16_t species;
    uint16_t hp;
    uint16_t maxHp;
    uint16_t attack;
    uint16_t defense;
    uint16_t speed;
    uint16_t spAttack;
    uint16_t spDefense;
    int8_t   statStages[BATTLE_NUM_STATS];
    uint32_t status1;
    uint32_t status2;
    uint16_t moves[BATTLE_MAX_MOVES];
    uint8_t  pp[BATTLE_MAX_MOVES];
    uint8_t  types[2];
    uint8_t  ability;
    uint16_t item;
    uint8_t  level;
    uint8_t  isAlive;
} BattleActiveMon;

/* Party member (for switching decisions) */
typedef struct {
    uint16_t species;
    uint16_t hp;
    uint16_t maxHp;
    uint16_t item;
    uint32_t status;
    uint8_t  level;
    uint16_t moves[BATTLE_MAX_MOVES];
    uint8_t  pp[BATTLE_MAX_MOVES];
    uint8_t  types[2];
    uint8_t  ability;
    uint8_t  isAlive;
} BattlePartyMon;

/* Per-side field conditions */
typedef struct {
    uint16_t sideStatus;
    uint8_t  reflectTimer;
    uint8_t  lightscreenTimer;
    uint8_t  mistTimer;
    uint8_t  safeguardTimer;
    uint8_t  spikesAmount;
} BattleSideState;

/* Full battle state snapshot */
typedef struct {
    BattleActiveMon active[BATTLE_MAX_BATTLERS];
    BattlePartyMon  party[BATTLE_NUM_SIDES][BATTLE_PARTY_SIZE];
    BattleSideState sides[BATTLE_NUM_SIDES];
    uint16_t weather;
    uint8_t  turn;
    uint8_t  battlerCount;
    uint8_t  outcome;
} BattleState;

/* What decision the agent needs to make */
typedef struct {
    uint8_t  type;               /* BATTLE_REQUEST_* */
    uint8_t  battler;            /* battler index (0-3) */
    uint8_t  side;               /* 0=player, 1=opponent */

    /* For BATTLE_REQUEST_MOVE */
    uint16_t availableMoves[BATTLE_MAX_MOVES];
    uint8_t  movePp[BATTLE_MAX_MOVES];
    uint8_t  moveMaxPp[BATTLE_MAX_MOVES];

    /* For BATTLE_REQUEST_SWITCH / BATTLE_REQUEST_ACTION */
    uint8_t  canSwitch[BATTLE_PARTY_SIZE]; /* 1 = valid switch target */
    uint8_t  numAlive;

    /* For BATTLE_REQUEST_ACTION */
    uint8_t  canFight;           /* 1 if at least one move has PP */
    uint8_t  canStruggle;        /* 1 if all moves are unusable (Struggle) */

    /* Is this a forced switch (fainted mon must be replaced)? */
    uint8_t  forced;
} BattleActionRequest;

/* What the agent submits */
typedef struct {
    uint8_t  type;               /* BATTLE_ACTION_* */
    uint8_t  moveSlot;           /* 0-3 for FIGHT */
    uint8_t  switchSlot;         /* 0-5 party index for SWITCH */
    uint8_t  target;             /* target battler index for doubles moves */
} BattleAction;

/* Configuration */
typedef struct {
    uint8_t  doubles;            /* 1 for doubles, 0 for singles */
    uint8_t  controlSide[2];     /* 1 = this side is programmatically controlled */
                                 /* sides not controlled use the built-in AI */
    uint8_t  verbose;            /* 1 = print battle log to stdout (default 0) */
} BattleConfig;

/* =========================================================================
 * API Functions
 * ========================================================================= */

/* Lifecycle */
BATTLE_API void battle_init(uint32_t seed);
BATTLE_API void battle_free(void);

/* Configuration (call between init and start) */
BATTLE_API void battle_configure(const BattleConfig *config);

/* Team setup (call between init and start) */
BATTLE_API int  battle_set_team_showdown(int side, const char *text);
BATTLE_API void battle_set_team_random(int side, uint32_t seed);

/* Start the battle (call after teams are set) */
BATTLE_API void battle_start(void);

/* Stepping */
BATTLE_API int  battle_step(void);

/* Observation (valid after battle_start) */
BATTLE_API void battle_get_state(BattleState *out);
BATTLE_API void battle_get_action_request(BattleActionRequest *out);
BATTLE_API int  battle_get_outcome(void);

/* Action submission */
BATTLE_API void battle_submit_action(const BattleAction *action);

/* Convenience: full reset for a new episode */
BATTLE_API void battle_reset(uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif /* BATTLE_API_H */
