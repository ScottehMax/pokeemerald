#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_interface.h"
#include "battle_overworld_scene.h"
#include "battle_setup.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "event_object_movement.h"
#include "fieldmap.h"
#include "gpu_regs.h"
#include "gba/isagbprint.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "task.h"
#include "constants/battle.h"
#include "constants/battle_anim.h"
#include "constants/global.h"
#include "constants/rgb.h"
#include "constants/species.h"
#include "constants/trainers.h"

#define OW_FRAME_32_SIZE (32 * 32 / 2)
#define OW_FRAME_64_SIZE (64 * 64 / 2)
#define OW_TRAINER_PLAYER_PAL_SLOT 12
#define OW_TRAINER_OPPONENT_PAL_SLOT 13
#define OW_SCENE_BASE_Y 56
#define OW_BG_ROUTE101_X 3
#define OW_BG_ROUTE101_Y 7
#define OW_BG_ROUTE101_WIDTH 15
#define OW_BG_ROUTE101_HEIGHT 10
#define OW_BG_PAL_UNMAPPED 0xFF
#define OW_BG_TILE_UNMAPPED 0xFFFF
#define OW_BG_TILE_RANGE_1_START 0x0C0
#define OW_BG_TILE_RANGE_1_END 0x100
#define OW_BG_TILE_RANGE_2_START 0x100
#define OW_BG_TILE_RANGE_2_END 0x100
#define OW_BG_CHARBASE 3
#define OW_BG_LOWER_SCREENBASE 26
#define OW_BG_LOWER_ID 3
#define OW_BG_COMPOSITE_TILE_CAPACITY 32
#define OW_BG_COMPOSITE_PAL_CAPACITY 4
#define OW_BG_COMPOSITE_PAL_START 12

struct BattleOwMonGfx
{
    const u32 *gfx;
    const u16 *pal;
    const u16 *shinyPal;
    const struct OamData *oam;
    const struct SpriteFrameImage *images;
    u16 frameSize;
};

extern const struct MapLayout Route101_Layout;

extern const struct OamData gObjectEventBaseOam_16x32;
extern const struct OamData gObjectEventBaseOam_64x64;

#define OW_MON_ENTRY_32(species)                                                              \
    [SPECIES_##species] = {sOwPic_##species, sOwPal_##species, sOwShinyPal_##species,          \
                           &gObjectEventBaseOam_32x32, sPicTable_##species, OW_FRAME_32_SIZE}

#define OW_MON_ENTRY_64(species)                                                              \
    [SPECIES_##species] = {sOwPic_##species, sOwPal_##species, sOwShinyPal_##species,          \
                           &gObjectEventBaseOam_64x64, sPicTable_##species, OW_FRAME_64_SIZE}

#define OW_MON_FRAMES(species, width, height)                                                  \
    static const struct SpriteFrameImage sPicTable_##species[] =                                \
    {                                                                                           \
        overworld_frame(sOwPic_##species, width, height, 0),                                    \
        overworld_frame(sOwPic_##species, width, height, 1),                                    \
        overworld_frame(sOwPic_##species, width, height, 2),                                    \
        overworld_frame(sOwPic_##species, width, height, 0),                                    \
        overworld_frame(sOwPic_##species, width, height, 3),                                    \
        overworld_frame(sOwPic_##species, width, height, 1),                                    \
        overworld_frame(sOwPic_##species, width, height, 4),                                    \
        overworld_frame(sOwPic_##species, width, height, 2),                                    \
        overworld_frame(sOwPic_##species, width, height, 5),                                    \
    }

#include "data/battle_overworld_pokemon.h"

extern const u32 gObjectEventPic_BrendanNormal[];
extern const u32 gObjectEventPic_MayNormal[];
extern const u16 gObjectEventPal_Brendan[];
extern const u16 gObjectEventPal_May[];
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_AquaMemberM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Beauty;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_BlackBelt;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Boy3;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Brandon;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_BugCatcher;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Camper;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_ExpertM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Fisherman;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Gentleman;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Greta;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_HexManiac;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Hiker;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Lass;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Lucy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_MagmaMemberM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man1;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man3;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Maniac;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_NinjaBoy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Noland;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Picnicker;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_PokefanM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_PsychicM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_ReporterM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RichBoy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RivalBrendanNormal;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Roxanne;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RubySapphireBrendan;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RunningTriathleteM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Sailor;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_SchoolKidM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Sidney;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Spenser;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Steven;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_SwimmerF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_SwimmerM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_TuberF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_TuberM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Tucker;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Twin;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Woman1;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Woman5;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Youngster;

static const struct SpriteFrameImage sPicTable_BattleBrendan[] =
{
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 0),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 1),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 2),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 3),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 4),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 5),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 6),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 7),
    overworld_frame(gObjectEventPic_BrendanNormal, 2, 4, 8),
};

static const struct SpriteFrameImage sPicTable_BattleMay[] =
{
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 0),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 1),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 2),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 3),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 4),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 5),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 6),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 7),
    overworld_frame(gObjectEventPic_MayNormal, 2, 4, 8),
};

