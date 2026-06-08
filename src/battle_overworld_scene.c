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
#include "frontier_util.h"
#include "gpu_regs.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sprite.h"
#include "task.h"
#include "tileset_anims.h"
#include "constants/battle.h"
#include "constants/battle_anim.h"
#include "constants/event_objects.h"
#include "constants/global.h"
#include "constants/layouts.h"
#include "constants/rgb.h"
#include "constants/species.h"
#include "constants/trainers.h"

#define OW_TRAINER_PLAYER_PAL_SLOT 12
#define OW_TRAINER_OPPONENT_PAL_SLOT 13
#define TAG_OW_TRAINER_PLAYER_PAL 0xD719
#define TAG_OW_TRAINER_OPPONENT_PAL 0xD71A
#define OW_SCENE_BASE_Y 56
#define OW_BG_DEFAULT_MAP_X 3
#define OW_BG_DEFAULT_MAP_Y 7
#define OW_BG_MAP_WIDTH 15
#define OW_BG_MAP_HEIGHT 10
#define OW_BG_PAL_UNMAPPED 0xFF
#define OW_BG_TILE_UNMAPPED 0xFFFF
#define OW_BG_CHARBASE 2
#define OW_BG_LOWER_SCREENBASE 26
#define OW_BG_UPPER_SCREENBASE 27
#define OW_BG_LOWER_ID 3
#define OW_BG_UPPER_ID 2
#define OW_BG_BLANK_TILE 0x040

struct BattleOwBgLayoutOffset
{
    u16 layoutId;
    u16 x;
    u16 y;
};

extern const struct MapLayout Route101_Layout;

extern const struct OamData gObjectEventBaseOam_16x32;

extern const u32 gObjectEventPic_BrendanNormalRunning[];
extern const u32 gObjectEventPic_MayNormalRunning[];
extern const u16 gObjectEventPal_Brendan[];
extern const u16 gObjectEventPal_May[];
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_AquaMemberM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_AquaMemberF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Anabel;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Archie;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Beauty;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_BlackBelt;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Boy3;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Brandon;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Brawly;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_BugCatcher;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Camper;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_CyclingTriathleteF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_CyclingTriathleteM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Drake;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_ExpertF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_ExpertM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Fisherman;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Flannery;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Gentleman;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Girl3;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Glacia;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Greta;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_HexManiac;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Hiker;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Juan;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Lass;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Leaf;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Liza;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Lucy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_MagmaMemberF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_MagmaMemberM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man1;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man3;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man4;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Man5;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Maniac;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Maxie;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_NinjaBoy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Noland;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Norman;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Phoebe;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Picnicker;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_PokefanF;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_PokefanM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_PsychicM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Red;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_ReporterM;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RichBoy;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RivalBrendanNormal;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RivalMayNormal;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Roxanne;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RubySapphireBrendan;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RubySapphireMay;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_RunningTriathleteF;
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
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Wallace;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Wally;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Wattson;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Winona;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Woman1;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Woman2;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Woman5;
extern const struct ObjectEventGraphicsInfo gObjectEventGraphicsInfo_Youngster;

static const struct SpriteFrameImage sPicTable_BattleBrendan[] =
{
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 0),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 1),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 2),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 3),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 4),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 5),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 6),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 7),
    overworld_frame(gObjectEventPic_BrendanNormalRunning, 2, 4, 8),
};

static const struct SpriteFrameImage sPicTable_BattleMay[] =
{
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 0),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 1),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 2),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 3),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 4),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 5),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 6),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 7),
    overworld_frame(gObjectEventPic_MayNormalRunning, 2, 4, 8),
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

static const union AnimCmd *const sAnimTable_BattleTrainerFaceWest[] =
{
    sAnim_BattleTrainerFaceWest,
};

static const union AnimCmd *const sAnimTable_BattleTrainerFaceEast[] =
{
    sAnim_BattleTrainerFaceEast,
};

static bool8 IsBattleOverworldSceneEnabled(void);
static void BattleOverworldScene_ApplyBgConfig(void);
static void BattleOverworldScene_SetBackgroundLayout(const struct MapLayout *layout, u16 x, u16 y);
static enum TrainerPicID GetBattleOwOpponentTrainerPic(void);
static enum TrainerClassID GetBattleOwOpponentTrainerClass(void);
static const struct ObjectEventGraphicsInfo *GetBattleOwTrainerGraphicsInfo(enum TrainerPicID trainerPic, enum TrainerClassID trainerClass);
static const struct ObjectEventGraphicsInfo *GetBattleOwMonGraphicsInfo(struct Pokemon *mon, u8 battler, u16 *species, u16 *graphicsId);
static void Task_BattleOverworldScene_WildShinyAnimations(u8 taskId);
static const u16 *GetBattleOwPlayerTrainerPalette(void);
static void LoadBattleOwPlayerTrainerPalette(void);
static void ReserveLoadedBattleOwTrainerPalette(u8 paletteSlot, u16 paletteTag);
static void LoadBattleOwOpponentTrainerPalette(u16 objectPaletteTag);
static void RestoreBattleOwTrainerPalettes(void);

static u8 sPlayerTrainerSpriteId;
static u8 sOpponentTrainerSpriteId;
static u16 sOpponentTrainerPaletteTag;
static u8 sOwBattlerSpriteIds[MAX_BATTLERS_COUNT];
static bool8 sOwBattlerHiddenByBall[MAX_BATTLERS_COUNT];
static bool8 sCreatedTrainerSprites;
static bool8 sSceneSuspended;
static bool8 sReshowTransitionAllowsBg3Blend;
static bool8 sBg3BlendFadeStarted;
static bool8 sSceneVisible;
static u8 sBg3BlendRefCount;
static EWRAM_DATA u16 sBattleOwBgTileMap[NUM_TILES_TOTAL] = {0};
static EWRAM_DATA u32 sBattleOwBgPaletteMask = 0;
// BG palettes 8 and 9 are used as battle-animation scratch palettes for battler BG masks.
static const u8 sBattleOwBgFreePalSlots[] = {2, 3, 4, 7, 10, 11, 12, 13, 14, 15};
static EWRAM_DATA u8 ALIGNED(4) sBattleOwDecompressionBuffer[MAX_DECOMPRESSION_BUFFER_SIZE] = {0};
static EWRAM_DATA const struct MapLayout *sBattleOwBgLayout = NULL;
static EWRAM_DATA u16 sBattleOwBgMapX = 0;
static EWRAM_DATA u16 sBattleOwBgMapY = 0;
static EWRAM_DATA bool8 sBattleOwBgTilesetAnimsActive = FALSE;
static EWRAM_DATA u8 sPreparedHealthboxPartyIds[MAX_BATTLERS_COUNT] = {0};

static const u16 sBattleOwBgTileRanges[][2] =
{
    {0x040, 0x200},
};

