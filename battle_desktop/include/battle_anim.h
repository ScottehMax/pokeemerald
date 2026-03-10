#ifndef GUARD_BATTLE_ANIM_H
#define GUARD_BATTLE_ANIM_H

/* Desktop stub for battle_anim.h - all animation functions are no-ops */

#include "battle.h"
#include "constants/battle_anim.h"
#include "task.h"
#include "sprite.h"

enum
{
    BG_ANIM_SCREEN_SIZE,
    BG_ANIM_AREA_OVERFLOW_MODE,
    BG_ANIM_MOSAIC,
    BG_ANIM_CHAR_BASE_BLOCK,
    BG_ANIM_PRIORITY,
    BG_ANIM_PALETTES_MODE,
    BG_ANIM_SCREEN_BASE_BLOCK,
};

struct BattleAnimBgData
{
    u8 *bgTiles;
    u16 *bgTilemap;
    u8 paletteId;
    u8 bgId;
    u16 tilesOffset;
    u16 unused;
};

struct BattleAnimBackground
{
    const u32 *image;
    const u32 *palette;
    const u32 *tilemap;
};

#define ANIM_ARGS_COUNT 8

extern void (*gAnimScriptCallback)(void);
extern bool8 gAnimScriptActive;
extern u8 gAnimVisualTaskCount;
extern u8 gAnimSoundTaskCount;
extern struct DisableStruct *gAnimDisableStructPtr;
extern s32 gAnimMoveDmg;
extern u16 gAnimMovePower;
extern u8 gAnimFriendship;
extern u16 gWeatherMoveAnim;
extern s16 gBattleAnimArgs[ANIM_ARGS_COUNT];
extern u8 gAnimMoveTurn;
extern u8 gBattleAnimAttacker;
extern u8 gBattleAnimTarget;
extern u16 gAnimBattlerSpecies[MAX_BATTLERS_COUNT];
extern u8 gAnimCustomPanning;

enum
{
    BATTLER_COORD_X,
    BATTLER_COORD_Y,
    BATTLER_COORD_X_2,
    BATTLER_COORD_Y_PIC_OFFSET,
    BATTLER_COORD_Y_PIC_OFFSET_DEFAULT,
};

enum
{
    BATTLER_COORD_ATTR_HEIGHT,
    BATTLER_COORD_ATTR_WIDTH,
    BATTLER_COORD_ATTR_TOP,
    BATTLER_COORD_ATTR_BOTTOM,
    BATTLER_COORD_ATTR_LEFT,
    BATTLER_COORD_ATTR_RIGHT,
    BATTLER_COORD_ATTR_RAW_BOTTOM,
};

#define STAT_ANIM_PLUS1  14
#define STAT_ANIM_PLUS2  38
#define STAT_ANIM_MINUS1 21
#define STAT_ANIM_MINUS2 45
#define STAT_ANIM_MULTIPLE_PLUS1  55
#define STAT_ANIM_MULTIPLE_PLUS2  56
#define STAT_ANIM_MULTIPLE_MINUS1 57
#define STAT_ANIM_MULTIPLE_MINUS2 58

