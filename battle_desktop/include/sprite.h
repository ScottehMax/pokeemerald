#ifndef GUARD_SPRITE_H
#define GUARD_SPRITE_H

#include "gba/types.h"

#define OAM_MATRIX_COUNT 32
#define MAX_SPRITES 64
#define SPRITE_NONE 0xFF
#define TAG_NONE 0xFFFF
#define NO_ANCHOR 0x800

struct SpriteSheet       { const void *data; u16 size; u16 tag; };
struct CompressedSpriteSheet { const u32 *data; u16 size; u16 tag; };
struct SpriteFrameImage  { const void *data; u16 size; };
struct SpritePalette     { const u16 *data; u16 tag; };
struct CompressedSpritePalette { const u32 *data; u16 tag; };

#define obj_frame_tiles(ptr) {.data = (u8 *)ptr, .size = sizeof ptr}
#define overworld_frame(ptr, width, height, frame) {.data = (u8 *)ptr + (width * height * frame * 64)/2, .size = (width * height * 64)/2}

struct AnimFrameCmd  { u32 imageValue:16; u32 duration:6; u32 hFlip:1; u32 vFlip:1; };
struct AnimLoopCmd   { u32 type:16; u32 count:6; };
struct AnimJumpCmd   { u32 type:16; u32 target:6; };
union AnimCmd        { s16 type; struct AnimFrameCmd frame; struct AnimLoopCmd loop; struct AnimJumpCmd jump; };

#define ANIMCMD_FRAME(...) {.frame = {__VA_ARGS__}}
#define ANIMCMD_LOOP(_count) {.loop = {.type = -3, .count = _count}}
#define ANIMCMD_JUMP(_target) {.jump = {.type = -2, .target = _target}}
#define ANIMCMD_END {.type = -1}

struct AffineAnimFrameCmd { s16 xScale; s16 yScale; u8 rotation; u8 duration; };
struct AffineAnimLoopCmd  { s16 type; s16 count; };
struct AffineAnimJumpCmd  { s16 type; u16 target; };
struct AffineAnimEndCmdAlt { s16 type; u16 val; };
union AffineAnimCmd { s16 type; struct AffineAnimFrameCmd frame; struct AffineAnimLoopCmd loop; struct AffineAnimJumpCmd jump; struct AffineAnimEndCmdAlt end; };

#define AFFINEANIMCMDTYPE_LOOP 0x7FFD
#define AFFINEANIMCMDTYPE_JUMP 0x7FFE
#define AFFINEANIMCMDTYPE_END  0x7FFF
#define AFFINEANIMCMD_FRAME(_xScale, _yScale, _rotation, _duration) {.frame = {.xScale = _xScale, .yScale = _yScale, .rotation = _rotation, .duration = _duration}}
#define AFFINEANIMCMD_LOOP(_count) {.loop = {.type = AFFINEANIMCMDTYPE_LOOP, .count = _count}}
#define AFFINEANIMCMD_JUMP(_target) {.jump = {.type = AFFINEANIMCMDTYPE_JUMP, .target = _target}}
#define AFFINEANIMCMD_END {.type = AFFINEANIMCMDTYPE_END}
#define AFFINEANIMCMD_END_ALT(_val) {.end = {.type = AFFINEANIMCMDTYPE_END, .val = _val}}

struct AffineAnimState { u8 animNum; u8 animCmdIndex; u8 delayCounter; u8 loopCounter; s16 xScale; s16 yScale; u16 rotation; };

enum { SUBSPRITES_OFF, SUBSPRITES_ON, SUBSPRITES_IGNORE_PRIORITY };

struct Subsprite { s8 x; s8 y; u16 shape:2; u16 size:2; u16 tileOffset:10; u16 priority:2; };
struct SubspriteTable { u8 subspriteCount; const struct Subsprite *subsprites; };

struct Sprite;
typedef void (*SpriteCallback)(struct Sprite *);

struct SpriteTemplate {
    u16 tileTag;
    u16 paletteTag;
    const struct OamData *oam;
    const union AnimCmd *const *anims;
    const struct SpriteFrameImage *images;
    const union AffineAnimCmd *const *affineAnims;
    SpriteCallback callback;
};

struct Sprite {
    struct OamData oam;
    const union AnimCmd *const *anims;
    const struct SpriteFrameImage *images;
    const union AffineAnimCmd *const *affineAnims;
    const struct SpriteTemplate *template;
    const struct SubspriteTable *subspriteTables;
    SpriteCallback callback;
    s16 x, y;
    s16 x2, y2;
    s8 centerToCornerVecX;
    s8 centerToCornerVecY;
    u8 animNum;
    u8 animCmdIndex;
    u8 animDelayCounter:6;
    bool8 animPaused:1;
    bool8 affineAnimPaused:1;
    u8 animLoopCounter;
    s16 data[8];
    bool16 inUse:1;
    bool16 coordOffsetEnabled:1;
    bool16 invisible:1;
    bool16 flags_3:1;
    bool16 flags_4:1;
    bool16 flags_5:1;
    bool16 flags_6:1;
    bool16 flags_7:1;
    bool16 hFlip:1;
    bool16 vFlip:1;
    bool16 animBeginning:1;
    bool16 affineAnimBeginning:1;
    bool16 animEnded:1;
    bool16 affineAnimEnded:1;
    bool16 usingSheet:1;
    bool16 anchored:1;
    u16 sheetTileStart;
    u8 subspriteTableNum:6;
    u8 subspriteMode:2;
    u8 subpriority;
};