static const struct BattleOwBgLayoutOffset sBattleOwBgLayoutOffsets[] =
{
    {LAYOUT_PETALBURG_CITY, 1, 7},
    {LAYOUT_SLATEPORT_CITY, 20, 10},
    {LAYOUT_MAUVILLE_CITY, 1, 5},

    {LAYOUT_ROUTE101, OW_BG_DEFAULT_MAP_X, OW_BG_DEFAULT_MAP_Y},
    {LAYOUT_ROUTE102, 7, 1},
    {LAYOUT_ROUTE103, 3, 7},
    {LAYOUT_ROUTE104, 15, 21},
    {LAYOUT_ROUTE106, 51, 10},
    {LAYOUT_ROUTE109, 15, 16},
    {LAYOUT_ROUTE110, 14, 36},
    {LAYOUT_ROUTE111, 0, 66},
    {LAYOUT_ROUTE112, 11, 35},
    {LAYOUT_ROUTE113, 30, 3},
    {LAYOUT_ROUTE114, 11, 31},
    {LAYOUT_ROUTE115, 6, 11},
    {LAYOUT_ROUTE116, 35, 7},
    {LAYOUT_ROUTE117, 39, 3},
    {LAYOUT_ROUTE118, 2, 4},
    {LAYOUT_ROUTE119, 11, 30},
    {LAYOUT_ROUTE120, 12, 12},
    {LAYOUT_ROUTE121, 53, 2},
    {LAYOUT_ROUTE123, 11, 6},
    {LAYOUT_ROUTE124, 5, 68},
    {LAYOUT_ROUTE125, 9, 17},
    {LAYOUT_ROUTE127, 6, 3},
    {LAYOUT_ROUTE128, 93, 27},
    {LAYOUT_ROUTE129, 22, 23},

    {LAYOUT_ROUTE130_MIRAGE_ISLAND, 36, 1},

    {LAYOUT_ROUTE132, 43, 19},
    {LAYOUT_ROUTE133, 40, 19},
    {LAYOUT_ROUTE134, 42, 12},

    {LAYOUT_DEWFORD_TOWN_GYM, 2, 18},

    {LAYOUT_METEOR_FALLS_1F_1R, 14, 14},
    {LAYOUT_METEOR_FALLS_1F_2R, 11, 16},
    {LAYOUT_METEOR_FALLS_B1F_1R, 3, 16},

    {LAYOUT_RUSTURF_TUNNEL, 6, 1},

    {LAYOUT_GRANITE_CAVE_1F, 16, 3},
    {LAYOUT_GRANITE_CAVE_B1F, 4, 9},
    {LAYOUT_GRANITE_CAVE_B2F, 8, 11},

    {LAYOUT_PETALBURG_WOODS, 8, 14},

    {LAYOUT_MT_CHIMNEY, 9, 13},

    {LAYOUT_MT_PYRE_1F, 2, 2},

    {LAYOUT_AQUA_HIDEOUT_1F, 7, 6},
    {LAYOUT_AQUA_HIDEOUT_B1F, 0, 2},
    {LAYOUT_AQUA_HIDEOUT_B2F, 18, 2},

    {LAYOUT_SEAFLOOR_CAVERN_ENTRANCE, 3, 4},
    {LAYOUT_SEAFLOOR_CAVERN_ROOM1, 3, 7},

    {LAYOUT_SEAFLOOR_CAVERN_ROOM3, 0, 7},
    {LAYOUT_SEAFLOOR_CAVERN_ROOM4, 2, 2},

    {LAYOUT_SEAFLOOR_CAVERN_ROOM9, 6, 24},
    {LAYOUT_CAVE_OF_ORIGIN_1F, 4, 11},
    {LAYOUT_CAVE_OF_ORIGIN_B1F, 3, 0},

    {LAYOUT_VICTORY_ROAD_1F, 1, 22},

    {LAYOUT_SHOAL_CAVE_LOW_TIDE_ENTRANCE_ROOM, 10, 21},
    {LAYOUT_SHOAL_CAVE_LOW_TIDE_INNER_ROOM, 5, 18},

    {LAYOUT_SHOAL_CAVE_LOW_TIDE_LOWER_ROOM, 6, 0},

    {LAYOUT_SHOAL_CAVE_HIGH_TIDE_ENTRANCE_ROOM, 10, 21},
    {LAYOUT_SHOAL_CAVE_HIGH_TIDE_INNER_ROOM, 5, 18},

    {LAYOUT_NEW_MAUVILLE_INSIDE, 11, 14},

    {LAYOUT_ABANDONED_SHIP_DECK, 1, 7},

    {LAYOUT_ABANDONED_SHIP_ROOMS2_B1F, 1, 0},

    {LAYOUT_SAFARI_ZONE_NORTHWEST, 13, 13},
    {LAYOUT_SAFARI_ZONE_NORTH, 7, 4},
    {LAYOUT_SAFARI_ZONE_SOUTHWEST, 6, 26},
    {LAYOUT_SAFARI_ZONE_SOUTH, 17, 29},

    {LAYOUT_FIERY_PATH, 17, 26},

    {LAYOUT_MT_PYRE_EXTERIOR, 14, 33},
    {LAYOUT_MT_PYRE_SUMMIT, 16, 7},

    {LAYOUT_MAGMA_HIDEOUT_1F, 8, 6},
    {LAYOUT_MAGMA_HIDEOUT_2F_1R, 8, 16},
    {LAYOUT_MAGMA_HIDEOUT_2F_2R, 12, 6},
    {LAYOUT_MAGMA_HIDEOUT_2F_3R, 42, 8},
    {LAYOUT_MAGMA_HIDEOUT_3F_1R, 5, 6},
    {LAYOUT_MAGMA_HIDEOUT_3F_2R, 6, 1},
    {LAYOUT_MAGMA_HIDEOUT_3F_3R, 7, 12},
    {LAYOUT_MAGMA_HIDEOUT_4F, 7, 17},

    {LAYOUT_DESERT_UNDERPASS, 8, 6},

    {LAYOUT_ROUTE111_NO_MIRAGE_TOWER, 0, 66},

    {LAYOUT_SAFARI_ZONE_NORTHEAST, 17, 7},
    {LAYOUT_SAFARI_ZONE_SOUTHEAST, 9, 11},

    {LAYOUT_ARTISAN_CAVE_B1F, 14, 43},

    {LAYOUT_BIRTH_ISLAND_EXTERIOR, 8, 8},
};

static bool8 IsPlayerBattlerPosition(u8 battlerPosition)
{
    return (battlerPosition == B_POSITION_PLAYER_LEFT || battlerPosition == B_POSITION_PLAYER_RIGHT);
}

static u16 RemapBgTile(u16 tile, const u8 *palMap)
{
    u16 pal;
    u16 tileNum;

    if (tile == 0)
        return OW_BG_BLANK_TILE;

    pal = palMap[tile >> 12];
    tileNum = sBattleOwBgTileMap[tile & 0x3FF];
    if (pal == OW_BG_PAL_UNMAPPED)
        pal = 0;
    if (tileNum == OW_BG_TILE_UNMAPPED)
        tileNum = OW_BG_BLANK_TILE;

    return (tile & 0x0C00) | tileNum | (pal << 12);
}

static void UpdateMapPaletteMask(const u8 *palMap)
{
    u8 i;

    sBattleOwBgPaletteMask = 0;

    for (i = 0; i < 16; i++)
    {
        if (palMap[i] != OW_BG_PAL_UNMAPPED)
            sBattleOwBgPaletteMask |= 1 << palMap[i];
    }
}

static u16 GetNextMapTile(u16 tile)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sBattleOwBgTileRanges); i++)
    {
        if (tile < sBattleOwBgTileRanges[i][0])
            return sBattleOwBgTileRanges[i][0];
        if (tile + 1 < sBattleOwBgTileRanges[i][1])
            return tile + 1;
    }

    return 0;
}

