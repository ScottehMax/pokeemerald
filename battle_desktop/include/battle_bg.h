#ifndef GUARD_BATTLE_BG_H
#define GUARD_BATTLE_BG_H
#include "gba/types.h"
/* Desktop stub - all background setup functions are no-ops */
extern u16 gBattle_BG0_X;
extern u16 gBattle_BG0_Y;
extern u16 gBattle_BG1_X;
extern u16 gBattle_BG1_Y;
extern u16 gBattle_BG2_X;
extern u16 gBattle_BG2_Y;
extern u16 gBattle_BG3_X;
extern u16 gBattle_BG3_Y;
extern u16 gBattle_WIN0H;
extern u16 gBattle_WIN0V;
extern u16 gBattle_WIN1H;
extern u16 gBattle_WIN1V;
static inline void LoadBattleBGs(void) {}
static inline void SetBattleBgPalette(void) {}
static inline void UpdateBattleWeather(void) {}
static inline void DrawMainBattleBackground(void) {}
#endif