static const struct SpriteTemplate sTrainerTemplate =
{
    .tileTag = TAG_NONE,
    .paletteTag = TAG_NONE,
    .oam = &gObjectEventBaseOam_16x32,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sAnim_BattleOwMonFaceWest[] =
{
    ANIMCMD_FRAME(2, 16),
    ANIMCMD_FRAME(8, 16),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sAnim_BattleOwMonFaceEast[] =
{
    ANIMCMD_FRAME(2, 16, .hFlip = TRUE),
    ANIMCMD_FRAME(8, 16, .hFlip = TRUE),
    ANIMCMD_JUMP(0),
};

static const union AnimCmd sAnim_BattleTrainerFaceWest[] =
{
    ANIMCMD_FRAME(2, 16),
    ANIMCMD_END,
};

static const union AnimCmd sAnim_BattleTrainerFaceEast[] =
{
    ANIMCMD_FRAME(2, 16, .hFlip = TRUE),
    ANIMCMD_END,
};

static const union AnimCmd *const sAnimTable_BattleOwMonFaceWest[] =
{
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
    sAnim_BattleOwMonFaceWest,
};

static const union AnimCmd *const sAnimTable_BattleOwMonFaceEast[] =
{
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
    sAnim_BattleOwMonFaceEast,
};

static const union AnimCmd *const sAnimTable_BattleTrainerFaceWest[] =
{
    sAnim_BattleTrainerFaceWest,
};

static const union AnimCmd *const sAnimTable_BattleTrainerFaceEast[] =
{
    sAnim_BattleTrainerFaceEast,
};

static bool8 IsBattleOverworldSceneEnabled(void);
static void BattleOverworldScene_ApplyBg3Config(void);
static const struct ObjectEventGraphicsInfo *GetBattleOwTrainerGraphicsInfo(u8 trainerClass);
static void Task_BattleOverworldScene_WildShinyAnimations(u8 taskId);

static u8 sPlayerTrainerSpriteId;
static u8 sOpponentTrainerSpriteId;
static u8 sOwBattlerSpriteIds[MAX_BATTLERS_COUNT];
static bool8 sOwBattlerHiddenByBall[MAX_BATTLERS_COUNT];
static bool8 sCreatedTrainerSprites;
static bool8 sSceneSuspended;
static bool8 sReshowTransitionAllowsBg3Blend;
static bool8 sBg3BlendFadeStarted;
static bool8 sSceneVisible;
static u16 sCompositeLowerTiles[OW_BG_COMPOSITE_TILE_CAPACITY];
static u16 sCompositeUpperTiles[OW_BG_COMPOSITE_TILE_CAPACITY];
static u16 sCompositeDestTiles[OW_BG_COMPOSITE_TILE_CAPACITY];
static u8 sCompositeCount;
static u8 sCompositePalLower[OW_BG_COMPOSITE_PAL_CAPACITY];
static u8 sCompositePalUpper[OW_BG_COMPOSITE_PAL_CAPACITY];
static u16 sCompositePalettes[OW_BG_COMPOSITE_PAL_CAPACITY][16];
static u8 sCompositePalColorCounts[OW_BG_COMPOSITE_PAL_CAPACITY];
static u8 sCompositePalCount;
static EWRAM_DATA u16 sBattleOwBgTileMap[NUM_TILES_TOTAL] = {0};
static EWRAM_DATA u32 sBattleOwBgPaletteMask = 0;
static const u8 sBattleOwBgFreePalSlots[] = {2, 3, 4, 6, 7, 10, 11};

static bool8 IsPlayerBattlerPosition(u8 battlerPosition)
{
    return (battlerPosition == B_POSITION_PLAYER_LEFT || battlerPosition == B_POSITION_PLAYER_RIGHT);
}

static u16 RemapBgTile(u16 tile, const u8 *palMap)
{
    u16 pal;
    u16 tileNum;

    if (tile == 0)
        return 0;

    pal = palMap[tile >> 12];
    tileNum = sBattleOwBgTileMap[tile & 0x3FF];
    if (pal == OW_BG_PAL_UNMAPPED)
        pal = 0;
    if (tileNum == OW_BG_TILE_UNMAPPED)
        tileNum = 0;

    return (tile & 0x0C00) | tileNum | (pal << 12);
}

static u8 GetTilePixel(const u8 *tiles, u16 tile, u8 x, u8 y)
{
    u8 srcX = (tile & 0x0400) ? 7 - x : x;
    u8 srcY = (tile & 0x0800) ? 7 - y : y;
    const u8 *src = tiles + (tile & 0x03FF) * TILE_SIZE_4BPP;
    u8 pixel = src[srcY * 4 + srcX / 2];

    if (srcX & 1)
        return pixel >> 4;
    else
        return pixel & 0x0F;
}

static void SetTilePixel(u8 *tile, u8 x, u8 y, u8 pixel)
{
    if (x & 1)
        tile[y * 4 + x / 2] = (tile[y * 4 + x / 2] & 0x0F) | (pixel << 4);
    else
        tile[y * 4 + x / 2] = (tile[y * 4 + x / 2] & 0xF0) | pixel;
}

static u8 GetCompositePalette(u8 lowerPal, u8 upperPal)
{
    u8 i;

    for (i = 0; i < sCompositePalCount; i++)
    {
        if (sCompositePalLower[i] == lowerPal && sCompositePalUpper[i] == upperPal)
            return i;
    }

    if (sCompositePalCount >= OW_BG_COMPOSITE_PAL_CAPACITY)
        return 0;

    i = sCompositePalCount++;
    sCompositePalLower[i] = lowerPal;
    sCompositePalUpper[i] = upperPal;
    sCompositePalettes[i][0] = Route101_Layout.primaryTileset->palettes[lowerPal][0];
    sCompositePalColorCounts[i] = 1;
    return i;
}

static u8 AddCompositeColor(u8 palId, u16 color)
{
    u8 i;

    for (i = 0; i < sCompositePalColorCounts[palId]; i++)
    {
        if (sCompositePalettes[palId][i] == color)
            return i;
    }

    if (sCompositePalColorCounts[palId] >= 16)
        return 0;

    i = sCompositePalColorCounts[palId]++;
    sCompositePalettes[palId][i] = color;
    return i;
}

static u8 FindCompositeColor(u8 palId, u16 color)
{
    u8 i;

    for (i = 0; i < sCompositePalColorCounts[palId]; i++)
    {
        if (sCompositePalettes[palId][i] == color)
            return i;
    }

    return AddCompositeColor(palId, color);
}

static void RegisterCompositePaletteColors(const u8 *tiles, u16 lowerTile, u16 upperTile)
{
    u8 x;
    u8 y;
    u8 lowerPixel;
    u8 upperPixel;
    u8 lowerPal = lowerTile >> 12;
    u8 upperPal = upperTile >> 12;
    u8 palId = GetCompositePalette(lowerPal, upperPal);
    u16 color;

    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 8; x++)
        {
            lowerPixel = GetTilePixel(tiles, lowerTile, x, y);
            upperPixel = GetTilePixel(tiles, upperTile, x, y);
            if (upperPixel != 0)
                color = Route101_Layout.primaryTileset->palettes[upperPal][upperPixel];
            else
                color = Route101_Layout.primaryTileset->palettes[lowerPal][lowerPixel];
            AddCompositeColor(palId, color);
        }
    }
}

static void LoadCompositePalettes(void)
{
    u8 i;

    for (i = 0; i < sCompositePalCount; i++)
        LoadPalette(sCompositePalettes[i], BG_PLTT_ID(OW_BG_COMPOSITE_PAL_START + i), PLTT_SIZE_4BPP);
}

static void UpdateRoute101PaletteMask(const u8 *palMap)
{
    u8 i;

    sBattleOwBgPaletteMask = 0;

    for (i = 0; i < 16; i++)
    {
        if (palMap[i] != OW_BG_PAL_UNMAPPED)
            sBattleOwBgPaletteMask |= 1 << palMap[i];
    }

    for (i = 0; i < sCompositePalCount; i++)
        sBattleOwBgPaletteMask |= 1 << (OW_BG_COMPOSITE_PAL_START + i);
}

static u16 GetCompositeTile(u16 lowerTile, u16 upperTile)
{
    u8 i;

    for (i = 0; i < sCompositeCount; i++)
    {
        if (sCompositeLowerTiles[i] == lowerTile && sCompositeUpperTiles[i] == upperTile)
            return sCompositeDestTiles[i];
    }

    return 0;
}

