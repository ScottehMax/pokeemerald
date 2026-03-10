#ifndef GUARD_POKEDEX_H
#define GUARD_POKEDEX_H
#include "gba/types.h"

struct PokedexEntry {
    u8 categoryName[12];
    u16 height;
    u16 weight;
    const u8 *description;
    u16 unused;
    u16 pokemonScale;
    u16 pokemonOffset;
    u16 trainerScale;
    u16 trainerOffset;
};

enum {
    FLAG_GET_SEEN,
    FLAG_GET_CAUGHT,
    FLAG_SET_SEEN,
    FLAG_SET_CAUGHT,
};
static inline s8 GetSetPokedexFlag(u16 nationalNum, u8 caseId) { return 0; }
static inline bool8 GetPokedexFlag(u16 nationalNum, u8 caseId) { return FALSE; }

#endif /* GUARD_POKEDEX_H */