static const u16 *GetMapMetatile(u16 metatileId)
{
    if (metatileId < NUM_METATILES_IN_PRIMARY)
        return sBattleOwBgLayout->primaryTileset->metatiles + metatileId * NUM_TILES_PER_METATILE;

    return sBattleOwBgLayout->secondaryTileset->metatiles + (metatileId - NUM_METATILES_IN_PRIMARY) * NUM_TILES_PER_METATILE;
}

static u16 GetMapMetatileIdAt(u8 x, u8 y)
{
    u16 mapX = sBattleOwBgMapX + x;
    u16 mapY = sBattleOwBgMapY + y;
    u16 metatileId;

    if (mapX >= sBattleOwBgLayout->width || mapY >= sBattleOwBgLayout->height)
        return 0;

    metatileId = sBattleOwBgLayout->map[mapY * sBattleOwBgLayout->width + mapX] & MAPGRID_METATILE_ID_MASK;
    if (metatileId >= NUM_METATILES_TOTAL)
        return 0;

    return metatileId;
}

static void DrawMapMetatile(u16 *lowerBg, u16 *upperBg, u8 x, u8 y, const u16 *metatile, const u8 *palMap)
{
    u16 offset = y * 2 * 32 + x * 2;

    lowerBg[offset] = RemapBgTile(metatile[0], palMap);
    lowerBg[offset + 1] = RemapBgTile(metatile[1], palMap);
    lowerBg[offset + 32] = RemapBgTile(metatile[2], palMap);
    lowerBg[offset + 33] = RemapBgTile(metatile[3], palMap);
    upperBg[offset] = RemapBgTile(metatile[4], palMap);
    upperBg[offset + 1] = RemapBgTile(metatile[5], palMap);
    upperBg[offset + 32] = RemapBgTile(metatile[6], palMap);
    upperBg[offset + 33] = RemapBgTile(metatile[7], palMap);
}

static void LoadMapPalette(u8 srcPal, u8 destPal)
{
    if (srcPal < NUM_PALS_IN_PRIMARY)
        LoadPalette(sBattleOwBgLayout->primaryTileset->palettes[srcPal], BG_PLTT_ID(destPal), PLTT_SIZE_4BPP);
    else
        LoadPalette(sBattleOwBgLayout->secondaryTileset->palettes[srcPal], BG_PLTT_ID(destPal), PLTT_SIZE_4BPP);
}

static void TryAssignMapPalette(const u16 *metatile, u8 *palMap, u8 *nextPalSlot)
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
                LoadMapPalette(srcPal, palMap[srcPal]);
                (*nextPalSlot)++;
            }
        }
    }
}

static void TryAssignMapTile(const u16 *metatile, u16 *nextTile)
{
    u8 i;
    u16 srcTile;

    for (i = 0; i < NUM_TILES_PER_METATILE; i++)
    {
        if (metatile[i] == 0)
            continue;

        srcTile = metatile[i] & 0x3FF;
        if (sBattleOwBgTileMap[srcTile] == OW_BG_TILE_UNMAPPED)
        {
            if (*nextTile == 0)
                sBattleOwBgTileMap[srcTile] = 0;
            else
            {
                sBattleOwBgTileMap[srcTile] = *nextTile;
                *nextTile = GetNextMapTile(*nextTile);
            }
        }
    }
}

static void BuildMapTileAndPaletteMaps(u8 *palMap)
{
    u8 i;
    u8 x;
    u8 y;
    u8 nextPalSlot = 0;
    u16 metatileId;
    u16 nextTile = GetNextMapTile(OW_BG_BLANK_TILE);

    for (i = 0; i < 16; i++)
        palMap[i] = OW_BG_PAL_UNMAPPED;
    for (metatileId = 0; metatileId < NUM_TILES_TOTAL; metatileId++)
        sBattleOwBgTileMap[metatileId] = OW_BG_TILE_UNMAPPED;

    for (y = 0; y < OW_BG_MAP_HEIGHT; y++)
    {
        for (x = 0; x < OW_BG_MAP_WIDTH; x++)
        {
            metatileId = GetMapMetatileIdAt(x, y);
            TryAssignMapPalette(GetMapMetatile(metatileId), palMap, &nextPalSlot);
            TryAssignMapTile(GetMapMetatile(metatileId), &nextTile);
        }
    }
}

static void CopyMappedMapTiles(const u8 *tiles, u16 firstTile, u16 numTiles)
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

static void LoadMappedMapTiles(void)
{
    CpuFill32(0, (void *)(BG_CHAR_ADDR(OW_BG_CHARBASE) + TILE_OFFSET_4BPP(OW_BG_BLANK_TILE)), TILE_SIZE_4BPP);

    if (sBattleOwBgLayout->primaryTileset->isCompressed)
        DecompressDataWithHeaderWram(sBattleOwBgLayout->primaryTileset->tiles, sBattleOwDecompressionBuffer);
    else
        CpuCopy32(sBattleOwBgLayout->primaryTileset->tiles, sBattleOwDecompressionBuffer, NUM_TILES_IN_PRIMARY * TILE_SIZE_4BPP);
    CopyMappedMapTiles(sBattleOwDecompressionBuffer, 0, NUM_TILES_IN_PRIMARY);

    if (sBattleOwBgLayout->secondaryTileset->isCompressed)
        DecompressDataWithHeaderWram(sBattleOwBgLayout->secondaryTileset->tiles, sBattleOwDecompressionBuffer);
    else
        CpuCopy32(sBattleOwBgLayout->secondaryTileset->tiles, sBattleOwDecompressionBuffer, (NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY) * TILE_SIZE_4BPP);
    CopyMappedMapTiles(sBattleOwDecompressionBuffer, NUM_TILES_IN_PRIMARY, NUM_TILES_TOTAL - NUM_TILES_IN_PRIMARY);
}

static void BattleOverworldScene_ApplyBgConfig(void)
{
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_CHARBASEINDEX, OW_BG_CHARBASE);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_MAPBASEINDEX, OW_BG_LOWER_SCREENBASE);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_SCREENSIZE, 0);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_PALETTEMODE, 0);
    SetBgAttribute(OW_BG_LOWER_ID, BG_ATTR_PRIORITY, 3);
    SetBgAttribute(OW_BG_UPPER_ID, BG_ATTR_CHARBASEINDEX, OW_BG_CHARBASE);
    SetBgAttribute(OW_BG_UPPER_ID, BG_ATTR_MAPBASEINDEX, OW_BG_UPPER_SCREENBASE);
    SetBgAttribute(OW_BG_UPPER_ID, BG_ATTR_SCREENSIZE, 0);
    SetBgAttribute(OW_BG_UPPER_ID, BG_ATTR_PALETTEMODE, 0);
    SetBgAttribute(OW_BG_UPPER_ID, BG_ATTR_PRIORITY, 2);
}

static void BattleOverworldScene_SetBackgroundLayout(const struct MapLayout *layout, u16 x, u16 y)
{
    if (layout == NULL)
        layout = &Route101_Layout;

    sBattleOwBgLayout = layout;
    if (layout->width <= OW_BG_MAP_WIDTH)
        x = 0;
    else if (x > layout->width - OW_BG_MAP_WIDTH)
        x = layout->width - OW_BG_MAP_WIDTH;
    if (layout->height <= OW_BG_MAP_HEIGHT)
        y = 0;
    else if (y > layout->height - OW_BG_MAP_HEIGHT)
        y = layout->height - OW_BG_MAP_HEIGHT;
    sBattleOwBgMapX = x;
    sBattleOwBgMapY = y;
}

