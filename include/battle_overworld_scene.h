#ifndef GUARD_BATTLE_OVERWORLD_SCENE_H
#define GUARD_BATTLE_OVERWORLD_SCENE_H

#include "global.h"
#include "pokemon.h"

void BattleOverworldScene_Reset(void);
void BattleOverworldScene_ResetSpriteReferences(void);
bool8 BattleOverworldScene_IsEnabled(void);
void BattleOverworldScene_SetSuspended(bool8 suspended);
u32 BattleOverworldScene_GetBgPaletteMask(void);
u32 BattleOverworldScene_ApplyBgPaletteMask(u32 selectedPalettes);
bool8 BattleOverworldScene_IsBattlerFacingRight(u8 battler);
void BattleOverworldScene_LoadBackground(void);
void BattleOverworldScene_RestoreBackground(void);
void BattleOverworldScene_KeepBaseBackgroundVisible(void);
void BattleOverworldScene_BeginReshowBlackout(void);
void BattleOverworldScene_BeginSceneFadeIn(void);
void BattleOverworldScene_CreateTrainerSprites(void);
void BattleOverworldScene_CreateIntroSprites(void);
void BattleOverworldScene_CreateInitialSprites(void);
void BattleOverworldScene_TryWildShinyAnimations(void);
bool8 BattleOverworldScene_CreateBattlerSprite(u8 battler);
void BattleOverworldScene_RegisterBattlerSprite(u8 battler, u8 spriteId);
bool8 BattleOverworldScene_IsBattlerSprite(u8 battler, u8 spriteId);
void BattleOverworldScene_SetBattlerHiddenByBall(u8 battler, bool8 hidden);
s16 BattleOverworldScene_GetBattlerSpriteX(u8 battler);
s16 BattleOverworldScene_GetBattlerSpriteY(u8 battler);
bool8 BattleOverworldScene_LoadMonSpriteGfx(struct Pokemon *mon, u8 battler);
bool8 BattleOverworldScene_SetMonSpriteTemplate(u16 species, u8 battlerPosition);
void BattleOverworldScene_RestoreBattlerSpriteAnim(u8 battler);

#endif // GUARD_BATTLE_OVERWORLD_SCENE_H