static u16 RemapCompositeTile(u16 lowerTile, u16 upperTile)
{
    u8 palId = GetCompositePalette(lowerTile >> 12, upperTile >> 12);
    return GetCompositeTile(lowerTile, upperTile) | ((OW_BG_COMPOSITE_PAL_START + palId) << 12);
}

static const u16 *GetRoute101Metatile(u16 metatileId)
{
    if (metatileId < NUM_METATILES_IN_PRIMARY)
        return Route101_Layout.primaryTileset->metatiles + metatileId * NUM_TILES_PER_METATILE;

    return Route101_Layout.secondaryTileset->metatiles + (metatileId - NUM_METATILES_IN_PRIMARY) * NUM_TILES_PER_METATILE;
}

static void DrawRoute101Metatile(u16 *lowerBg, u8 x, u8 y, const u16 *metatile, const u8 *palMap)
{
    u16 offset = y * 2 * 32 + x * 2;

    lowerBg[offset] = (metatile[4] != 0) ? RemapCompositeTile(metatile[0], metatile[4]) : RemapBgTile(metatile[0], palMap);
    lowerBg[offset + 1] = (metatile[5] != 0) ? RemapCompositeTile(metatile[1], metatile[5]) : RemapBgTile(metatile[1], palMap);
    lowerBg[offset + 32] = (metatile[6] != 0) ? RemapCompositeTile(metatile[2], metatile[6]) : RemapBgTile(metatile[2], palMap);
    lowerBg[offset + 33] = (metatile[7] != 0) ? RemapCompositeTile(metatile[3], metatile[7]) : RemapBgTile(metatile[3], palMap);

}

static void LoadRoute101Palette(u8 srcPal, u8 destPal)
{
    if (srcPal < NUM_PALS_IN_PRIMARY)
        LoadPalette(Route101_Layout.primaryTileset->palettes[srcPal], BG_PLTT_ID(destPal), PLTT_SIZE_4BPP);
    else
        LoadPalette(Route101_Layout.secondaryTileset->palettes[srcPal], BG_PLTT_ID(destPal), PLTT_SIZE_4BPP);
}

static void TryAssignRoute101Palette(const u16 *metatile, u8 *palMap, u8 *nextPalSlot)
{
    u8 i;
    u8 srcPal;

    for (i = 0; i < NUM_TILES_PER_METATILE; i++)
    {
        if (metatile[i] == 0)
            continue;

        srcPal = metatile[i] >> 12;
        if (palMap[srcPal] == OW_BG_PAL_UNMAPPED)
        {
            if (*nextPalSlot >= ARRAY_COUNT(sBattleOwBgFreePalSlots))
                palMap[srcPal] = 0;
            else
            {
                palMap[srcPal] = sBattleOwBgFreePalSlots[*nextPalSlot];
                LoadRoute101Palette(srcPal, palMap[srcPal]);
                (*nextPalSlot)++;
            }
        }
    }
}

static void TryAssignRoute101Tile(const u16 *metatile, u16 *nextTile)
{
    u8 i;
    u16 srcTile;

    for (i = 0; i < 4; i++)
    {
        if (metatile[i] == 0)
            continue;

        srcTile = metatile[i] & 0x3FF;
        if (sBattleOwBgTileMap[srcTile] == OW_BG_TILE_UNMAPPED)
        {
            if (*nextTile >= OW_BG_TILE_RANGE_1_END && *nextTile < OW_BG_TILE_RANGE_2_START)
                *nextTile = OW_BG_TILE_RANGE_2_START;

            if (*nextTile >= OW_BG_TILE_RANGE_2_END)
                sBattleOwBgTileMap[srcTile] = 0;
            else
            {
                sBattleOwBgTileMap[srcTile] = (*nextTile)++;
            }
        }
    }
}

static void TryAssignCompositeTile(const u16 *metatile, u16 *nextTile)
{
    u8 i;

    for (i = 0; i < 4; i++)
    {
        if (metatile[i + 4] == 0)
            continue;
        if (GetCompositeTile(metatile[i], metatile[i + 4]) != 0)
            continue;

        if (*nextTile >= OW_BG_TILE_RANGE_1_END && *nextTile < OW_BG_TILE_RANGE_2_START)
            *nextTile = OW_BG_TILE_RANGE_2_START;
        if (*nextTile >= OW_BG_TILE_RANGE_2_END || sCompositeCount >= OW_BG_COMPOSITE_TILE_CAPACITY)
            continue;

        sCompositeLowerTiles[sCompositeCount] = metatile[i];
        sCompositeUpperTiles[sCompositeCount] = metatile[i + 4];
        sCompositeDestTiles[sCompositeCount] = (*nextTile)++;
        sCompositeCount++;
    }
}

static void BuildRoute101TileAndPaletteMaps(u8 *palMap)
{
    u8 i;
    u8 x;
    u8 y;
    u8 nextPalSlot = 0;
    u16 metatileId;
    u16 nextTile = OW_BG_TILE_RANGE_1_START;

    for (i = 0; i < 16; i++)
        palMap[i] = OW_BG_PAL_UNMAPPED;
    for (metatileId = 0; metatileId < NUM_TILES_TOTAL; metatileId++)
        sBattleOwBgTileMap[metatileId] = OW_BG_TILE_UNMAPPED;
    for (i = 0; i < OW_BG_COMPOSITE_TILE_CAPACITY; i++)
    {
        sCompositeLowerTiles[i] = 0;
        sCompositeUpperTiles[i] = 0;
        sCompositeDestTiles[i] = 0;
    }
    sCompositeCount = 0;

    for (y = 0; y < OW_BG_ROUTE101_HEIGHT; y++)
    {
        for (x = 0; x < OW_BG_ROUTE101_WIDTH; x++)
        {
            metatileId = Route101_Layout.map[(OW_BG_ROUTE101_Y + y) * Route101_Layout.width + OW_BG_ROUTE101_X + x] & MAPGRID_METATILE_ID_MASK;
            if (metatileId >= NUM_METATILES_TOTAL)
                metatileId = 0;
            TryAssignRoute101Palette(GetRoute101Metatile(metatileId), palMap, &nextPalSlot);
            TryAssignRoute101Tile(GetRoute101Metatile(metatileId), &nextTile);
            TryAssignCompositeTile(GetRoute101Metatile(metatileId), &nextTile);
        }
    }
}