/* Stubs for all battle animation functions */
static inline void ClearBattleAnimationVars(void) {}
static inline void DoMoveAnim(u16 move) {}
static inline void LaunchBattleAnimation(const u8 *const animsTable[], u16 tableId, bool8 isMoveAnim) {}
static inline void DestroyAnimSprite(struct Sprite *sprite) {}
static inline void DestroyAnimVisualTask(u8 taskId) {}
static inline void DestroyAnimSoundTask(u8 taskId) {}
static inline bool8 IsBattlerSpriteVisible(u8 battler) { return FALSE; }
static inline void MoveBattlerSpriteToBG(u8 battler, bool8 toBG_2, bool8 setSpriteInvisible) {}
static inline bool8 IsContest(void) { return FALSE; }
static inline s8 BattleAnimAdjustPanning(s8 pan) { return pan; }
static inline s8 BattleAnimAdjustPanning2(s8 pan) { return pan; }
static inline s16 KeepPanInRange(s16 panArg, int oldPan) { return panArg; }
static inline s16 CalculatePanIncrement(s16 sourcePan, s16 targetPan, s16 incrementPan) { return 0; }
static inline void RelocateBattleBgPal(u16 paletteNum, u16 *dest, u32 offset, bool8 largeScreen) {}
static inline void ResetBattleAnimBg(bool8 toBG2) {}
static inline void SetAnimBgAttribute(u8 bgId, u8 attributeId, u8 value) {}
static inline void DrawBattlerOnBg(int bgId, u8 x, u8 y, u8 battlerPosition, u8 paletteId, u8 *tiles, u16 *tilemap, u16 tilesOffset) {}
static inline void HandleIntroSlide(u8 environment) {}
static inline int GetAnimBgAttribute(u8 bgId, u8 attributeId) { return 0; }
static inline void TranslateSpriteInEllipse(struct Sprite *sprite) {}
static inline void AnimTranslateLinearAndFlicker(struct Sprite *sprite) {}
static inline void AnimTranslateLinearAndFlicker_Flipped(struct Sprite *sprite) {}
static inline void AnimWeatherBallUp(struct Sprite *sprite) {}
static inline void AnimWeatherBallDown(struct Sprite *sprite) {}
static inline void AnimSpinningSparkle(struct Sprite *sprite) {}
static inline void SetAverageBattlerPositions(u8 battler, bool8 respectMonPicOffsets, s16 *x, s16 *y) {}
static inline void DestroySpriteAndMatrix(struct Sprite *sprite) {}
static inline void TranslateSpriteLinearFixedPoint(struct Sprite *sprite) {}
static inline void InitSpritePosToAnimAttacker(struct Sprite *sprite, bool8 respectMonPicOffsets) {}
static inline void InitSpritePosToAnimTarget(struct Sprite *sprite, bool8 respectMonPicOffsets) {}
static inline void StartAnimLinearTranslation(struct Sprite *sprite) {}
static inline void InitAnimArcTranslation(struct Sprite *sprite) {}
static inline bool8 AnimTranslateLinear(struct Sprite *sprite) { return TRUE; }
static inline void TranslateAnimSpriteToTargetMonLocation(struct Sprite *sprite) {}
static inline u8 GetBattlerSpriteCoord2(u8 battler, u8 coordType) { return 0; }
static inline void InitAnimLinearTranslationWithSpeed(struct Sprite *sprite) {}
static inline u16 ArcTan2Neg(s16 x, s16 y) { return 0; }
static inline void TrySetSpriteRotScale(struct Sprite *sprite, bool8 recalcCenterVector, s16 xScale, s16 yScale, u16 rotation) {}
static inline void RunStoredCallbackWhenAffineAnimEnds(struct Sprite *sprite) {}
static inline void TranslateSpriteLinearAndFlicker(struct Sprite *sprite) {}
static inline void SetSpriteCoordsToAnimAttackerCoords(struct Sprite *sprite) {}
static inline void RunStoredCallbackWhenAnimEnds(struct Sprite *sprite) {}
static inline void SetAnimSpriteInitialXOffset(struct Sprite *sprite, s16 xOffset) {}
static inline s16 GetBattlerSpriteCoordAttr(u8 battler, u8 attr) { return 0; }
static inline u8 GetBattlerYCoordWithElevation(u8 battler) { return 0; }
static inline void WaitAnimForDuration(struct Sprite *sprite) {}
static inline void AnimTravelDiagonally(struct Sprite *sprite) {}
static inline void InitAnimLinearTranslation(struct Sprite *sprite) {}
static inline void AnimTranslateLinear_WithFollowup(struct Sprite *sprite) {}
static inline u8 GetBattlerSpriteBGPriority(u8 battler) { return 0; }
static inline void *LoadPointerFromVars(s16 lo, s16 hi) { return NULL; }
static inline void StorePointerInVars(s16 *lo, s16 *hi, const void *ptr) {}
static inline void InitPrioritiesForVisibleBattlers(void) {}
static inline void GetBattleAnimBg1Data(struct BattleAnimBgData *out) {}
static inline void GetBattleAnimBgData(struct BattleAnimBgData *out, u32 bgId) {}
static inline u8 GetBattlerSpriteSubpriority(u8 battler) { return 0; }
static inline bool8 TranslateAnimHorizontalArc(struct Sprite *sprite) { return TRUE; }
static inline void TranslateSpriteLinearByIdFixedPoint(struct Sprite *sprite) {}
static inline void ResetSpriteRotScale(u8 spriteId) {}
static inline void SetSpriteRotScale(u8 spriteId, s16 xScale, s16 yScale, u16 rotation) {}
static inline void InitSpriteDataForLinearTranslation(struct Sprite *sprite) {}
static inline void PrepareBattlerSpriteForRotScale(u8 spriteId, u8 objMode) {}
static inline void SetBattlerSpriteYOffsetFromRotation(u8 spriteId) {}
static inline u32 GetBattlePalettesMask(bool8 a, bool8 b, bool8 c, bool8 d, bool8 e, bool8 f, bool8 g) { return 0; }
static inline u32 GetBattleMonSpritePalettesMask(u8 a, u8 b, u8 c, u8 d) { return 0; }
static inline u8 GetSpritePalIdxByBattler(u8 battler) { return 0; }
static inline s16 CloneBattlerSpriteWithBlend(u8 animBattler) { return 0; }
static inline void DestroySpriteWithActiveSheet(struct Sprite *sprite) {}
static inline u8 CreateInvisibleSpriteCopy(int battler, u8 spriteId, int species) { return 0; }
static inline void AnimLoadCompressedBgTilemapHandleContest(struct BattleAnimBgData *data, const void *src, bool32 largeScreen) {}
static inline void AnimLoadCompressedBgGfx(u32 bgId, const u32 *src, u32 tilesOffset) {}
static inline void UpdateAnimBg3ScreenSize(bool8 largeScreenSize) {}
static inline void TranslateSpriteInGrowingCircle(struct Sprite *sprite) {}
static inline void SetBattlerSpriteYOffsetFromYScale(u8 spriteId) {}
static inline void PrepareEruptAnimTaskData(struct Task *task, u8 spriteId, s16 xScaleStart, s16 yScaleStart, s16 xScaleEnd, s16 yScaleEnd, u16 duration) {}
static inline u8 UpdateEruptAnimTask(struct Task *task) { return 0; }
static inline void DestroyAnimSpriteAndDisableBlend(struct Sprite *sprite) {}
static inline void AnimLoadCompressedBgTilemap(u32 bgId, const void *src) {}
static inline void InitAnimFastLinearTranslationWithSpeed(struct Sprite *sprite) {}
static inline bool8 AnimFastTranslateLinear(struct Sprite *sprite) { return TRUE; }
static inline void InitAndRunAnimFastLinearTranslation(struct Sprite *sprite) {}
static inline void TranslateSpriteLinearById(struct Sprite *sprite) {}
static inline void TranslateSpriteLinear(struct Sprite *sprite) {}
static inline void AnimSpriteOnMonPos(struct Sprite *sprite) {}
static inline void InitAnimLinearTranslationWithSpeedAndPos(struct Sprite *sprite) {}
static inline void TranslateSpriteInCircle(struct Sprite *sprite) {}
static inline void SetGrayscaleOrOriginalPalette(u16 paletteNum, bool8 restoreOriginalColor) {}
static inline void PrepareAffineAnimInTaskData(struct Task *task, u8 spriteId, const union AffineAnimCmd *affineAnimCmds) {}
static inline bool8 RunAffineAnimFromTaskData(struct Task *task) { return TRUE; }
static inline void AnimThrowProjectile(struct Sprite *sprite) {}
static inline void GetBgDataForTransform(struct BattleAnimBgData *out, u8 battler) {}
static inline u8 CreateAdditionalMonSpriteForMoveAnim(u16 species, bool8 isBackpic, u8 id, s16 x, s16 y, u8 subpriority, u32 personality, u32 trainerId, u32 battler, bool32 ignoreDeoxysForm) { return 0; }
static inline void ResetSpriteRotScale_PreserveAffine(struct Sprite *sprite) {}
static inline void Trade_MoveSelectedMonToTarget(struct Sprite *sprite) {}
static inline void DestroyAnimVisualTaskAndDisableBlend(u8 taskId) {}
static inline void DestroySpriteAndFreeResources_(struct Sprite *sprite) {}
static inline void SetBattlerSpriteYOffsetFromOtherYScale(u8 spriteId, u8 otherSpriteId) {}
static inline u8 GetBattlerSide(u8 battler) { return (battler & BIT_SIDE); }
static inline u8 GetBattlerPosition(u8 battler) { return battler; }
static inline u8 GetBattlerAtPosition(u8 position) { return position; }
static inline void ConvertPosDataToTranslateLinearData(struct Sprite *sprite) {}
static inline void InitAnimFastLinearTranslationWithSpeedAndPos(struct Sprite *sprite) {}
static inline u8 GetBattlerSpriteCoord(u8 battler, u8 coordType) { return 0; }
static inline bool8 IsBattlerSpritePresent(u8 battler) { return FALSE; }
static inline void ClearBattleAnimBg(u32 bgId) {}
static inline u8 GetAnimBattlerSpriteId(u8 animBattler) { return 0; }
static inline bool8 IsDoubleBattle(void) { return FALSE; }
static inline u8 GetBattleBgPaletteNum(void) { return 0; }
static inline u8 GetBattlerSpriteBGPriorityRank(u8 battler) { return 0; }
static inline void StoreSpriteCallbackInData6(struct Sprite *sprite, void (*callback)(struct Sprite *)) {}
static inline void SetSpritePrimaryCoordsFromSecondaryCoords(struct Sprite *sprite) {}
static inline u8 GetBattlerSpriteDefault_Y(u8 battler) { return 0; }
static inline u8 GetSubstituteSpriteDefault_Y(u8 battler) { return 0; }
static inline void AnimParticleBurst(struct Sprite *sprite) {}
static inline void AnimWaterPulseRing(struct Sprite *sprite) {}
static inline void DestroyAnimSpriteAfterTimer(struct Sprite *sprite) {}
static inline u8 SmokescreenImpact(s16 x, s16 y, bool8 persist) { return 0; }
static inline u32 UnpackSelectedBattlePalettes(s16 selector) { return 0; }
static inline u8 GetBattlerSpriteFinal_Y(u8 battler, u16 species, bool8 a3) { return 0; }

