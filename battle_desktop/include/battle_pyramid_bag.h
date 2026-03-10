#ifndef GUARD_BATTLE_PYRAMID_BAG_H
#define GUARD_BATTLE_PYRAMID_BAG_H

enum {
    PYRAMIDBAG_LOC_FIELD,
    PYRAMIDBAG_LOC_BATTLE,
    PYRAMIDBAG_LOC_PARTY,
    PYRAMIDBAG_LOC_CHOOSE_TOSS,
    PYRAMIDBAG_LOC_PREV,
};

static inline void ShowBattlePyramidBag(u8 location, void (*callback)(void)) {}
static inline bool8 InBattlePyramidBag(void) { return FALSE; }
static inline u16 GetPyramidBagUsedItem(void) { return 0; }

#endif /* GUARD_BATTLE_PYRAMID_BAG_H */