static void CopyMappedRoute101Tiles(const u8 *tiles, u16 firstTile, u16 numTiles)
{
    u16 i;
    u16 destTile;

    for (i = 0; i < numTiles; i++)
    {
        destTile = sBattleOwBgTileMap[firstTile + i];
        if (destTile != OW_BG_TILE_UNMAPPED && destTile != 0)
            CpuCopy32(tiles + i * TILE_SIZE_4BPP,
                      (void *)(BG_CHAR_ADDR(OW_BG_CHARBASE) + TILE_OFFSET_4BPP(destTile)),
                      TILE_SIZE_4BPP);
    }
}

static void ComposeRoute101Tile(const u8 *tiles, u16 lowerTile, u16 upperTile, u8 *dest)
{
    u8 x;
    u8 y;
    u8 lowerPixel;
    u8 upperPixel;
    u8 lowerPal = lowerTile >> 12;
    u8 upperPal = upperTile >> 12;
    u8 palId = GetCompositePalette(lowerPal, upperPal);
    u16 color;

    CpuFill32(0, dest, TILE_SIZE_4BPP);
    for (y = 0; y < 8; y++)
    {
        for (x = 0; x < 8; x++)
        {
            lowerPixel = GetTilePixel(tiles, lowerTile, x, y);
            upperPixel = GetTilePixel(tiles, upperTile, x, y);
            if (upperPixel != 0)
                color = Route101_Layout.primaryTileset->palettes[upperPal][upperPixel];
            else
                color = Route101_Layout.primaryTileset->palettes[lowerPal][lowerPixel];
            SetTilePixel(dest, x, y, FindCompositeColor(palId, color));
        }
    }
}

static void LoadCompositeRoute101Tiles(const u8 *tiles)
{
    u8 x;
    u8 y;
    u8 i;
    u8 tileGfx[TILE_SIZE_4BPP];
    u16 metatileId;
    const u16 *metatile;

    sCompositePalCount = 0;
    for (i = 0; i < OW_BG_COMPOSITE_PAL_CAPACITY; i++)
    {
        sCompositePalLower[i] = 0xFF;
        sCompositePalUpper[i] = 0xFF;
        sCompositePalColorCounts[i] = 0;
    }

    for (y = 0; y < OW_BG_ROUTE101_HEIGHT; y++)
    {
        for (x = 0; x < OW_BG_ROUTE101_WIDTH; x++)
        {
            metatileId = Route101_Layout.map[(OW_BG_ROUTE101_Y + y) * Route101_Layout.width + OW_BG_ROUTE101_X + x] & MAPGRID_METATILE_ID_MASK;
            if (metatileId >= NUM_METATILES_IN_PRIMARY)
                continue;

            metatile = GetRoute101Metatile(metatileId);
            for (i = 0; i < 4; i++)
            {
                if (metatile[i + 4] != 0)
                    RegisterCompositePaletteColors(tiles, metatile[i], metatile[i + 4]);
            }
        }
    }

    LoadCompositePalettes();

    for (i = 0; i < sCompositeCount; i++)
    {
        ComposeRoute101Tile(tiles, sCompositeLowerTiles[i], sCompositeUpperTiles[i], tileGfx);
        CpuCopy32(tileGfx,
                  (void *)(BG_CHAR_ADDR(OW_BG_CHARBASE) + TILE_OFFSET_4BPP(sCompositeDestTiles[i])),
                  TILE_SIZE_4BPP);
    }
}

static void LoadMappedRoute101Tiles(void)
{
    if (Route101_Layout.primaryTileset->isCompressed)
        LZDecompressWram(Route101_Layout.primaryTileset->tiles, gDecompressionBuffer);
    else
        CpuCopy32(Route101_Layout.primaryTileset->tiles, gDecompressionBuffer, NUM_TILES_IN_PRIMARY * TILE_SIZE_4BPP);
    CopyMappedRoute101Tiles(gDecompressionBuffer, 0, NUM_TILES_IN_PRIMARY);
    LoadCompositeRoute101Tiles(gDecompressionBuffer);

    if (Route101_Layout.secondaryTileset->isCompressed)
        LZDecompressWram(Route101_Layout.secondaryTileset->tiles, gDecompressionBuffer);
    else
        CpuCopy32(Route101_Layout.secondaryTileset->tiles, gDecompressionBuffer, (NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY) * TILE_SIZE_4BPP);
    CopyMappedRoute101Tiles(gDecompressionBuffer, NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY);
}

static void BattleOverworldScene_ApplyBg3Config(void)
{
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_CHARBASEINDEX, OW_BG_CHARBASE);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_MAPBASEINDEX, OW_BG_LOWER_SCREENBASE);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_SCREENSIZE, 0);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_PALETTEMODE, 0);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_PRIORITY, 3);
}

void BattleOverworldScene_LoadBackground(void)
{
    u8 palMap[16];

    if (!IsBattleOverworldSceneEnabled())
        return;

    BuildRoute101TileAndPaletteMaps(palMap);
    LoadMappedRoute101Tiles();
    UpdateRoute101PaletteMask(palMap);
    BattleOverworldScene_RestoreBackground();
}

void BattleOverworldScene_RestoreBackground(void)
{
    u8 x;
    u8 y;
    u8 palMap[16];
    u16 metatileId;
    u16 *lowerBg = (u16 *)BG_SCREEN_ADDR(OW_BG_LOWER_SCREENBASE);

    if (!IsBattleOverworldSceneEnabled())
        return;

    BuildRoute101TileAndPaletteMaps(palMap);
    UpdateRoute101PaletteMask(palMap);

    CpuFill16(0, lowerBg, BG_SCREEN_SIZE);

    for (y = 0; y < OW_BG_ROUTE101_HEIGHT; y++)
    {
        for (x = 0; x < OW_BG_ROUTE101_WIDTH; x++)
        {
            metatileId = Route101_Layout.map[(OW_BG_ROUTE101_Y + y) * Route101_Layout.width + OW_BG_ROUTE101_X + x] & MAPGRID_METATILE_ID_MASK;
            if (metatileId >= NUM_METATILES_TOTAL)
                metatileId = 0;
            DrawRoute101Metatile(lowerBg, x, y, GetRoute101Metatile(metatileId), palMap);
        }
    }

    gBattle_BG3_X = 0;
    gBattle_BG3_Y = 0;
    BattleOverworldScene_ApplyBg3Config();
    if (sSceneVisible)
        ShowBg(OW_BG_LOWER_ID);
    else
        HideBg(OW_BG_LOWER_ID);
    SetGpuReg(REG_OFFSET_BG3CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_LOWER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);
}

