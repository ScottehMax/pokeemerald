/*
 * team_randomiser.c - Generate random teams for ML training
 *
 * Creates a party of random Pokémon with level-up moves at a given level.
 * Uses the authentic species data, learnsets, and item tables from the
 * pokeemerald ROM data.
 */

#include "global.h"
#include "pokemon.h"
#include "random.h"
#include "constants/species.h"
#include "constants/moves.h"
#include "constants/items.h"
#include "constants/pokemon.h"
#include <stdlib.h>
#include <string.h>

/* =========================================================================
 * Configuration
 * ========================================================================= */

#define RANDOM_TEAM_SIZE     3   /* default party size for random teams */
#define RANDOM_LEVEL        50   /* default level */

/* Species to exclude (eggs, special forms, etc.) */
static int IsValidSpecies(u16 species)
{
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
        return 0;
    /* Skip Unown forms — they're indexed separately */
    if (species == SPECIES_EGG)
        return 0;
    return 1;
}

/* =========================================================================
 * Random number helpers (uses libc rand, seeded per call)
 * ========================================================================= */

static u32 RandU32(void)
{
    return ((u32)rand() << 16) | (u32)(rand() & 0xFFFF);
}

static u32 RandRange(u32 lo, u32 hi)
{
    if (lo >= hi) return lo;
    return lo + (RandU32() % (hi - lo));
}

/* =========================================================================
 * Build a pool of level-up moves for a species at a given level
 * ========================================================================= */

static int GetLearnableMoves(u16 species, u8 level, u16 *movesOut, int maxMoves)
{
    int count = 0;
    const u16 *learnset = gLevelUpLearnsets[species];

    for (int i = 0; learnset[i] != LEVEL_UP_END; i++) {
        u16 moveLevel = (learnset[i] & LEVEL_UP_MOVE_LV) >> 9;
        u16 moveId    = (learnset[i] & LEVEL_UP_MOVE_ID);

        if (moveLevel <= level && moveId != MOVE_NONE) {
            /* Check for duplicates */
            int dup = 0;
            for (int j = 0; j < count; j++) {
                if (movesOut[j] == moveId) { dup = 1; break; }
            }
            if (!dup && count < maxMoves)
                movesOut[count++] = moveId;
        }
    }
    return count;
}

/* =========================================================================
 * Shuffle array (Fisher-Yates)
 * ========================================================================= */

static void ShuffleU16(u16 *arr, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = RandRange(0, i + 1);
        u16 tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* =========================================================================
 * Public: generate a random team
 * ========================================================================= */

void RandomiseTeam(struct Pokemon *party, u8 otIdType, u32 seed)
{
    srand(seed);

    /* Build pool of valid species */
    u16 speciesPool[NUM_SPECIES];
    int poolSize = 0;
    for (u16 s = 1; s < NUM_SPECIES; s++) {
        if (IsValidSpecies(s))
            speciesPool[poolSize++] = s;
    }

    /* Shuffle and pick RANDOM_TEAM_SIZE species */
    ShuffleU16(speciesPool, poolSize);

    int teamSize = (poolSize < RANDOM_TEAM_SIZE) ? poolSize : RANDOM_TEAM_SIZE;

    for (int i = 0; i < teamSize; i++) {
        u16 species = speciesPool[i];

        /* Create the mon at RANDOM_LEVEL */
        CreateMon(&party[i], species, RANDOM_LEVEL, 31, FALSE, 0, otIdType, 0);

        /* Get learnable moves and pick up to 4 randomly */
        u16 learnableMoves[128]; /* more than any species knows */
        int numLearnable = GetLearnableMoves(species, RANDOM_LEVEL, learnableMoves, 128);

        if (numLearnable > 0) {
            ShuffleU16(learnableMoves, numLearnable);
            int numToSet = (numLearnable < MAX_MON_MOVES) ? numLearnable : MAX_MON_MOVES;

            for (int j = 0; j < numToSet; j++) {
                u16 move = learnableMoves[j];
                u8 pp = gBattleMoves[move].pp;
                int monDataMove = MON_DATA_MOVE1 + j;
                int monDataPp   = MON_DATA_PP1 + j;
                SetMonData(&party[i], monDataMove, &move);
                SetMonData(&party[i], monDataPp, &pp);
            }
        }

        /* Random EVs: distribute 510 total across stats */
        u8 evs[6] = {0};
        int evTotal = 510;
        for (int j = 0; j < 6 && evTotal > 0; j++) {
            int maxEv = (evTotal > 252) ? 252 : evTotal;
            evs[j] = (u8)RandRange(0, maxEv + 1);
            evTotal -= evs[j];
        }
        SetMonData(&party[i], MON_DATA_HP_EV, &evs[0]);
        SetMonData(&party[i], MON_DATA_ATK_EV, &evs[1]);
        SetMonData(&party[i], MON_DATA_DEF_EV, &evs[2]);
        SetMonData(&party[i], MON_DATA_SPEED_EV, &evs[3]);
        SetMonData(&party[i], MON_DATA_SPATK_EV, &evs[4]);
        SetMonData(&party[i], MON_DATA_SPDEF_EV, &evs[5]);

        /* Recalculate stats with the new EVs */
        CalculateMonStats(&party[i]);
    }
}