struct OamMatrix { s16 a; s16 b; s16 c; s16 d; };

extern const struct OamData gDummyOamData;
extern const union AnimCmd *const gDummySpriteAnimTable[];
extern const union AffineAnimCmd *const gDummySpriteAffineAnimTable[];
extern const struct SpriteTemplate gDummySpriteTemplate;
extern u8 gReservedSpritePaletteCount;
extern struct Sprite gSprites[MAX_SPRITES + 1];
extern u8 gOamLimit;
extern u16 gReservedSpriteTileCount;
extern s16 gSpriteCoordOffsetX;
extern s16 gSpriteCoordOffsetY;
extern struct OamMatrix gOamMatrices[OAM_MATRIX_COUNT];
extern bool8 gAffineAnimsDisabled;

void ResetSpriteData(void);
void AnimateSprites(void);
void BuildOamBuffer(void);
u8 CreateSprite(const struct SpriteTemplate *template, s16 x, s16 y, u8 subpriority);
u8 CreateSpriteAtEnd(const struct SpriteTemplate *template, s16 x, s16 y, u8 subpriority);
u8 CreateInvisibleSprite(SpriteCallback callback);
u8 CreateSpriteAndAnimate(const struct SpriteTemplate *template, s16 x, s16 y, u8 subpriority);
void DestroySprite(struct Sprite *sprite);
void ResetOamRange(u8 start, u8 end);
void LoadOam(void);
void SetSpriteTileNum(struct Sprite *sprite, u16 tileNum);
void CalcCenterToCornerVec(struct Sprite *sprite, u8 shape, u8 size, u8 affineMode);
void SpriteCallbackDummy(struct Sprite *sprite);
void ProcessSpriteCopyRequests(void);
void RequestSpriteFrameImageCopy(u16 index, u16 tileNum, const struct SpriteFrameImage *images);
void RequestSpriteCopy(const void *src, void *dst, u16 size);
void CopyFromOamBuffer(u8 start, u8 end);
void FreeSpriteTilesByTag(u16 tag);
void FreeSpritePaletteByTag(u16 tag);
void FreeSpriteOamMatrix(struct Sprite *sprite);
u16 LoadSpriteSheet(const struct SpriteSheet *sheet);
void LoadSpriteSheets(const struct SpriteSheet *sheets);
void FreeAllSpritePalettes(void);
u8 LoadSpritePalette(const struct SpritePalette *palette);
void LoadSpritePalettes(const struct SpritePalette *palettes);
void StartSpriteAnim(struct Sprite *sprite, u8 animNum);
void StartSpriteAnimIfDifferent(struct Sprite *sprite, u8 animNum);
void SeekSpriteAnim(struct Sprite *sprite, u8 animCmdIndex);
void StartSpriteAffineAnim(struct Sprite *sprite, u8 animNum);
void StartSpriteAffineAnimIfDifferent(struct Sprite *sprite, u8 animNum);
void ChangeSpriteAffineAnim(struct Sprite *sprite, u8 animNum);
void ChangeSpriteAffineAnimIfDifferent(struct Sprite *sprite, u8 animNum);
void SetSpriteMatrixAnchor(struct Sprite *sprite, s16 x, s16 y);
void SetAffineSpriteTileRange(u8 startId, u8 count, u8 matrixNum);
void FreeOamMatrix(u8 matrixNum);
u8 AllocOamMatrix(void);
void InitSpriteAffineAnim(struct Sprite *sprite);
void SetSpriteAffineMatrix(u8 matrixNum, struct ObjAffineSrcData *src);
u16 LoadCompressedSpriteSheet(const struct CompressedSpriteSheet *sheet);
void LoadCompressedSpriteSheets(const struct CompressedSpriteSheet *sheets);
u8 LoadCompressedSpritePalette(const struct CompressedSpritePalette *palette);
void LoadCompressedSpritePalettes(const struct CompressedSpritePalette *palettes);
bool8 CheckIfSpriteTileTagExists(u16 tag);
u16 GetSpriteTileStartByTag(u16 tag);
u8 GetSpritePaletteTagByPaletteNum(u8 paletteNum);
u8 GetSpritePaletteNumByTag(u16 tag);
void RequestSpriteCopy2(const void *src, void *dst, u16 size);
void SetSubspriteTables(struct Sprite *sprite, const struct SubspriteTable *subspriteTables);
bool8 IsContest(void);

#endif // GUARD_SPRITE_H