void BattleOverworldScene_KeepBaseBackgroundVisible(void)
{
    u16 bldCnt;
    u16 winIn;
    u16 winOut;

    if (!IsBattleOverworldSceneEnabled())
        return;

    gBattle_BG3_X = 0;
    gBattle_BG3_Y = 0;
    BattleOverworldScene_ApplyBg3Config();
    if (sSceneVisible)
        ShowBg(OW_BG_LOWER_ID);
    else
        HideBg(OW_BG_LOWER_ID);
    SetGpuReg(REG_OFFSET_BG3CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_LOWER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);

    winIn = GetGpuReg(REG_OFFSET_WININ);
    winOut = GetGpuReg(REG_OFFSET_WINOUT);
    SetGpuReg(REG_OFFSET_WININ, winIn | WININ_WIN0_BG3 | WININ_WIN1_BG3);
    SetGpuReg(REG_OFFSET_WINOUT, winOut | WINOUT_WIN01_BG3 | WINOUT_WINOBJ_BG3);

    if (sReshowTransitionAllowsBg3Blend && sBg3BlendFadeStarted && !gPaletteFade.active)
    {
        sReshowTransitionAllowsBg3Blend = FALSE;
        sBg3BlendFadeStarted = FALSE;
    }

    if (!sReshowTransitionAllowsBg3Blend)
    {
        bldCnt = GetGpuReg(REG_OFFSET_BLDCNT);
        if (bldCnt & (BLDCNT_TGT1_BG3 | BLDCNT_TGT2_BG3))
            SetGpuReg(REG_OFFSET_BLDCNT, bldCnt & ~(BLDCNT_TGT1_BG3 | BLDCNT_TGT2_BG3));
    }
}

void BattleOverworldScene_BeginReshowBlackout(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return;

    sReshowTransitionAllowsBg3Blend = TRUE;
    sBg3BlendFadeStarted = FALSE;
    sSceneVisible = FALSE;
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_ALL | BLDCNT_EFFECT_DARKEN);
    SetGpuReg(REG_OFFSET_BLDY, 16);
}

void BattleOverworldScene_BeginSceneFadeIn(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return;

    sReshowTransitionAllowsBg3Blend = TRUE;
    sBg3BlendFadeStarted = TRUE;
    sSceneVisible = TRUE;
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    ShowBg(3);
    BeginHardwarePaletteFade(0xFF, 0, 0x10, 0, 1);
}

void BattleOverworldScene_RestoreBattlerSpriteAnim(u8 battler)
{
    struct Sprite *sprite;

    if (!IsBattleOverworldSceneEnabled() || battler >= gBattlersCount)
        return;
    if (sOwBattlerSpriteIds[battler] >= MAX_SPRITES || gBattlerSpriteIds[battler] != sOwBattlerSpriteIds[battler])
        return;

    sprite = &gSprites[sOwBattlerSpriteIds[battler]];
    if (!sprite->inUse)
        return;

    sprite->oam.affineMode = ST_OAM_AFFINE_OFF;
    sprite->oam.objMode = ST_OAM_OBJ_NORMAL;
    sprite->hFlip = FALSE;
    sprite->vFlip = FALSE;
    sprite->oam.matrixNum &= ~ST_OAM_MNUM_FLIP_MASK;
    sprite->animPaused = FALSE;
    sprite->affineAnimPaused = FALSE;
    CalcCenterToCornerVec(sprite, sprite->oam.shape, sprite->oam.size, sprite->oam.affineMode);
    StartSpriteAnim(sprite, gBattleMonForms[battler]);
    AnimateSprite(sprite);
}

bool8 BattleOverworldScene_IsBattlerFacingRight(u8 battler)
{
    if (!IsBattleOverworldSceneEnabled() || battler >= gBattlersCount)
        return FALSE;

    return IsPlayerBattlerPosition(GetBattlerPosition(battler));
}

void BattleOverworldScene_RegisterBattlerSprite(u8 battler, u8 spriteId)
{
    if (!IsBattleOverworldSceneEnabled() || battler >= MAX_BATTLERS_COUNT)
        return;

    sOwBattlerSpriteIds[battler] = spriteId;
}

bool8 BattleOverworldScene_IsBattlerSprite(u8 battler, u8 spriteId)
{
    if (!IsBattleOverworldSceneEnabled() || battler >= MAX_BATTLERS_COUNT)
        return FALSE;

    return sOwBattlerSpriteIds[battler] == spriteId && spriteId < MAX_SPRITES;
}

void BattleOverworldScene_SetBattlerHiddenByBall(u8 battler, bool8 hidden)
{
    if (!IsBattleOverworldSceneEnabled() || battler >= MAX_BATTLERS_COUNT)
        return;

    sOwBattlerHiddenByBall[battler] = hidden;
}

static void Task_BattleOverworldScene_KeepSpritesVisible(u8 taskId)
{
    u8 battler;

    if (!IsBattleOverworldSceneEnabled())
    {
        DestroyTask(taskId);
        return;
    }

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (BattleOverworldScene_IsBattlerSprite(battler, gBattlerSpriteIds[battler])
         && gSprites[gBattlerSpriteIds[battler]].inUse)
        {
            if (!sOwBattlerHiddenByBall[battler])
                gSprites[gBattlerSpriteIds[battler]].invisible = FALSE;
            gBattleSpritesDataPtr->battlerData[battler].invisible = FALSE;
        }

        if (gHealthboxSpriteIds[battler] < MAX_SPRITES
         && (!BattleOverworldScene_IsBattlerSprite(battler, gBattlerSpriteIds[battler]) || sOwBattlerHiddenByBall[battler]))
            SetHealthboxSpriteInvisible(gHealthboxSpriteIds[battler]);
    }

    if (sPlayerTrainerSpriteId < MAX_SPRITES && gSprites[sPlayerTrainerSpriteId].inUse)
        gSprites[sPlayerTrainerSpriteId].invisible = FALSE;

    if (sOpponentTrainerSpriteId < MAX_SPRITES && gSprites[sOpponentTrainerSpriteId].inUse)
        gSprites[sOpponentTrainerSpriteId].invisible = FALSE;
}

static void BattleOverworldScene_EnsureVisibilityTask(void)
{
    if (FindTaskIdByFunc(Task_BattleOverworldScene_KeepSpritesVisible) == TASK_NONE)
        CreateTask(Task_BattleOverworldScene_KeepSpritesVisible, 0);
}

static bool8 IsBattleOverworldSceneEnabled(void)
{
    return !sSceneSuspended && !(gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_RECORDED | BATTLE_TYPE_SAFARI));
}

bool8 BattleOverworldScene_IsEnabled(void)
{
    return IsBattleOverworldSceneEnabled();
}

void BattleOverworldScene_SetSuspended(bool8 suspended)
{
    sSceneSuspended = suspended;
}

u32 BattleOverworldScene_GetBgPaletteMask(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return 0;

    return sBattleOwBgPaletteMask;
}

