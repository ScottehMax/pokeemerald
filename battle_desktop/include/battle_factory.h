#ifndef GUARD_BATTLE_FACTORY_H
#define GUARD_BATTLE_FACTORY_H

static inline void CallBattleFactoryFunction(void) {}
static inline bool8 InBattleFactory(void) { return FALSE; }
static inline u8 GetFactoryMonFixedIV(u8 challengeNum, bool8 isLastBattle) { return 0; }
static inline void FillFactoryBrainParty(void) {}
static inline u8 GetNumPastRentalsRank(u8 battleMode, u8 lvlMode) { return 0; }
static inline u32 GetAiScriptsInBattleFactory(void) { return 0; }
static inline void SetMonMoveAvoidReturn(struct Pokemon *mon, u16 moveArg, u8 moveSlot) {}

#endif /* GUARD_BATTLE_FACTORY_H */
