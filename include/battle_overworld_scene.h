#ifndef GUARD_BATTLE_OVERWORLD_SCENE_H
#define GUARD_BATTLE_OVERWORLD_SCENE_H

#include "global.h"
#include "pokemon.h"

#define BATTLE_OW_MOVE_BG_PAL_SLOT 9

void BattleOverworldScene_Reset(void);
void BattleOverworldScene_ResetSpriteReferences(void);
bool8 BattleOverworldScene_IsEnabled(void);
void BattleOverworldScene_SetSuspended(bool8 suspended);
void BattleOverworldScene_CreateDebugTrainerSprites(u16 playerGraphicsId, u16 opponentGraphicsId, s16 y);
u32 BattleOverworldScene_GetBgPaletteMask(void);
u32 BattleOverworldScene_ApplyBgPaletteMask(u32 selectedPalettes);
u32 BattleOverworldScene_ApplyScenePaletteMask(u32 selectedPalettes);
u16 BattleOverworldScene_ApplyBgBlendTargetMask(u16 blendCnt);
void BattleOverworldScene_ApplyAnimBgPriority(u8 bgId, u8 priority);
void BattleOverworldScene_SetMoveBgActive(bool8 active);
bool8 BattleOverworldScene_IsMoveBgActive(void);
bool8 BattleOverworldScene_IsBattlerFacingRight(u8 battler);
void BattleOverworldScene_LoadBackground(void);
void BattleOverworldScene_RestoreBackground(void);
void BattleOverworldScene_LoadDebugBackground(const struct MapLayout *layout, s16 x, s16 y);
bool8 BattleOverworldScene_BeginDebugBackgroundScroll(const struct MapLayout *layout, s16 x, s16 y);
bool8 BattleOverworldScene_UpdateDebugBackgroundScroll(void);
bool8 BattleOverworldScene_GetBackgroundOffsetForLayout(u16 layoutId, u16 *x, u16 *y);
void BattleOverworldScene_StopBackgroundAnimation(void);
void BattleOverworldScene_UpdateBackgroundAnimation(void);
void BattleOverworldScene_TransferBackgroundAnimation(void);
void BattleOverworldScene_ShowBaseBackgroundInVBlank(void);
void BattleOverworldScene_TryShowBaseBackgroundInVBlank(void);
bool8 BattleOverworldScene_IsTilesetAnimActive(void);
bool8 BattleOverworldScene_GetTilesetAnimDestination(u16 sourceTile, u16 **dest);
void BattleOverworldScene_KeepBaseBackgroundVisible(void);
bool8 BattleOverworldScene_IsProtectedBg(u8 bgId);
void BattleOverworldScene_BeginReshowBlackout(void);
void BattleOverworldScene_BeginSceneFadeIn(void);
void BattleOverworldScene_CreateTrainerSprites(void);
void BattleOverworldScene_CreateIntroSprites(void);
void BattleOverworldScene_CreateInitialSprites(void);
void BattleOverworldScene_TryWildShinyAnimations(void);
bool8 BattleOverworldScene_GetPlayerTrainerSpriteCoords(s16 *x, s16 *y);
bool8 BattleOverworldScene_CreateBattlerSprite(u8 battler);
void BattleOverworldScene_RegisterBattlerSprite(u8 battler, u8 spriteId);
bool8 BattleOverworldScene_IsBattlerSprite(u8 battler, u8 spriteId);
void BattleOverworldScene_StartBattlerSpriteAnim(u8 battler, u8 spriteId, u8 animNum);
void BattleOverworldScene_SetBattlerHiddenByBall(u8 battler, bool8 hidden);
void BattleOverworldScene_SetBattlerHiddenByMonBg(u8 battler, bool8 hidden);
void BattleOverworldScene_PrepareHealthbox(u8 battler, struct Pokemon *mon, u8 partyId);
bool8 BattleOverworldScene_IsHealthboxPrepared(u8 battler, u8 partyId);
void BattleOverworldScene_ClearPreparedHealthbox(u8 battler);
s16 BattleOverworldScene_GetBattlerSpriteX(u8 battler);
s16 BattleOverworldScene_GetBattlerSpriteY(u8 battler);
s16 BattleOverworldScene_GetBattlerSpriteWidth(u8 battler);
s16 BattleOverworldScene_GetBattlerSpriteHeight(u8 battler);
bool8 BattleOverworldScene_LoadMonSpriteGfx(struct Pokemon *mon, u8 battler);
bool8 BattleOverworldScene_SetMonSpriteTemplate(u16 species, u8 battlerPosition);
u8 BattleOverworldScene_CreatePreviewMonSprite(u16 species, u8 battlerPosition, s16 x, s16 y, u8 subpriority);
void BattleOverworldScene_RestoreBattlerSpriteAnim(u8 battler);
void BattleOverworldScene_RestoreBattlerSpriteOam(u8 battler);
u16 BattleOverworldScene_GetBattlerSpriteFrameImageValue(u8 battler);
bool8 BattleOverworldScene_DrawBattlerOnBg(u8 battler, u8 bgId, u8 paletteId, u16 tilesOffset, u8 *tiles, u16 *tilemap, s16 *bgX, s16 *bgY, u16 *frameTileCount);
bool8 BattleOverworldScene_UpdateBattlerBgFrame(u8 battler, u8 paletteId, u16 tilesOffset, u16 frameTileCount, u16 *tilemap);

#endif // GUARD_BATTLE_OVERWORLD_SCENE_H
