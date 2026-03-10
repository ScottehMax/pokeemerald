#ifndef GUARD_BATTLE_GFX_SFX_UTIL_H
#define GUARD_BATTLE_GFX_SFX_UTIL_H

/* Desktop stubs for battle_gfx_sfx_util.h */

#include "sprite.h"

static inline u16 ChooseMoveAndTargetInBattlePalace(void) { return 0; }
static inline void SpriteCB_WaitForBattlerBallReleaseAnim(struct Sprite *sprite) {}
static inline void SpriteCB_TrainerSlideIn(struct Sprite *sprite) {}
static inline void InitAndLaunchChosenStatusAnimation(bool8 isStatus2, u32 status) {}
static inline bool8 TryHandleLaunchBattleTableAnimation(u8 activeBattler, u8 atkBattler, u8 defBattler, u8 tableId, u16 argument) { return FALSE; }
static inline void InitAndLaunchSpecialAnimation(u8 activeBattler, u8 atkBattler, u8 defBattler, u8 tableId) {}
static inline bool8 IsMoveWithoutAnimation(u16 move, u8 animationTurn) { return TRUE; }
static inline bool8 IsBattleSEPlaying(u8 battler) { return FALSE; }
static inline void BattleLoadOpponentMonSpriteGfx(struct Pokemon *mon, u8 battler) {}
static inline void BattleLoadPlayerMonSpriteGfx(struct Pokemon *mon, u8 battler) {}
static inline void BattleGfxSfxDummy2(u16 species) {}
static inline void DecompressTrainerFrontPic(u16 frontPicId, u8 battler) {}
static inline void DecompressTrainerBackPic(u16 backPicId, u8 battler) {}
static inline void BattleGfxSfxDummy3(u8 gender) {}
static inline void FreeTrainerFrontPicPalette(u16 frontPicId) {}
static inline bool8 BattleLoadAllHealthBoxesGfx(u8 state) { return TRUE; }
static inline void LoadBattleBarGfx(u8 unused) {}
static inline bool8 BattleInitAllSprites(u8 *state1, u8 *battler) { return TRUE; }
static inline void ClearSpritesHealthboxAnimData(void) {}
static inline void CopyAllBattleSpritesInvisibilities(void) {}
static inline void CopyBattleSpriteInvisibility(u8 battler) {}
static inline void HandleSpeciesGfxDataChange(u8 battlerAtk, u8 battlerDef, bool8 castform) {}
static inline void BattleLoadSubstituteOrMonSpriteGfx(u8 battler, bool8 loadMonSprite) {}
static inline void LoadBattleMonGfxAndAnimate(u8 battler, bool8 loadMonSprite, u8 spriteId) {}
static inline void TrySetBehindSubstituteSpriteBit(u8 battler, u16 move) {}
static inline void ClearBehindSubstituteBit(u8 battler) {}
static inline void HandleLowHpMusicChange(struct Pokemon *mon, u8 battler) {}
static inline void BattleStopLowHpSound(void) {}
static inline u8 GetMonHPBarLevel(struct Pokemon *mon) { return 0; }
static inline void HandleBattleLowHpMusicChange(void) {}
static inline void SetBattlerSpriteAffineMode(u8 affineMode) {}
static inline void LoadAndCreateEnemyShadowSprites(void) {}
static inline void SpriteCB_SetInvisible(struct Sprite *sprite) {}
static inline void SetBattlerShadowSpriteCallback(u8 battler, u16 species) {}
static inline void HideBattlerShadowSprite(u8 battler) {}
static inline void FillAroundBattleWindows(void) {}
static inline void ClearTemporarySpeciesSpriteData(u8 battler, bool8 dontClearSubstitute) {}
static inline void AllocateMonSpritesGfx(void) {}
static inline void FreeMonSpritesGfx(void) {}
static inline bool32 ShouldPlayNormalMonCry(struct Pokemon *mon) { return FALSE; }

/* Defined in stubs.c */
void AllocateBattleSpritesData(void);
void FreeBattleSpritesData(void);

#endif /* GUARD_BATTLE_GFX_SFX_UTIL_H */
