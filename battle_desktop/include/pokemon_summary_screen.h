#ifndef GUARD_POKEMON_SUMMARY_SCREEN_H
#define GUARD_POKEMON_SUMMARY_SCREEN_H

#include "main.h"

extern u8 gLastViewedMonIndex;

enum PokemonSummaryScreenMode
{
    SUMMARY_MODE_NORMAL,
    SUMMARY_MODE_LOCK_MOVES,
    SUMMARY_MODE_BOX,
    SUMMARY_MODE_SELECT_MOVE,
};

static inline void ShowPokemonSummaryScreen(u8 mode, void *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void)) {}
static inline void ShowSelectMovePokemonSummaryScreen(struct Pokemon *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void), u16 newMove) {}
static inline void ShowPokemonSummaryScreenHandleDeoxys(u8 mode, struct BoxPokemon *mons, u8 monIndex, u8 maxMonIndex, void (*callback)(void)) {}
static inline u8 GetMoveSlotToReplace(void) { return 0; }
static inline void SummaryScreen_SetAnimDelayTaskId(u8 taskId) {}

#endif /* GUARD_POKEMON_SUMMARY_SCREEN_H */