static void BattleOverworldScene_InitBackgroundAnimation(void)
{
    InitTilesetAnimationsForLayout(sBattleOwBgLayout);
    sBattleOwBgTilesetAnimsActive = TRUE;
}

bool8 BattleOverworldScene_GetBackgroundOffsetForLayout(u16 layoutId, u16 *x, u16 *y)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sBattleOwBgLayoutOffsets); i++)
    {
        if (sBattleOwBgLayoutOffsets[i].layoutId == layoutId)
        {
            *x = sBattleOwBgLayoutOffsets[i].x;
            *y = sBattleOwBgLayoutOffsets[i].y;
            return TRUE;
        }
    }

    return FALSE;
}

static void BattleOverworldScene_SetCurrentMapBackgroundLayout(void)
{
    u16 layoutId = gMapHeader.mapLayoutId;
    u16 x = 0;
    u16 y = 0;

    if (gSaveBlock1Ptr != NULL && gSaveBlock1Ptr->mapLayoutId != 0)
        layoutId = gSaveBlock1Ptr->mapLayoutId;

    BattleOverworldScene_GetBackgroundOffsetForLayout(layoutId, &x, &y);
    BattleOverworldScene_SetBackgroundLayout(gMapHeader.mapLayout, x, y);
}

static void BattleOverworldScene_DrawBackground(bool8 visible)
{
    u8 palMap[16];
    u8 x;
    u8 y;
    u16 metatileId;
    u16 *lowerBg = (u16 *)BG_SCREEN_ADDR(OW_BG_LOWER_SCREENBASE);
    u16 *upperBg = (u16 *)BG_SCREEN_ADDR(OW_BG_UPPER_SCREENBASE);

    BuildMapTileAndPaletteMaps(palMap);
    UpdateMapPaletteMask(palMap);

    CpuFill16(0, lowerBg, BG_SCREEN_SIZE);
    CpuFill16(0, upperBg, BG_SCREEN_SIZE);

    for (y = 0; y < OW_BG_MAP_HEIGHT; y++)
    {
        for (x = 0; x < OW_BG_MAP_WIDTH; x++)
        {
            metatileId = GetMapMetatileIdAt(x, y);
            DrawMapMetatile(lowerBg, upperBg, x, y, GetMapMetatile(metatileId), palMap);
        }
    }

    gBattle_BG2_X = 0;
    gBattle_BG2_Y = 0;
    gBattle_BG3_X = 0;
    gBattle_BG3_Y = 0;
    BattleOverworldScene_ApplyBgConfig();
    if (visible)
    {
        ShowBg(OW_BG_UPPER_ID);
        ShowBg(OW_BG_LOWER_ID);
    }
    else
    {
        HideBg(OW_BG_UPPER_ID);
        HideBg(OW_BG_LOWER_ID);
    }
    SetGpuReg(REG_OFFSET_BG2CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_UPPER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG3CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_LOWER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG2HOFS, gBattle_BG2_X);
    SetGpuReg(REG_OFFSET_BG2VOFS, gBattle_BG2_Y);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);
}

void BattleOverworldScene_LoadBackground(void)
{
    u8 palMap[16];

    if (!IsBattleOverworldSceneEnabled())
        return;

    BattleOverworldScene_SetCurrentMapBackgroundLayout();
    BuildMapTileAndPaletteMaps(palMap);
    LoadMappedMapTiles();
    BattleOverworldScene_InitBackgroundAnimation();
    BattleOverworldScene_DrawBackground(sSceneVisible);
}

void BattleOverworldScene_RestoreBackground(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return;

    BattleOverworldScene_DrawBackground(sSceneVisible);
}

void BattleOverworldScene_LoadDebugBackground(const struct MapLayout *layout, u16 x, u16 y)
{
    u8 palMap[16];

    BattleOverworldScene_SetBackgroundLayout(layout, x, y);
    BuildMapTileAndPaletteMaps(palMap);
    LoadMappedMapTiles();
    BattleOverworldScene_InitBackgroundAnimation();
    BattleOverworldScene_DrawBackground(TRUE);
}

void BattleOverworldScene_StopBackgroundAnimation(void)
{
    sBattleOwBgTilesetAnimsActive = FALSE;
}

void BattleOverworldScene_UpdateBackgroundAnimation(void)
{
    if (sBattleOwBgTilesetAnimsActive)
        UpdateTilesetAnimations();
}

void BattleOverworldScene_TransferBackgroundAnimation(void)
{
    if (sBattleOwBgTilesetAnimsActive)
        TransferTilesetAnimsBuffer();
}

bool8 BattleOverworldScene_IsTilesetAnimActive(void)
{
    return sBattleOwBgTilesetAnimsActive;
}

bool8 BattleOverworldScene_GetTilesetAnimDestination(u16 sourceTile, u16 **dest)
{
    u16 mappedTile;

    if (!sBattleOwBgTilesetAnimsActive || sourceTile >= NUM_TILES_TOTAL)
        return FALSE;

    mappedTile = sBattleOwBgTileMap[sourceTile];
    if (mappedTile == OW_BG_TILE_UNMAPPED || mappedTile == 0)
        return FALSE;

    *dest = (u16 *)(BG_CHAR_ADDR(OW_BG_CHARBASE) + TILE_OFFSET_4BPP(mappedTile));
    return TRUE;
}

void BattleOverworldScene_KeepBaseBackgroundVisible(void)
{
    u16 bldCnt;
    u16 winIn;
    u16 winOut;

    if (!IsBattleOverworldSceneEnabled())
        return;

    gBattle_BG2_X = 0;
    gBattle_BG2_Y = 0;
    gBattle_BG3_X = 0;
    gBattle_BG3_Y = 0;
    BattleOverworldScene_ApplyBgConfig();
    if (sSceneVisible)
    {
        ShowBg(OW_BG_UPPER_ID);
        ShowBg(OW_BG_LOWER_ID);
    }
    else
    {
        HideBg(OW_BG_UPPER_ID);
        HideBg(OW_BG_LOWER_ID);
    }
    SetGpuReg(REG_OFFSET_BG2CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_UPPER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG3CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(OW_BG_CHARBASE) | BGCNT_16COLOR | BGCNT_SCREENBASE(OW_BG_LOWER_SCREENBASE) | BGCNT_TXT256x256);
    SetGpuReg(REG_OFFSET_BG2HOFS, gBattle_BG2_X);
    SetGpuReg(REG_OFFSET_BG2VOFS, gBattle_BG2_Y);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);

    winIn = GetGpuReg(REG_OFFSET_WININ);
    winOut = GetGpuReg(REG_OFFSET_WINOUT);
    SetGpuReg(REG_OFFSET_WININ, winIn | WININ_WIN0_BG2 | WININ_WIN1_BG2 | WININ_WIN0_BG3 | WININ_WIN1_BG3);
    SetGpuReg(REG_OFFSET_WINOUT, winOut | WINOUT_WIN01_BG2 | WINOUT_WINOBJ_BG2 | WINOUT_WIN01_BG3 | WINOUT_WINOBJ_BG3);

    if (sReshowTransitionAllowsBg3Blend && sBg3BlendFadeStarted && !gPaletteFade.active)
    {
        sReshowTransitionAllowsBg3Blend = FALSE;
        sBg3BlendFadeStarted = FALSE;
    }

    if (!sReshowTransitionAllowsBg3Blend && sBg3BlendRefCount == 0)
    {
        bldCnt = GetGpuReg(REG_OFFSET_BLDCNT);
        if (bldCnt & (BLDCNT_TGT1_BG2 | BLDCNT_TGT2_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT2_BG3))
            SetGpuReg(REG_OFFSET_BLDCNT, bldCnt & ~(BLDCNT_TGT1_BG2 | BLDCNT_TGT2_BG2 | BLDCNT_TGT1_BG3 | BLDCNT_TGT2_BG3));
    }
}