u32 BattleOverworldScene_ApplyBgPaletteMask(u32 selectedPalettes)
{
    const u32 vanillaBattleBgMask = 0xE; // BG palettes 1, 2, and 3

    if (!IsBattleOverworldSceneEnabled())
        return selectedPalettes;

    if (selectedPalettes & vanillaBattleBgMask)
        selectedPalettes = (selectedPalettes & ~vanillaBattleBgMask) | sBattleOwBgPaletteMask;

    return selectedPalettes;
}

static const struct BattleOwMonGfx *GetBattleOwMonGfx(u16 species)
{
    if (species >= NUM_SPECIES || sBattleOwMonGfx[species].gfx == NULL)
        return &sBattleOwMonGfx[SPECIES_PIKACHU];

    return &sBattleOwMonGfx[species];
}

void BattleOverworldScene_Reset(void)
{
    BattleOverworldScene_ResetSpriteReferences();
    sSceneSuspended = FALSE;
    sReshowTransitionAllowsBg3Blend = FALSE;
    sBg3BlendFadeStarted = FALSE;
    sSceneVisible = FALSE;
    sCompositeCount = 0;
    sCompositePalCount = 0;
}

void BattleOverworldScene_ResetSpriteReferences(void)
{
    u8 battler;

    sPlayerTrainerSpriteId = SPRITE_NONE;
    sOpponentTrainerSpriteId = SPRITE_NONE;
    for (battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
    {
        sOwBattlerSpriteIds[battler] = SPRITE_NONE;
        sOwBattlerHiddenByBall[battler] = FALSE;
    }
    sCreatedTrainerSprites = FALSE;
}

static const struct ObjectEventGraphicsInfo *GetBattleOwTrainerGraphicsInfo(u8 trainerClass)
{
    switch (trainerClass)
    {
    case TRAINER_CLASS_HIKER:
        return &gObjectEventGraphicsInfo_Hiker;
    case TRAINER_CLASS_TEAM_AQUA:
    case TRAINER_CLASS_AQUA_ADMIN:
    case TRAINER_CLASS_AQUA_LEADER:
        return &gObjectEventGraphicsInfo_AquaMemberM;
    case TRAINER_CLASS_TEAM_MAGMA:
    case TRAINER_CLASS_MAGMA_ADMIN:
    case TRAINER_CLASS_MAGMA_LEADER:
        return &gObjectEventGraphicsInfo_MagmaMemberM;
    case TRAINER_CLASS_PKMN_BREEDER:
    case TRAINER_CLASS_AROMA_LADY:
    case TRAINER_CLASS_LADY:
    case TRAINER_CLASS_BEAUTY:
    case TRAINER_CLASS_PARASOL_LADY:
        return &gObjectEventGraphicsInfo_Beauty;
    case TRAINER_CLASS_COOLTRAINER:
    case TRAINER_CLASS_COOLTRAINER_2:
    case TRAINER_CLASS_DRAGON_TAMER:
        return &gObjectEventGraphicsInfo_Boy3;
    case TRAINER_CLASS_BIRD_KEEPER:
    case TRAINER_CLASS_KINDLER:
        return &gObjectEventGraphicsInfo_Man3;
    case TRAINER_CLASS_COLLECTOR:
    case TRAINER_CLASS_POKEMANIAC:
    case TRAINER_CLASS_RUIN_MANIAC:
    case TRAINER_CLASS_BUG_MANIAC:
        return &gObjectEventGraphicsInfo_Maniac;
    case TRAINER_CLASS_SWIMMER_M:
        return &gObjectEventGraphicsInfo_SwimmerM;
    case TRAINER_CLASS_SWIMMER_F:
        return &gObjectEventGraphicsInfo_SwimmerF;
    case TRAINER_CLASS_EXPERT:
    case TRAINER_CLASS_OLD_COUPLE:
        return &gObjectEventGraphicsInfo_ExpertM;
    case TRAINER_CLASS_BLACK_BELT:
    case TRAINER_CLASS_BATTLE_GIRL:
        return &gObjectEventGraphicsInfo_BlackBelt;
    case TRAINER_CLASS_HEX_MANIAC:
        return &gObjectEventGraphicsInfo_HexManiac;
    case TRAINER_CLASS_INTERVIEWER:
        return &gObjectEventGraphicsInfo_ReporterM;
    case TRAINER_CLASS_TUBER_F:
        return &gObjectEventGraphicsInfo_TuberF;
    case TRAINER_CLASS_TUBER_M:
        return &gObjectEventGraphicsInfo_TuberM;
    case TRAINER_CLASS_RICH_BOY:
        return &gObjectEventGraphicsInfo_RichBoy;
    case TRAINER_CLASS_GUITARIST:
        return &gObjectEventGraphicsInfo_Man1;
    case TRAINER_CLASS_CAMPER:
    case TRAINER_CLASS_PKMN_RANGER:
        return &gObjectEventGraphicsInfo_Camper;
    case TRAINER_CLASS_PICNICKER:
        return &gObjectEventGraphicsInfo_Picnicker;
    case TRAINER_CLASS_PSYCHIC:
        return &gObjectEventGraphicsInfo_PsychicM;
    case TRAINER_CLASS_GENTLEMAN:
        return &gObjectEventGraphicsInfo_Gentleman;
    case TRAINER_CLASS_ELITE_FOUR:
        return &gObjectEventGraphicsInfo_Sidney;
    case TRAINER_CLASS_LEADER:
        return &gObjectEventGraphicsInfo_Roxanne;
    case TRAINER_CLASS_SCHOOL_KID:
        return &gObjectEventGraphicsInfo_SchoolKidM;
    case TRAINER_CLASS_SR_AND_JR:
    case TRAINER_CLASS_TWINS:
    case TRAINER_CLASS_YOUNG_COUPLE:
    case TRAINER_CLASS_SIS_AND_BRO:
        return &gObjectEventGraphicsInfo_Twin;
    case TRAINER_CLASS_WINSTRATE:
        return &gObjectEventGraphicsInfo_Woman1;
    case TRAINER_CLASS_POKEFAN:
        return &gObjectEventGraphicsInfo_PokefanM;
    case TRAINER_CLASS_CHAMPION:
        return &gObjectEventGraphicsInfo_Steven;
    case TRAINER_CLASS_FISHERMAN:
        return &gObjectEventGraphicsInfo_Fisherman;
    case TRAINER_CLASS_TRIATHLETE:
        return &gObjectEventGraphicsInfo_RunningTriathleteM;
    case TRAINER_CLASS_NINJA_BOY:
        return &gObjectEventGraphicsInfo_NinjaBoy;
    case TRAINER_CLASS_SAILOR:
        return &gObjectEventGraphicsInfo_Sailor;
    case TRAINER_CLASS_RIVAL:
        return &gObjectEventGraphicsInfo_RivalBrendanNormal;
    case TRAINER_CLASS_BUG_CATCHER:
        return &gObjectEventGraphicsInfo_BugCatcher;
    case TRAINER_CLASS_LASS:
        return &gObjectEventGraphicsInfo_Lass;
    case TRAINER_CLASS_SALON_MAIDEN:
        return &gObjectEventGraphicsInfo_Woman5;
    case TRAINER_CLASS_DOME_ACE:
        return &gObjectEventGraphicsInfo_Tucker;
    case TRAINER_CLASS_PALACE_MAVEN:
        return &gObjectEventGraphicsInfo_Spenser;
    case TRAINER_CLASS_ARENA_TYCOON:
        return &gObjectEventGraphicsInfo_Greta;
    case TRAINER_CLASS_FACTORY_HEAD:
        return &gObjectEventGraphicsInfo_Noland;
    case TRAINER_CLASS_PIKE_QUEEN:
        return &gObjectEventGraphicsInfo_Lucy;
    case TRAINER_CLASS_PYRAMID_KING:
        return &gObjectEventGraphicsInfo_Brandon;
    case TRAINER_CLASS_RS_PROTAG:
        return &gObjectEventGraphicsInfo_RubySapphireBrendan;
    case TRAINER_CLASS_YOUNGSTER:
    default:
        return &gObjectEventGraphicsInfo_Youngster;
    }
}

void BattleOverworldScene_CreateTrainerSprites(void)
{
    struct SpriteTemplate template;
    const struct ObjectEventGraphicsInfo *opponentGraphicsInfo;

    if (!IsBattleOverworldSceneEnabled())
        return;

    if (sCreatedTrainerSprites)
    {
        if (sPlayerTrainerSpriteId < MAX_SPRITES && gSprites[sPlayerTrainerSpriteId].inUse)
            return;
        sCreatedTrainerSprites = FALSE;
        sPlayerTrainerSpriteId = SPRITE_NONE;
        sOpponentTrainerSpriteId = SPRITE_NONE;
    }

    template = sTrainerTemplate;
    template.images = (gSaveBlock2Ptr->playerGender == FEMALE) ? sPicTable_BattleMay : sPicTable_BattleBrendan;
    template.anims = sAnimTable_BattleTrainerFaceEast;
    LoadPalette(gSaveBlock2Ptr->playerGender == FEMALE ? gObjectEventPal_May : gObjectEventPal_Brendan,
                OBJ_PLTT_ID(OW_TRAINER_PLAYER_PAL_SLOT),
                PLTT_SIZE_4BPP);
    sPlayerTrainerSpriteId = CreateSprite(&template, 36, OW_SCENE_BASE_Y, 1);
    if (sPlayerTrainerSpriteId != MAX_SPRITES)
    {
        gSprites[sPlayerTrainerSpriteId].oam.paletteNum = OW_TRAINER_PLAYER_PAL_SLOT;
        gSprites[sPlayerTrainerSpriteId].oam.priority = 0;
    }

    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        opponentGraphicsInfo = GetBattleOwTrainerGraphicsInfo(gTrainers[gTrainerBattleOpponent_A].trainerClass);
        template = sTrainerTemplate;
        template.oam = opponentGraphicsInfo->oam;
        template.images = opponentGraphicsInfo->images;
        template.anims = sAnimTable_BattleTrainerFaceWest;
        PatchObjectPalette(opponentGraphicsInfo->paletteTag, OW_TRAINER_OPPONENT_PAL_SLOT);
        sOpponentTrainerSpriteId = CreateSprite(&template, 212, OW_SCENE_BASE_Y, 0);
        if (sOpponentTrainerSpriteId != MAX_SPRITES)
        {
            gSprites[sOpponentTrainerSpriteId].oam.paletteNum = OW_TRAINER_OPPONENT_PAL_SLOT;
            gSprites[sOpponentTrainerSpriteId].oam.priority = 0;
        }
    }

    sCreatedTrainerSprites = TRUE;
    BattleOverworldScene_EnsureVisibilityTask();
}

