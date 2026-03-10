#ifndef GUARD_POKEBLOCK_H
#define GUARD_POKEBLOCK_H

#define TAG_POKEBLOCK 14818

enum
{
    PBLOCK_CLR_NONE,  PBLOCK_CLR_RED,   PBLOCK_CLR_BLUE,  PBLOCK_CLR_PINK,
    PBLOCK_CLR_GREEN, PBLOCK_CLR_YELLOW, PBLOCK_CLR_PURPLE, PBLOCK_CLR_INDIGO,
    PBLOCK_CLR_BROWN, PBLOCK_CLR_LITE_BLUE, PBLOCK_CLR_OLIVE, PBLOCK_CLR_GRAY,
    PBLOCK_CLR_BLACK, PBLOCK_CLR_WHITE, PBLOCK_CLR_GOLD,
};

enum { PBLOCK_COLOR, PBLOCK_SPICY, PBLOCK_DRY, PBLOCK_SWEET, PBLOCK_BITTER, PBLOCK_SOUR, PBLOCK_FEEL };
enum { PBLOCK_CASE_FIELD, PBLOCK_CASE_BATTLE, PBLOCK_CASE_FEEDER, PBLOCK_CASE_GIVE };

extern u8 gPokeblockMonId;
extern s16 gPokeblockGain;
#define NUM_NATURES 25
#define FLAVOR_COUNT 5
extern const s8 gPokeblockFlavorCompatibilityTable[NUM_NATURES * FLAVOR_COUNT];

static inline void ChooseMonToGivePokeblock(struct Pokeblock *pokeblock, void (*callback)(void)) {}
static inline void PreparePokeblockFeedScene(void) {}
static inline void OpenPokeblockCase(u8 caseId, void (*callback)(void)) {}
static inline void OpenPokeblockCaseInBattle(void) {}

#endif /* GUARD_POKEBLOCK_H */