/* Dummy extern data - defined in stubs.c */
extern const struct OamData gOamData_AffineOff_ObjNormal_8x16;
extern const struct OamData gOamData_AffineNormal_ObjBlend_16x16;
extern const struct OamData gOamData_AffineOff_ObjNormal_8x8;
extern const struct OamData gOamData_AffineDouble_ObjNormal_8x8;
extern const struct OamData gOamData_AffineOff_ObjNormal_16x16;
extern const struct OamData gOamData_AffineOff_ObjNormal_32x16;
extern const struct OamData gOamData_AffineNormal_ObjNormal_32x32;
extern const struct OamData gOamData_AffineNormal_ObjNormal_64x32;
extern const struct OamData gOamData_AffineDouble_ObjNormal_16x16;
extern const struct OamData gOamData_AffineOff_ObjNormal_32x32;
extern const struct OamData gOamData_AffineNormal_ObjNormal_16x16;
extern const struct OamData gOamData_AffineOff_ObjBlend_32x32;
extern const struct OamData gOamData_AffineOff_ObjBlend_64x64;
extern const struct OamData gOamData_AffineNormal_ObjBlend_32x32;
extern const struct OamData gOamData_AffineOff_ObjNormal_16x32;
extern const struct OamData gOamData_AffineDouble_ObjBlend_8x8;
extern const struct OamData gOamData_AffineDouble_ObjNormal_32x32;
extern const struct OamData gOamData_AffineNormal_ObjBlend_64x64;
extern const struct OamData gOamData_AffineNormal_ObjBlend_32x64;
extern const struct OamData gOamData_AffineDouble_ObjBlend_32x16;
extern const struct OamData gOamData_AffineOff_ObjBlend_32x16;
extern const struct OamData gOamData_AffineDouble_ObjNormal_16x32;
extern const struct OamData gOamData_AffineDouble_ObjNormal_32x64;
extern const struct OamData gOamData_AffineNormal_ObjNormal_32x64;
extern const struct OamData gOamData_AffineDouble_ObjBlend_32x32;
extern const struct OamData gOamData_AffineDouble_ObjNormal_64x64;
extern const struct OamData gOamData_AffineDouble_ObjBlend_64x64;
extern const struct OamData gOamData_AffineDouble_ObjBlend_64x32;
extern const struct OamData gOamData_AffineDouble_ObjNormal_8x16;
extern const struct OamData gOamData_AffineOff_ObjBlend_16x16;
extern const struct OamData gOamData_AffineDouble_ObjBlend_16x16;
extern const struct OamData gOamData_AffineNormal_ObjNormal_8x8;
extern const struct OamData gOamData_AffineDouble_ObjBlend_8x16;
extern const struct OamData gOamData_AffineOff_ObjBlend_8x8;
extern const struct OamData gOamData_AffineNormal_ObjBlend_8x16;
extern const struct OamData gOamData_AffineNormal_ObjBlend_8x8;
extern const struct OamData gOamData_AffineOff_ObjBlend_8x16;
extern const struct OamData gOamData_AffineOff_ObjNormal_64x64;
extern const struct OamData gOamData_AffineOff_ObjNormal_32x64;
extern const struct OamData gOamData_AffineNormal_ObjNormal_64x64;
extern const struct OamData gOamData_AffineDouble_ObjNormal_32x16;
extern const struct OamData gOamData_AffineOff_ObjNormal_64x32;
extern const struct OamData gOamData_AffineOff_ObjBlend_64x32;
extern const struct OamData gOamData_AffineOff_ObjBlend_16x32;

#endif /* GUARD_BATTLE_ANIM_H */