static s16 GetBattlerOwX(u8 battler)
{
    switch (GetBattlerPosition(battler))
    {
    case B_POSITION_PLAYER_LEFT:
        return 76;
    case B_POSITION_OPPONENT_LEFT:
        return 164;
    case B_POSITION_PLAYER_RIGHT:
        return 84;
    case B_POSITION_OPPONENT_RIGHT:
    default:
        return 172;
    }
}

static s16 GetBattlerOwY(u8 battler)
{
    switch (GetBattlerPosition(battler))
    {
    case B_POSITION_PLAYER_LEFT:
    case B_POSITION_OPPONENT_LEFT:
    case B_POSITION_PLAYER_RIGHT:
    case B_POSITION_OPPONENT_RIGHT:
    default:
        return OW_SCENE_BASE_Y;
    }
}

s16 BattleOverworldScene_GetBattlerSpriteX(u8 battler)
{
    return GetBattlerOwX(battler);
}

s16 BattleOverworldScene_GetBattlerSpriteY(u8 battler)
{
    return GetBattlerOwY(battler);
}

static struct Pokemon *GetBattlerMon(u8 battler)
{
    if (GetBattlerSide(battler) == B_SIDE_PLAYER)
        return &gPlayerParty[gBattlerPartyIndexes[battler]];

    return &gEnemyParty[gBattlerPartyIndexes[battler]];
}

bool8 BattleOverworldScene_CreateBattlerSprite(u8 battler)
{
    struct Pokemon *mon;
    u16 species;

    if (!IsBattleOverworldSceneEnabled())
        return FALSE;
    if (battler >= gBattlersCount)
        return TRUE;
    if (gBattleSpritesDataPtr->battlerData[battler].behindSubstitute)
        return FALSE;

    sOwBattlerSpriteIds[battler] = SPRITE_NONE;

    mon = GetBattlerMon(battler);
    if (GetMonData(mon, MON_DATA_HP) == 0)
        return TRUE;

    species = GetMonData(mon, MON_DATA_SPECIES);
    if (!BattleOverworldScene_LoadMonSpriteGfx(mon, battler))
        return TRUE;
    if (!BattleOverworldScene_SetMonSpriteTemplate(species, GetBattlerPosition(battler)))
        return TRUE;

    gBattlerSpriteIds[battler] = CreateSprite(&gMultiuseSpriteTemplate,
                                              GetBattlerOwX(battler),
                                              GetBattlerOwY(battler),
                                              GetBattlerSpriteSubpriority(battler));
    if (gBattlerSpriteIds[battler] != MAX_SPRITES)
    {
        BattleOverworldScene_RegisterBattlerSprite(battler, gBattlerSpriteIds[battler]);
        gSprites[gBattlerSpriteIds[battler]].data[0] = battler;
        gSprites[gBattlerSpriteIds[battler]].data[2] = species;
        gSprites[gBattlerSpriteIds[battler]].oam.paletteNum = battler;
        gSprites[gBattlerSpriteIds[battler]].oam.priority = 0;
        gSprites[gBattlerSpriteIds[battler]].callback = SpriteCallbackDummy;
        gSprites[gBattlerSpriteIds[battler]].invisible = sOwBattlerHiddenByBall[battler];
        gBattleSpritesDataPtr->battlerData[battler].invisible = FALSE;
        StartSpriteAnim(&gSprites[gBattlerSpriteIds[battler]], gBattleMonForms[battler]);
        BattleOverworldScene_RestoreBattlerSpriteAnim(battler);
        BattleOverworldScene_EnsureVisibilityTask();
    }

    return TRUE;
}