bool8 BattleOverworldScene_IsProtectedBg(u8 bgId)
{
    return IsBattleOverworldSceneEnabled() && (bgId == OW_BG_UPPER_ID || bgId == OW_BG_LOWER_ID);
}

void BattleOverworldScene_AddBg3BlendRef(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return;

    if (sBg3BlendRefCount != 0xFF)
        sBg3BlendRefCount++;
}

void BattleOverworldScene_RemoveBg3BlendRef(void)
{
    if (!IsBattleOverworldSceneEnabled())
        return;

    if (sBg3BlendRefCount != 0)
        sBg3BlendRefCount--;
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
    StartSpriteAnim(sprite, BattleOverworldScene_GetBattlerAnimNum(battler));
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
    struct Pokemon *mon;
    u16 species;
    u16 graphicsId;
    const struct ObjectEventGraphicsInfo *info;

    if (!IsBattleOverworldSceneEnabled() || battler >= MAX_BATTLERS_COUNT)
        return;

    sOwBattlerSpriteIds[battler] = spriteId;

    if (spriteId >= MAX_SPRITES)
        return;

    mon = GetBattlerMon(battler);
    info = GetBattleOwMonGraphicsInfo(mon, battler, &species, &graphicsId);
    if (info != NULL)
        LoadSheetGraphicsInfo(info, graphicsId, &gSprites[spriteId]);
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

    if (gMain.callback2 != BattleMainCB2)
        return;

    if (!sSceneVisible)
        return;

    for (battler = 0; battler < MAX_BATTLERS_COUNT && battler < gBattlersCount; battler++)
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

    RestoreBattleOwTrainerPalettes();
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
    const u32 vanillaBattleBgMask = 0xE; // BG palettes 1, 2, and 3.

    if (!IsBattleOverworldSceneEnabled())
        return selectedPalettes;

    if (selectedPalettes & vanillaBattleBgMask)
        selectedPalettes = (selectedPalettes & ~vanillaBattleBgMask) | sBattleOwBgPaletteMask;

    return selectedPalettes;
}

void BattleOverworldScene_Reset(void)
{
    u8 battler;

    BattleOverworldScene_ResetSpriteReferences();
    sSceneSuspended = FALSE;
    sReshowTransitionAllowsBg3Blend = FALSE;
    sBg3BlendFadeStarted = FALSE;
    sSceneVisible = FALSE;
    sBg3BlendRefCount = 0;
    sBattleOwBgTilesetAnimsActive = FALSE;
    for (battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
        sPreparedHealthboxPartyIds[battler] = PARTY_SIZE;
    BattleOverworldScene_SetBackgroundLayout(&Route101_Layout, OW_BG_DEFAULT_MAP_X, OW_BG_DEFAULT_MAP_Y);
}

void BattleOverworldScene_ResetSpriteReferences(void)
{
    u8 battler;

    sPlayerTrainerSpriteId = SPRITE_NONE;
    sOpponentTrainerSpriteId = SPRITE_NONE;
    sOpponentTrainerPaletteTag = TAG_NONE;
    for (battler = 0; battler < MAX_BATTLERS_COUNT; battler++)
    {
        sOwBattlerSpriteIds[battler] = SPRITE_NONE;
        sOwBattlerHiddenByBall[battler] = FALSE;
    }
    sCreatedTrainerSprites = FALSE;
}

void BattleOverworldScene_PrepareHealthbox(u8 battler, struct Pokemon *mon, u8 partyId)
{
    if (!IsBattleOverworldSceneEnabled() || battler >= MAX_BATTLERS_COUNT || partyId >= PARTY_SIZE)
        return;
    if (gHealthboxSpriteIds[battler] >= MAX_SPRITES)
        return;

    UpdateHealthboxAttribute(gHealthboxSpriteIds[battler], mon, HEALTHBOX_ALL);
    SetHealthboxSpriteInvisible(gHealthboxSpriteIds[battler]);
    sPreparedHealthboxPartyIds[battler] = partyId;
}

bool8 BattleOverworldScene_IsHealthboxPrepared(u8 battler, u8 partyId)
{
    return IsBattleOverworldSceneEnabled()
        && battler < MAX_BATTLERS_COUNT
        && partyId < PARTY_SIZE
        && sPreparedHealthboxPartyIds[battler] == partyId;
}

void BattleOverworldScene_ClearPreparedHealthbox(u8 battler)
{
    if (battler < MAX_BATTLERS_COUNT)
        sPreparedHealthboxPartyIds[battler] = PARTY_SIZE;
}

static const struct ObjectEventGraphicsInfo *GetBattleOwTrainerGraphicsInfo(enum TrainerPicID trainerPic, enum TrainerClassID trainerClass)
{
    switch (trainerPic)
    {
    case TRAINER_PIC_HIKER:
        return &gObjectEventGraphicsInfo_Hiker;
    case TRAINER_PIC_AQUA_GRUNT_M:
    case TRAINER_PIC_AQUA_ADMIN_M:
        return &gObjectEventGraphicsInfo_AquaMemberM;
    case TRAINER_PIC_AQUA_GRUNT_F:
    case TRAINER_PIC_AQUA_ADMIN_F:
        return &gObjectEventGraphicsInfo_AquaMemberF;
    case TRAINER_PIC_AQUA_LEADER_ARCHIE:
        return &gObjectEventGraphicsInfo_Archie;
    case TRAINER_PIC_MAGMA_GRUNT_M:
    case TRAINER_PIC_MAGMA_ADMIN:
        return &gObjectEventGraphicsInfo_MagmaMemberM;
    case TRAINER_PIC_MAGMA_GRUNT_F:
        return &gObjectEventGraphicsInfo_MagmaMemberF;
    case TRAINER_PIC_MAGMA_LEADER_MAXIE:
        return &gObjectEventGraphicsInfo_Maxie;
    case TRAINER_PIC_POKEMON_BREEDER_F:
    case TRAINER_PIC_AROMA_LADY:
    case TRAINER_PIC_LADY:
        return &gObjectEventGraphicsInfo_Woman2;
    case TRAINER_PIC_COOLTRAINER_M:
    case TRAINER_PIC_DRAGON_TAMER:
        return &gObjectEventGraphicsInfo_Man3;
    case TRAINER_PIC_BIRD_KEEPER:
    case TRAINER_PIC_GUITARIST:
    case TRAINER_PIC_KINDLER:
        return &gObjectEventGraphicsInfo_Man5;
    case TRAINER_PIC_COLLECTOR:
    case TRAINER_PIC_POKEMANIAC:
    case TRAINER_PIC_RUIN_MANIAC:
    case TRAINER_PIC_BUG_MANIAC:
        return &gObjectEventGraphicsInfo_Maniac;
    case TRAINER_PIC_SWIMMER_M:
        return &gObjectEventGraphicsInfo_SwimmerM;
    case TRAINER_PIC_SWIMMER_F:
        return &gObjectEventGraphicsInfo_SwimmerF;
    case TRAINER_PIC_SWIMMING_TRIATHLETE_M:
        return &gObjectEventGraphicsInfo_RunningTriathleteM;
    case TRAINER_PIC_SWIMMING_TRIATHLETE_F:
        return &gObjectEventGraphicsInfo_RunningTriathleteF;
    case TRAINER_PIC_EXPERT_M:
    case TRAINER_PIC_OLD_COUPLE:
        return &gObjectEventGraphicsInfo_ExpertM;
    case TRAINER_PIC_EXPERT_F:
        return &gObjectEventGraphicsInfo_ExpertF;
    case TRAINER_PIC_BLACK_BELT:
        return &gObjectEventGraphicsInfo_BlackBelt;
    case TRAINER_PIC_BATTLE_GIRL:
        return &gObjectEventGraphicsInfo_Girl3;
    case TRAINER_PIC_HEX_MANIAC:
        return &gObjectEventGraphicsInfo_HexManiac;
    case TRAINER_PIC_INTERVIEWER:
        return &gObjectEventGraphicsInfo_ReporterM;
    case TRAINER_PIC_TUBER_F:
        return &gObjectEventGraphicsInfo_TuberF;
    case TRAINER_PIC_TUBER_M:
        return &gObjectEventGraphicsInfo_TuberM;
    case TRAINER_PIC_COOLTRAINER_F:
    case TRAINER_PIC_PARASOL_LADY:
        return &gObjectEventGraphicsInfo_Woman5;
    case TRAINER_PIC_BEAUTY:
        return &gObjectEventGraphicsInfo_Beauty;
    case TRAINER_PIC_RICH_BOY:
        return &gObjectEventGraphicsInfo_RichBoy;
    case TRAINER_PIC_CAMPER:
    case TRAINER_PIC_POKEMON_RANGER_M:
        return &gObjectEventGraphicsInfo_Camper;
    case TRAINER_PIC_PICNICKER:
    case TRAINER_PIC_POKEMON_RANGER_F:
        return &gObjectEventGraphicsInfo_Picnicker;
    case TRAINER_PIC_POKEMON_BREEDER_M:
        return &gObjectEventGraphicsInfo_Man4;
    case TRAINER_PIC_PSYCHIC_M:
        return &gObjectEventGraphicsInfo_PsychicM;
    case TRAINER_PIC_PSYCHIC_F:
        return &gObjectEventGraphicsInfo_Lass;
    case TRAINER_PIC_GENTLEMAN:
        return &gObjectEventGraphicsInfo_Gentleman;
    case TRAINER_PIC_ELITE_FOUR_SIDNEY:
        return &gObjectEventGraphicsInfo_Sidney;
    case TRAINER_PIC_ELITE_FOUR_PHOEBE:
        return &gObjectEventGraphicsInfo_Phoebe;
    case TRAINER_PIC_ELITE_FOUR_GLACIA:
        return &gObjectEventGraphicsInfo_Glacia;
    case TRAINER_PIC_ELITE_FOUR_DRAKE:
        return &gObjectEventGraphicsInfo_Drake;
    case TRAINER_PIC_LEADER_ROXANNE:
        return &gObjectEventGraphicsInfo_Roxanne;
    case TRAINER_PIC_LEADER_BRAWLY:
        return &gObjectEventGraphicsInfo_Brawly;
    case TRAINER_PIC_LEADER_WATTSON:
        return &gObjectEventGraphicsInfo_Wattson;
    case TRAINER_PIC_LEADER_FLANNERY:
        return &gObjectEventGraphicsInfo_Flannery;
    case TRAINER_PIC_LEADER_NORMAN:
        return &gObjectEventGraphicsInfo_Norman;
    case TRAINER_PIC_LEADER_WINONA:
        return &gObjectEventGraphicsInfo_Winona;
    case TRAINER_PIC_LEADER_TATE_AND_LIZA:
        return &gObjectEventGraphicsInfo_Liza;
    case TRAINER_PIC_LEADER_JUAN:
        return &gObjectEventGraphicsInfo_Juan;
    case TRAINER_PIC_SCHOOL_KID_M:
        return &gObjectEventGraphicsInfo_SchoolKidM;
    case TRAINER_PIC_SCHOOL_KID_F:
        return &gObjectEventGraphicsInfo_Girl3;
    case TRAINER_PIC_SR_AND_JR:
    case TRAINER_PIC_TWINS:
    case TRAINER_PIC_YOUNG_COUPLE:
    case TRAINER_PIC_SIS_AND_BRO:
        return &gObjectEventGraphicsInfo_Twin;
    case TRAINER_PIC_POKEFAN_M:
        return &gObjectEventGraphicsInfo_PokefanM;
    case TRAINER_PIC_POKEFAN_F:
        return &gObjectEventGraphicsInfo_PokefanF;
    case TRAINER_PIC_YOUNGSTER:
        return &gObjectEventGraphicsInfo_Youngster;
    case TRAINER_PIC_CHAMPION_WALLACE:
        return &gObjectEventGraphicsInfo_Wallace;
    case TRAINER_PIC_FISHERMAN:
        return &gObjectEventGraphicsInfo_Fisherman;
    case TRAINER_PIC_CYCLING_TRIATHLETE_M:
        return &gObjectEventGraphicsInfo_CyclingTriathleteM;
    case TRAINER_PIC_CYCLING_TRIATHLETE_F:
        return &gObjectEventGraphicsInfo_CyclingTriathleteF;
    case TRAINER_PIC_RUNNING_TRIATHLETE_M:
        return &gObjectEventGraphicsInfo_RunningTriathleteM;
    case TRAINER_PIC_RUNNING_TRIATHLETE_F:
        return &gObjectEventGraphicsInfo_RunningTriathleteF;
    case TRAINER_PIC_NINJA_BOY:
        return &gObjectEventGraphicsInfo_NinjaBoy;
    case TRAINER_PIC_SAILOR:
        return &gObjectEventGraphicsInfo_Sailor;
    case TRAINER_PIC_WALLY:
        return &gObjectEventGraphicsInfo_Wally;
    case TRAINER_PIC_BRENDAN:
        return &gObjectEventGraphicsInfo_RivalBrendanNormal;
    case TRAINER_PIC_MAY:
        return &gObjectEventGraphicsInfo_RivalMayNormal;
    case TRAINER_PIC_BUG_CATCHER:
        return &gObjectEventGraphicsInfo_BugCatcher;
    case TRAINER_PIC_LASS:
        return &gObjectEventGraphicsInfo_Lass;
    case TRAINER_PIC_STEVEN:
        return &gObjectEventGraphicsInfo_Steven;
    case TRAINER_PIC_SALON_MAIDEN_ANABEL:
        return &gObjectEventGraphicsInfo_Anabel;
    case TRAINER_PIC_DOME_ACE_TUCKER:
        return &gObjectEventGraphicsInfo_Tucker;
    case TRAINER_PIC_PALACE_MAVEN_SPENSER:
        return &gObjectEventGraphicsInfo_Spenser;
    case TRAINER_PIC_ARENA_TYCOON_GRETA:
        return &gObjectEventGraphicsInfo_Greta;
    case TRAINER_PIC_FACTORY_HEAD_NOLAND:
        return &gObjectEventGraphicsInfo_Noland;
    case TRAINER_PIC_PIKE_QUEEN_LUCY:
        return &gObjectEventGraphicsInfo_Lucy;
    case TRAINER_PIC_PYRAMID_KING_BRANDON:
        return &gObjectEventGraphicsInfo_Brandon;
    case TRAINER_PIC_RED:
        return &gObjectEventGraphicsInfo_Red;
    case TRAINER_PIC_LEAF:
        return &gObjectEventGraphicsInfo_Leaf;
    case TRAINER_PIC_RS_BRENDAN:
        return &gObjectEventGraphicsInfo_RubySapphireBrendan;
    case TRAINER_PIC_RS_MAY:
        return &gObjectEventGraphicsInfo_RubySapphireMay;
    default:
        break;
    }

    switch (trainerClass)
    {
    case TRAINER_CLASS_HIKER:
        return &gObjectEventGraphicsInfo_Hiker;
    case TRAINER_CLASS_TEAM_AQUA:
    case TRAINER_CLASS_AQUA_ADMIN:
        return &gObjectEventGraphicsInfo_AquaMemberM;
    case TRAINER_CLASS_AQUA_LEADER:
        return &gObjectEventGraphicsInfo_Archie;
    case TRAINER_CLASS_TEAM_MAGMA:
    case TRAINER_CLASS_MAGMA_ADMIN:
        return &gObjectEventGraphicsInfo_MagmaMemberM;
    case TRAINER_CLASS_MAGMA_LEADER:
        return &gObjectEventGraphicsInfo_Maxie;
    case TRAINER_CLASS_PKMN_BREEDER:
    case TRAINER_CLASS_AROMA_LADY:
    case TRAINER_CLASS_LADY:
        return &gObjectEventGraphicsInfo_Woman2;
    case TRAINER_CLASS_BEAUTY:
        return &gObjectEventGraphicsInfo_Beauty;
    case TRAINER_CLASS_PARASOL_LADY:
        return &gObjectEventGraphicsInfo_Woman5;
    case TRAINER_CLASS_COOLTRAINER:
    case TRAINER_CLASS_COOLTRAINER_2:
    case TRAINER_CLASS_DRAGON_TAMER:
        return &gObjectEventGraphicsInfo_Man3;
    case TRAINER_CLASS_BIRD_KEEPER:
    case TRAINER_CLASS_GUITARIST:
    case TRAINER_CLASS_KINDLER:
        return &gObjectEventGraphicsInfo_Man5;
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
        return &gObjectEventGraphicsInfo_BlackBelt;
    case TRAINER_CLASS_BATTLE_GIRL:
        return &gObjectEventGraphicsInfo_Girl3;
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
    case TRAINER_CLASS_CAMPER:
        return &gObjectEventGraphicsInfo_Camper;
    case TRAINER_CLASS_PICNICKER:
    case TRAINER_CLASS_PKMN_RANGER:
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
        return &gObjectEventGraphicsInfo_Wallace;
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
        return &gObjectEventGraphicsInfo_Anabel;
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

static enum TrainerClassID GetBattleOwOpponentTrainerClass(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
        return GetFrontierOpponentClass(TRAINER_BATTLE_PARAM.opponentA);
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER_HILL)
        return TRAINER_CLASS_EXPERT;

    return GetTrainerClassFromId(TRAINER_BATTLE_PARAM.opponentA);
}

static enum TrainerPicID GetBattleOwOpponentTrainerPic(void)
{
    if (gBattleTypeFlags & BATTLE_TYPE_FRONTIER)
        return GetFrontierTrainerFrontSpriteId(TRAINER_BATTLE_PARAM.opponentA);
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER_HILL)
        return TRAINER_PIC_NONE;

    return GetTrainerPicFromId(TRAINER_BATTLE_PARAM.opponentA);
}

static const u16 *GetBattleOwPlayerTrainerPalette(void)
{
    return gSaveBlock2Ptr->playerGender == FEMALE ? gObjectEventPal_May : gObjectEventPal_Brendan;
}

static void LoadBattleOwPlayerTrainerPalette(void)
{
    const struct SpritePalette palette =
    {
        .data = GetBattleOwPlayerTrainerPalette(),
        .tag = TAG_OW_TRAINER_PLAYER_PAL,
    };

    LoadSpritePaletteInSlot(&palette, OW_TRAINER_PLAYER_PAL_SLOT);
}

static void ReserveLoadedBattleOwTrainerPalette(u8 paletteSlot, u16 paletteTag)
{
    const struct SpritePalette palette =
    {
        .data = &gPlttBufferUnfaded[OBJ_PLTT_ID(paletteSlot)],
        .tag = paletteTag,
    };

    LoadSpritePaletteInSlot(&palette, paletteSlot);
}

static void LoadBattleOwOpponentTrainerPalette(u16 objectPaletteTag)
{
    PatchObjectPalette(objectPaletteTag, OW_TRAINER_OPPONENT_PAL_SLOT);
    ReserveLoadedBattleOwTrainerPalette(OW_TRAINER_OPPONENT_PAL_SLOT, TAG_OW_TRAINER_OPPONENT_PAL);
}

static void RestoreBattleOwTrainerPalettes(void)
{
    if (!sSceneVisible)
        return;

    if (gPaletteFade.active)
        return;

    if (sPlayerTrainerSpriteId < MAX_SPRITES && gSprites[sPlayerTrainerSpriteId].inUse)
        LoadBattleOwPlayerTrainerPalette();

    if (sOpponentTrainerSpriteId < MAX_SPRITES && gSprites[sOpponentTrainerSpriteId].inUse)
        LoadBattleOwOpponentTrainerPalette(sOpponentTrainerPaletteTag);
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
    LoadBattleOwPlayerTrainerPalette();
    sPlayerTrainerSpriteId = CreateSprite(&template, 36, OW_SCENE_BASE_Y, 1);
    if (sPlayerTrainerSpriteId != MAX_SPRITES)
    {
        gSprites[sPlayerTrainerSpriteId].oam.paletteNum = OW_TRAINER_PLAYER_PAL_SLOT;
        gSprites[sPlayerTrainerSpriteId].oam.priority = 0;
    }

    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
    {
        opponentGraphicsInfo = GetBattleOwTrainerGraphicsInfo(GetBattleOwOpponentTrainerPic(), GetBattleOwOpponentTrainerClass());
        template = sTrainerTemplate;
        template.oam = opponentGraphicsInfo->oam;
        template.images = opponentGraphicsInfo->images;
        template.anims = sAnimTable_BattleTrainerFaceWest;
        sOpponentTrainerPaletteTag = opponentGraphicsInfo->paletteTag;
        LoadBattleOwOpponentTrainerPalette(sOpponentTrainerPaletteTag);
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

bool8 BattleOverworldScene_GetPlayerTrainerSpriteCoords(s16 *x, s16 *y)
{
    if (!IsBattleOverworldSceneEnabled()
     || sPlayerTrainerSpriteId >= MAX_SPRITES
     || !gSprites[sPlayerTrainerSpriteId].inUse)
        return FALSE;

    *x = gSprites[sPlayerTrainerSpriteId].x;
    *y = gSprites[sPlayerTrainerSpriteId].y;
    return TRUE;
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

static const u16 *GetBattleOwMonPalette(u16 species, bool32 shiny, bool32 female)
{
#if OW_POKEMON_OBJECT_EVENTS == TRUE && OW_PKMN_OBJECTS_SHARE_PALETTES == FALSE
#if P_GENDER_DIFFERENCES
    if (female)
    {
        if (shiny && gSpeciesInfo[species].overworldShinyPaletteFemale != NULL)
            return gSpeciesInfo[species].overworldShinyPaletteFemale;
        if (!shiny && gSpeciesInfo[species].overworldPaletteFemale != NULL)
            return gSpeciesInfo[species].overworldPaletteFemale;
    }
#endif
    if (shiny && gSpeciesInfo[species].overworldShinyPalette != NULL)
        return gSpeciesInfo[species].overworldShinyPalette;
    if (!shiny && gSpeciesInfo[species].overworldPalette != NULL)
        return gSpeciesInfo[species].overworldPalette;
#endif
    return GetMonSpritePalFromSpecies(species, shiny, female);
}

static u16 GetBattleOwMonGraphicsId(u16 species, bool32 shiny, bool32 female)
{
    u16 graphicsId = species + OBJ_EVENT_MON;

    if (shiny)
        graphicsId += OBJ_EVENT_MON_SHINY;
    if (female)
        graphicsId += OBJ_EVENT_MON_FEMALE;

    return graphicsId;
}

u8 BattleOverworldScene_GetBattlerAnimNum(u8 battler)
{
    enum Direction direction = IsPlayerBattlerPosition(GetBattlerPosition(battler)) ? DIR_EAST : DIR_WEST;

    return GetMoveDirectionAnimNum(direction);
}

static const struct ObjectEventGraphicsInfo *GetBattleOwMonGraphicsInfo(struct Pokemon *mon, u8 battler, u16 *species, u16 *graphicsId)
{
    bool32 shiny;
    bool32 female;

    if (gBattleSpritesDataPtr->battlerData[battler].transformSpecies == SPECIES_NONE)
        *species = GetMonData(mon, MON_DATA_SPECIES);
    else
        *species = gBattleSpritesDataPtr->battlerData[battler].transformSpecies;

    shiny = IsMonShiny(mon);
    female = GetMonGender(mon) == MON_FEMALE;
    *graphicsId = GetBattleOwMonGraphicsId(*species, shiny, female);

    return SpeciesToGraphicsInfo(*species, shiny, female);
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
    if (!BattleOverworldScene_SetMonSpriteTemplate(species, battler))
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
        StartSpriteAnim(&gSprites[gBattlerSpriteIds[battler]], BattleOverworldScene_GetBattlerAnimNum(battler));
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

    for (battler = 0; battler < MAX_BATTLERS_COUNT && battler < gBattlersCount; battler++)
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

    for (battler = 0; battler < MAX_BATTLERS_COUNT && battler < gBattlersCount; battler++)
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

    for (battler = 0; battler < MAX_BATTLERS_COUNT && battler < gBattlersCount; battler++)
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
    u16 species;
    u16 graphicsId;
    const struct ObjectEventGraphicsInfo *info;
    bool32 shiny;
    bool32 female;
    const u16 *palette;

    if (!IsBattleOverworldSceneEnabled())
        return FALSE;

    shiny = IsMonShiny(mon);
    female = GetMonGender(mon) == MON_FEMALE;
    info = GetBattleOwMonGraphicsInfo(mon, battler, &species, &graphicsId);
    if (info == NULL)
        return FALSE;

    LoadSheetGraphicsInfo(info, graphicsId, NULL);
    palette = GetBattleOwMonPalette(species, shiny, female);
    LoadPalette(palette, OBJ_PLTT_ID(battler), PLTT_SIZE_4BPP);
    LoadPalette(palette, BG_PLTT_ID(8) + BG_PLTT_ID(battler), PLTT_SIZE_4BPP);

    return TRUE;
}

bool8 BattleOverworldScene_SetMonSpriteTemplate(u16 species, u8 battler)
{
    struct Pokemon *mon;
    u16 graphicsSpecies;
    u16 graphicsId;
    u8 battlerPosition;
    const struct ObjectEventGraphicsInfo *info;

    (void)species;

    if (!IsBattleOverworldSceneEnabled())
        return FALSE;

    mon = GetBattlerMon(battler);
    info = GetBattleOwMonGraphicsInfo(mon, battler, &graphicsSpecies, &graphicsId);
    if (info == NULL)
        return FALSE;

    battlerPosition = GetBattlerPosition(battler);
    gMultiuseSpriteTemplate = gMonSpritesGfxPtr->templates[battlerPosition];
    gMultiuseSpriteTemplate.tileTag = LoadSheetGraphicsInfo(info, graphicsId, NULL);
    gMultiuseSpriteTemplate.oam = info->oam;
    gMultiuseSpriteTemplate.anims = info->anims;
    gMultiuseSpriteTemplate.images = info->images;
    gMultiuseSpriteTemplate.affineAnims = IsPlayerBattlerPosition(battlerPosition)
                                        ? gAffineAnims_BattleSpritePlayerSide
                                        : gAffineAnims_BattleSpriteOpponentSide;
    gMultiuseSpriteTemplate.paletteTag = TAG_NONE;

    return TRUE;
}

u8 BattleOverworldScene_CreatePreviewMonSprite(u16 species, u8 battlerPosition, s16 x, s16 y, u8 subpriority)
{
    u8 spriteId;
    u8 paletteNum;
    u16 graphicsId;
    enum Direction direction = IsPlayerBattlerPosition(battlerPosition) ? DIR_EAST : DIR_WEST;
    const struct ObjectEventGraphicsInfo *info;
    const u16 *palette;
    bool32 shiny = FALSE;
    bool32 female = FALSE;

    if (battlerPosition >= MAX_BATTLERS_COUNT)
        return MAX_SPRITES;

    info = SpeciesToGraphicsInfo(species, shiny, female);
    if (info == NULL)
        return MAX_SPRITES;

    paletteNum = AllocSpritePalette(species);
    if (paletteNum == 0xFF)
        return MAX_SPRITES;

    graphicsId = GetBattleOwMonGraphicsId(species, shiny, female);
    if (gMonSpritesGfxPtr != NULL)
        gMultiuseSpriteTemplate = gMonSpritesGfxPtr->templates[battlerPosition];
    else
        gMultiuseSpriteTemplate = gBattlerSpriteTemplates[battlerPosition];
    gMultiuseSpriteTemplate.tileTag = LoadSheetGraphicsInfo(info, graphicsId, NULL);
    gMultiuseSpriteTemplate.oam = info->oam;
    gMultiuseSpriteTemplate.anims = info->anims;
    gMultiuseSpriteTemplate.images = info->images;
    gMultiuseSpriteTemplate.affineAnims = IsPlayerBattlerPosition(battlerPosition)
                                        ? gAffineAnims_BattleSpritePlayerSide
                                        : gAffineAnims_BattleSpriteOpponentSide;
    gMultiuseSpriteTemplate.paletteTag = species;

    palette = GetBattleOwMonPalette(species, shiny, female);
    LoadPalette(palette, OBJ_PLTT_ID(paletteNum), PLTT_SIZE_4BPP);
    spriteId = CreateSprite(&gMultiuseSpriteTemplate, x, y, subpriority);
    if (spriteId == MAX_SPRITES)
    {
        FreeSpritePaletteByTag(species);
        return MAX_SPRITES;
    }

    gSprites[spriteId].coordOffsetEnabled = FALSE;
    gSprites[spriteId].oam.priority = 0;
    gSprites[spriteId].oam.paletteNum = paletteNum;
    gSprites[spriteId].callback = SpriteCallbackDummy;
    StartSpriteAnim(&gSprites[spriteId], GetMoveDirectionAnimNum(direction));
    return spriteId;
}