void BattleOverworldScene_CreateIntroSprites(void)
{
    u8 battler;

    if (!IsBattleOverworldSceneEnabled())
        return;

    BattleOverworldScene_CreateTrainerSprites();

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (gHealthboxSpriteIds[battler] < MAX_SPRITES)
            SetHealthboxSpriteInvisible(gHealthboxSpriteIds[battler]);
    }

    if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
    {
        for (battler = 0; battler < gBattlersCount; battler++)
        {
            if (GetBattlerSide(battler) == B_SIDE_OPPONENT)
            {
                BattleOverworldScene_CreateBattlerSprite(battler);
                if (gHealthboxSpriteIds[battler] < MAX_SPRITES)
                {
                    gSprites[gHealthboxSpriteIds[battler]].x2 = 0;
                    gSprites[gHealthboxSpriteIds[battler]].y2 = 0;
                    gSprites[gHealthboxSpriteIds[battler]].callback = SpriteCallbackDummy;
                    SetHealthboxSpriteVisible(gHealthboxSpriteIds[battler]);
                }
            }
        }
    }

    BattleOverworldScene_EnsureVisibilityTask();
}

void BattleOverworldScene_CreateInitialSprites(void)
{
    u8 battler;

    if (!IsBattleOverworldSceneEnabled())
        return;

    BattleOverworldScene_CreateTrainerSprites();

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        BattleOverworldScene_CreateBattlerSprite(battler);

        if (gHealthboxSpriteIds[battler] < MAX_SPRITES)
        {
            gSprites[gHealthboxSpriteIds[battler]].x2 = 0;
            gSprites[gHealthboxSpriteIds[battler]].y2 = 0;
            gSprites[gHealthboxSpriteIds[battler]].callback = SpriteCallbackDummy;
            SetHealthboxSpriteVisible(gHealthboxSpriteIds[battler]);
        }
    }

    BattleOverworldScene_EnsureVisibilityTask();
}

void BattleOverworldScene_TryWildShinyAnimations(void)
{
    u8 battler;
    bool8 started = FALSE;

    if (!IsBattleOverworldSceneEnabled())
        return;
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        return;
    if (FindTaskIdByFunc(Task_BattleOverworldScene_WildShinyAnimations) != TASK_NONE)
        return;

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (GetBattlerSide(battler) != B_SIDE_OPPONENT)
            continue;
        if (!BattleOverworldScene_IsBattlerSprite(battler, gBattlerSpriteIds[battler]))
            continue;

        TryShinyAnimation(battler, GetBattlerMon(battler));
        started = TRUE;
    }

    if (started)
        CreateTask(Task_BattleOverworldScene_WildShinyAnimations, 10);
}

static void Task_BattleOverworldScene_WildShinyAnimations(u8 taskId)
{
    u8 battler;
    bool8 waiting = FALSE;

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (GetBattlerSide(battler) != B_SIDE_OPPONENT)
            continue;
        if (!gBattleSpritesDataPtr->healthBoxesData[battler].triedShinyMonAnim)
            continue;
        if (!gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim)
            waiting = TRUE;
    }

    if (waiting)
        return;

    for (battler = 0; battler < gBattlersCount; battler++)
    {
        if (GetBattlerSide(battler) != B_SIDE_OPPONENT)
            continue;

        gBattleSpritesDataPtr->healthBoxesData[battler].triedShinyMonAnim = FALSE;
        gBattleSpritesDataPtr->healthBoxesData[battler].finishedShinyMonAnim = FALSE;
    }

    FreeSpriteTilesByTag(ANIM_TAG_GOLD_STARS);
    FreeSpritePaletteByTag(ANIM_TAG_GOLD_STARS);
    DestroyTask(taskId);
}

bool8 BattleOverworldScene_LoadMonSpriteGfx(struct Pokemon *mon, u8 battler)
{
    u32 personality;
    u16 species;
    const struct BattleOwMonGfx *info;
    const u16 *palette;

    if (!IsBattleOverworldSceneEnabled())
        return FALSE;

    if (gBattleSpritesDataPtr->battlerData[battler].transformSpecies == SPECIES_NONE)
        species = GetMonData(mon, MON_DATA_SPECIES);
    else
        species = gBattleSpritesDataPtr->battlerData[battler].transformSpecies;

    info = GetBattleOwMonGfx(species);
    if (info == NULL)
        return FALSE;

    personality = GetMonData(mon, MON_DATA_PERSONALITY);
    palette = IsShinyOtIdPersonality(GetMonData(mon, MON_DATA_OT_ID), personality) ? info->shinyPal : info->pal;

    LoadPalette(palette, OBJ_PLTT_ID(battler), PLTT_SIZE_4BPP);
    LoadPalette(palette, BG_PLTT_ID(8) + BG_PLTT_ID(battler), PLTT_SIZE_4BPP);

    return TRUE;
}

bool8 BattleOverworldScene_SetMonSpriteTemplate(u16 species, u8 battlerPosition)
{
    const struct BattleOwMonGfx *info;

    if (!IsBattleOverworldSceneEnabled())
        return FALSE;

    info = GetBattleOwMonGfx(species);
    if (info == NULL)
        return FALSE;

    gMultiuseSpriteTemplate = gMonSpritesGfxPtr->templates[battlerPosition];
    gMultiuseSpriteTemplate.oam = info->oam;
    gMultiuseSpriteTemplate.anims = IsPlayerBattlerPosition(battlerPosition)
                                  ? sAnimTable_BattleOwMonFaceEast
                                  : sAnimTable_BattleOwMonFaceWest;
    gMultiuseSpriteTemplate.images = info->images;
    gMultiuseSpriteTemplate.affineAnims = IsPlayerBattlerPosition(battlerPosition)
                                        ? gAffineAnims_BattleSpritePlayerSide
                                        : gAffineAnims_BattleSpriteOpponentSide;
    gMultiuseSpriteTemplate.paletteTag = species;

    return TRUE;
}
