/*
 * stubs.c - Desktop battle engine: global variable definitions and
 *           stub function implementations.
 *
 * Only defines things NOT covered by:
 *  - Inline stubs in shadow headers (window.h, palette.h, bg.h, etc.)
 *  - Actual implementations in compiled src/ files (util.c, pokemon.c, etc.)
 */

#include "global.h"
#include "battle.h"
#include "sprite.h"
#include "main.h"
#include "palette.h"
#include "m4a.h"
#include "link.h"
#include "scanline_effect.h"
#include "decompress.h"
#include "battle_interface.h"
#include "battle_bg.h"
#include "battle_setup.h"
#include "battle_tower.h"
#include "graphics.h"
#include "util.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "pokeblock.h"
#include "item.h"
#include "berry.h"
#include "pokeball.h"
#include "party_menu.h"
#include "frontier_util.h"
#include "recorded_battle.h"
#include "trainer_hill.h"
#include "reshow_battle_screen.h"
#include "item_menu.h"
#include "international_string_util.h"
#include "roamer.h"
#include "safari_zone.h"
#include "evolution_scene.h"
#include "tv.h"
#include "event_data.h"
#include "battle_anim.h"
#include "text.h"
#include "apprentice.h"
#include "pokemon_animation.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ===========================================================================
 * Global variable definitions
 * =========================================================================== */

/* GBA main loop state */
struct Main gMain = {0};
u16 gKeyRepeatStartDelay = 40;
u16 gKeyRepeatContinueDelay = 5;
bool8 gLinkTransferringData = FALSE;
bool8 gSoftResetDisabled = FALSE;
u8 gLinkVSyncDisabled = 0;
s8 gPcmDmaCounter = 0;
IntrFunc gIntrTable[15] = {0};
u32 IntrMain_Buffer[4] = {0};

const u8 gGameVersion = 2; /* Emerald */
const u8 gGameLanguage = 2; /* English */
const u8 RomHeaderGameCode[4] = {'B', 'P', 'E', 'E'};
const u8 RomHeaderSoftwareVersion = 0;

/* Save blocks - static instances */
static struct SaveBlock1 sSaveBlock1;
static struct SaveBlock2 sSaveBlock2;
struct SaveBlock1 *gSaveBlock1Ptr = &sSaveBlock1;
struct SaveBlock2 *gSaveBlock2Ptr = &sSaveBlock2;

/* Event data */
u16 gSpecialVar_Result = 0;
u16 gSpecialVar_0x8004 = 0;
u16 gSpecialVar_0x8005 = 0;
u16 gSpecialVar_0x8006 = 0;
u16 gSpecialVar_0x8007 = 0;
u16 gSpecialVar_0x8008 = 0;

/* Link state */
u8 gWirelessCommType = 0;
bool8 gReceivedRemoteLinkPlayers = FALSE;
struct LinkPlayer gLinkPlayers[MAX_RFU_PLAYERS] = {0};

/* Sprite system */
struct Sprite gSprites[MAX_SPRITES + 1] = {0};
u8 gOamLimit = 64;
u16 gReservedSpriteTileCount = 0;
u8 gReservedSpritePaletteCount = 0;
s16 gSpriteCoordOffsetX = 0;
s16 gSpriteCoordOffsetY = 0;
struct OamMatrix gOamMatrices[OAM_MATRIX_COUNT] = {0};
bool8 gAffineAnimsDisabled = FALSE;

/* Dummy sprite data */
const struct OamData gDummyOamData = {0};
static const union AnimCmd sDummyAnimCmd = {.type = -1};
const union AnimCmd *const gDummySpriteAnimTable[] = {&sDummyAnimCmd, NULL};
static const union AffineAnimCmd sDummyAffineAnimCmd = {.type = AFFINEANIMCMDTYPE_END};
static const union AffineAnimCmd *const sDummyAffineAnims[] = {&sDummyAffineAnimCmd, NULL};
const union AffineAnimCmd *const gDummySpriteAffineAnimTable[] = {&sDummyAffineAnims[0], NULL};
const struct SpriteTemplate gDummySpriteTemplate = {
    .tileTag = TAG_NONE,
    .paletteTag = TAG_NONE,
    .oam = &gDummyOamData,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = NULL,
};

/* Dummy OamData instances referenced from battle_anim.h */
const struct OamData gOamData_AffineOff_ObjNormal_8x16 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_16x16 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_8x8 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_8x8 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_16x16 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_32x16 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_32x32 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_64x32 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_16x16 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_32x32 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_16x16 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_32x32 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_64x64 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_32x32 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_16x32 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_8x8 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_32x32 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_64x64 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_32x64 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_32x16 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_32x16 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_16x32 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_32x64 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_32x64 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_32x32 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_64x64 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_64x64 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_64x32 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_8x16 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_16x16 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_16x16 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_8x8 = {0};
const struct OamData gOamData_AffineDouble_ObjBlend_8x16 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_8x8 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_8x16 = {0};
const struct OamData gOamData_AffineNormal_ObjBlend_8x8 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_8x16 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_64x64 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_32x64 = {0};
const struct OamData gOamData_AffineNormal_ObjNormal_64x64 = {0};
const struct OamData gOamData_AffineDouble_ObjNormal_32x16 = {0};
const struct OamData gOamData_AffineOff_ObjNormal_64x32 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_64x32 = {0};
const struct OamData gOamData_AffineOff_ObjBlend_16x32 = {0};

/* Palette state */
struct PaletteFadeControl gPaletteFade = {0};
u16 gPlttBufferUnfaded[PLTT_BUFFER_SIZE] = {0};
u16 gPlttBufferFaded[PLTT_BUFFER_SIZE] = {0};

/* Text flags */
TextFlags gTextFlags = {0};

/* PP text palette */
const u16 gPPTextPalette[16] = {0};

/* Scanline effect */
struct ScanlineEffect gScanlineEffect = {0};
u16 gScanlineEffectRegBuffers[2][160] = {0};

/* Decompress buffer */
u8 gDecompressionBuffer[0x4000] = {0};

/* Battle BG state defined in battle_main.c */

/* Battle interface state defined in battle_main.c */

/* Private storage for AllocateBattleSpritesData */
static struct BattleSpriteInfo      sBattlerData[MAX_BATTLERS_COUNT];
static struct BattleHealthboxInfo   sHealthBoxesData[MAX_BATTLERS_COUNT];
static struct BattleAnimationInfo   sAnimationData[MAX_BATTLERS_COUNT];
static struct BattleBarInfo         sBattleBars[MAX_BATTLERS_COUNT];
static struct BattleSpriteData      sBattleSpritesData = {
    .battlerData     = sBattlerData,
    .healthBoxesData = sHealthBoxesData,
    .animationData   = sAnimationData,
    .battleBars      = sBattleBars,
};
/* gBattleSpritesDataPtr is defined in battle_main.c */

/* M4A (audio) */
struct MusicPlayerInfo gMPlayInfo_BGM = {0};
struct MusicPlayerInfo gMPlayInfo_SE1 = {0};
struct MusicPlayerInfo gMPlayInfo_SE2 = {0};
struct MusicPlayerInfo gMPlayInfo_SE3 = {0};

/* Animation global state referenced from battle_anim.h externs */
void (*gAnimScriptCallback)(void) = NULL;
bool8 gAnimScriptActive = FALSE;
u8 gAnimVisualTaskCount = 0;
u8 gAnimSoundTaskCount = 0;
struct DisableStruct *gAnimDisableStructPtr = NULL;
s32 gAnimMoveDmg = 0;
u16 gAnimMovePower = 0;
u8 gAnimFriendship = 0;
u16 gWeatherMoveAnim = 0;
s16 gBattleAnimArgs[ANIM_ARGS_COUNT] = {0};
u8 gAnimMoveTurn = 0;
u8 gBattleAnimAttacker = 0;
u8 gBattleAnimTarget = 0;
u16 gAnimBattlerSpecies[MAX_BATTLERS_COUNT] = {0};
u8 gAnimCustomPanning = 0;

/* Graphics data stubs */
const u32 gBattleTerrainTable[8] = {0};
const u8 gBattleTextboxTiles[0x100] = {0};
const u16 gBattleTextboxPalette[16] = {0};
const u8 gBattleInterface_BallDisplayGfx[0x100] = {0};

/* Trig tables (referenced by battle code) */
const s16 gSineTable[256] = {0};

/* GBA heap (malloc/free are stdlib on desktop, heap unused) */
u8 gHeap[HEAP_SIZE] = {0};

/* Trainer/Frontier data */
u16 gTrainerBattleOpponent_A = 0;
u16 gTrainerBattleOpponent_B = 0;
u16 gPartnerTrainerId = 0;

const struct RematchTrainer gRematchTable[REMATCH_TABLE_ENTRIES] = {0};

/* Frontier tables */
const u8 gTowerMaleFacilityClasses[30] = {0};
const u8 gTowerMaleTrainerGfxIds[30] = {0};
const u8 gTowerFemaleFacilityClasses[20] = {0};
const u8 gTowerFemaleTrainerGfxIds[20] = {0};
const u16 gBattleFrontierHeldItems[] = {0};
const struct FacilityMon gBattleFrontierMons[] = {0};
const struct BattleFrontierTrainer gBattleFrontierTrainers[] = {0};
const struct FacilityMon gSlateportBattleTentMons[] = {0};
const struct BattleFrontierTrainer gSlateportBattleTentTrainers[] = {0};
u16 gFrontierTempParty[FRONTIER_PARTY_SIZE] = {0};
const struct BattleFrontierTrainer *gFacilityTrainers = NULL;
const struct FacilityMon *gFacilityTrainerMons = NULL;

/* Safari zone */
u8 gNumSafariBalls = 0;

/* Pokeblock */
u8 gPokeblockMonId = 0;
s16 gPokeblockGain = 0;

/* Pokemon storage */
struct PokemonStorage *gPokemonStoragePtr = NULL;

/* Last viewed mon (pokemon_summary_screen.h) */
u8 gLastViewedMonIndex = 0;

/* Party order */
u8 gBattlePartyCurrentOrder[PARTY_SIZE / 2] = {0};

/* Evolution callback */
void (*gCB2_AfterEvolution)(void) = NULL;

/* Link system */
u16 gBlockRecvBuffer[MAX_RFU_PLAYERS][BLOCK_BUFFER_SIZE / 2] = {0};

/* Special variables */
u16 gSpecialVar_MonBoxId = 0;
u16 gSpecialVar_MonBoxPos = 0;

/* RTC */
struct Time gLocalTime = {0};

/* Pokeblock compatibility */
const s8 gPokeblockFlavorCompatibilityTable[NUM_NATURES * FLAVOR_COUNT] = {0};

/* Apprentices */
const struct ApprenticeTrainer gApprentices[1] = {0};
void BufferApprenticeChallengeText(u8 saveApprenticeId) {}
void Apprentice_ScriptContext_Enable(void) {}
void ResetApprenticeStruct(struct Apprentice *apprentice) {}

/* Pokemon animation */
u8 GetSpeciesBackAnimSet(u16 species) { return 0; }
void LaunchAnimationTaskForFrontSprite(struct Sprite *sprite, u8 frontAnimId) {}
void StartMonSummaryAnimation(struct Sprite *sprite, u8 frontAnimId) {}
void LaunchAnimationTaskForBackSprite(struct Sprite *sprite, u8 backAnimSet) {}
void SetSpriteCB_MonAnimDummy(struct Sprite *sprite) {}

/* Recorded battle */
u32 gRecordedBattleRngSeed = 0;
u32 gBattlePalaceMoveSelectionRngValue = 0;
u8  gRecordedBattleMultiplayerId = 0;

/* Audio */
struct SoundInfo gSoundInfo = {0};

/* GBA I/O registers — desktop stubs */
#include <stdint.h>
volatile uint16_t REG_BG0HOFS = 0, REG_BG0VOFS = 0;
volatile uint16_t REG_BG1HOFS = 0, REG_BG1VOFS = 0;
volatile uint16_t REG_BG2HOFS = 0, REG_BG2VOFS = 0;
volatile uint16_t REG_BG3HOFS = 0, REG_BG3VOFS = 0;
volatile uint16_t REG_VCOUNT = 0;

/* ===========================================================================
 * main.h function implementations
 * =========================================================================== */

void SetMainCallback2(MainCallback callback) { gMain.callback2 = callback; }
void SetVBlankCallback(IntrCallback callback)  { gMain.vblankCallback = callback; }
void SetHBlankCallback(IntrCallback callback)  { gMain.hblankCallback = callback; }
void SetVCountCallback(IntrCallback callback)  { gMain.vcountCallback = callback; }
void SetSerialCallback(IntrCallback callback)  { gMain.serialCallback = callback; }
void InitKeys(void) {}
void InitFlashTimer(void) {}
void SetTrainerHillVBlankCounter(u32 *counter) {}
void ClearTrainerHillVBlankCounter(void) {}
void DoSoftReset(void) {}
void ClearPokemonCrySongs(void) {}
void RestoreSerialTimer3IntrHandlers(void) {}
void StartTimer1(void) {}
void SeedRngAndSetTrainerId(void) {}
u16 GetGeneratedTrainerIdLower(void) { return 0; }
void AgbMain(void) {}

/* ===========================================================================
 * Sprite system - minimal implementations
 * =========================================================================== */

void ResetSpriteData(void) { memset(gSprites, 0, sizeof(gSprites)); }
void AnimateSprites(void) {}
void BuildOamBuffer(void) {}

void SpriteCallbackDummy(struct Sprite *sprite) {}

u8 CreateSprite(const struct SpriteTemplate *template, s16 x, s16 y, u8 subpriority)
{
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!gSprites[i].inUse) {
            memset(&gSprites[i], 0, sizeof(struct Sprite));
            gSprites[i].inUse = TRUE;
            gSprites[i].x = x;
            gSprites[i].y = y;
            gSprites[i].template = template;
            if (template->oam) gSprites[i].oam = *template->oam;
            gSprites[i].anims = template->anims;
            gSprites[i].affineAnims = template->affineAnims;
            gSprites[i].callback = template->callback ? template->callback : SpriteCallbackDummy;
            gSprites[i].subpriority = subpriority;
            return i;
        }
    }
    return 0;
}

u8 CreateSpriteAtEnd(const struct SpriteTemplate *t, s16 x, s16 y, u8 sub) { return CreateSprite(t, x, y, sub); }
u8 CreateInvisibleSprite(SpriteCallback cb) { return 0; }
u8 CreateSpriteAndAnimate(const struct SpriteTemplate *t, s16 x, s16 y, u8 sub) { return CreateSprite(t, x, y, sub); }

void DestroySprite(struct Sprite *sprite) { if (sprite) sprite->inUse = FALSE; }
void ResetOamRange(u8 start, u8 end) {}
void LoadOam(void) {}
void SetSpriteTileNum(struct Sprite *sprite, u16 tileNum) {}
void CalcCenterToCornerVec(struct Sprite *sprite, u8 shape, u8 size, u8 affineMode) {}
void ProcessSpriteCopyRequests(void) {}
void RequestSpriteFrameImageCopy(u16 index, u16 tileNum, const struct SpriteFrameImage *images) {}
void RequestSpriteCopy(const u8 *src, u8 *dest, u16 size) {}
void CopyFromOamBuffer(u8 start, u8 end) {}
void FreeSpriteTilesByTag(u16 tag) {}
void FreeSpriteOamMatrix(struct Sprite *sprite) {}
u16 LoadSpriteSheet(const struct SpriteSheet *sheet) { return 0; }
void LoadSpriteSheets(const struct SpriteSheet *sheets) {}
void FreeAllSpritePalettes(void) {}
u8 LoadSpritePalette(const struct SpritePalette *palette) { return 0; }
void LoadSpritePalettes(const struct SpritePalette *palettes) {}
void StartSpriteAnim(struct Sprite *sprite, u8 animNum) {}
void StartSpriteAnimIfDifferent(struct Sprite *sprite, u8 animNum) {}
void SeekSpriteAnim(struct Sprite *sprite, u8 animCmdIndex) {}
void StartSpriteAffineAnim(struct Sprite *sprite, u8 animNum) {}
void StartSpriteAffineAnimIfDifferent(struct Sprite *sprite, u8 animNum) {}
void ChangeSpriteAffineAnim(struct Sprite *sprite, u8 animNum) {}
void ChangeSpriteAffineAnimIfDifferent(struct Sprite *sprite, u8 animNum) {}
void SetSpriteMatrixAnchor(struct Sprite *sprite, s16 x, s16 y) {}
void SetAffineSpriteTileRange(u8 startId, u8 count, u8 matrixNum) {}
void FreeOamMatrix(u8 matrixNum) {}
u8 AllocOamMatrix(void) { return 0; }
void InitSpriteAffineAnim(struct Sprite *sprite) {}
void SetSpriteAffineMatrix(u8 matrixNum, struct ObjAffineSrcData *src) {}
u16 LoadCompressedSpriteSheet(const struct CompressedSpriteSheet *sheet) { return 0; }
void LoadCompressedSpriteSheets(const struct CompressedSpriteSheet *sheets) {}
u8 LoadCompressedSpritePalette(const struct CompressedSpritePalette *palette) { return 0; }
void LoadCompressedSpritePalettes(const struct CompressedSpritePalette *palettes) {}
bool8 CheckIfSpriteTileTagExists(u16 tag) { return FALSE; }
u16 GetSpriteTileStartByTag(u16 tag) { return 0xFFFF; }
u16 GetSpritePaletteTagByPaletteNum(u8 paletteNum) { return 0xFFFF; }
u8 GetSpritePaletteNumByTag(u16 tag) { return 0xFF; }
void SetSubspriteTables(struct Sprite *sprite, const struct SubspriteTable *subspriteTables) {}

/* ===========================================================================
 * Battle sprite data - real implementation needed
 * =========================================================================== */

void AllocateBattleSpritesData(void)
{
    memset(sBattlerData,     0, sizeof(sBattlerData));
    memset(sHealthBoxesData, 0, sizeof(sHealthBoxesData));
    memset(sAnimationData,   0, sizeof(sAnimationData));
    memset(sBattleBars,      0, sizeof(sBattleBars));
    gBattleSpritesDataPtr = &sBattleSpritesData;
    gBattleSpritesDataPtr->battlerData     = sBattlerData;
    gBattleSpritesDataPtr->healthBoxesData = sHealthBoxesData;
    gBattleSpritesDataPtr->animationData   = sAnimationData;
    gBattleSpritesDataPtr->battleBars      = sBattleBars;
}

void FreeBattleSpritesData(void) {}

/* ===========================================================================
 * Battle message display - defined in battle_message.c (compiled)
 * =========================================================================== */

/* BattlePutTextOnWindow defined in battle_message.c */

/* ===========================================================================
 * Game system stubs
 * =========================================================================== */

/* Link — not in any shadow header */
/* TryReceiveLinkBattleData defined in battle_controllers.c */
/* PrepareBufferDataTransferLink defined in battle_controllers.c */
void Task_WaitForLinkPlayerConnection(u8 taskId) {}
u8 GetMultiplayerId(void) { return 0; }
u8 BitmaskAllOtherLinkPlayers(void) { return 0; }
bool8 SendBlock(u8 unused, const void *src, u16 size) { return TRUE; }
u8 GetBlockReceivedStatus(void) { return 0; }
void ResetBlockReceivedFlags(void) {}
bool8 IsLinkTaskFinished(void) { return TRUE; }
bool8 IsLinkRfuTaskFinished(void) { return TRUE; }
void ResetBlockReceivedFlag(u8 playerId) {}
void CheckShouldAdvanceLinkState(void) {}

/* Item — non-static implementation for ItemId_GetName (others are static inline in item.h shadow) */
static const u8 sNoItemName[] = {0xFF}; /* EOS */
const u8 *ItemId_GetName(u16 itemId) { return sNoItemName; }

/* Reshow battle (declared in battle_controllers.h) */
void CB2_SetUpReshowBattleScreenAfterMenu(void) {}
void CB2_SetUpReshowBattleScreenAfterMenu2(void) {}

/* Palette — non-static implementations (removed from palette.h shadow) */
u8 AllocSpritePalette(u16 tag) { return 0; }
u8 IndexOfSpritePaletteTag(u16 tag) { return 0xFF; }
void FreeSpritePaletteByTag(u16 tag) {}

/* International string util — stub implementations */
bool8 IsStringSmaller(const u8 *str1, const u8 *str2) { return FALSE; }
void GetTrainerNameFormatted(u8 *dst, u16 trainerId) { if (dst) dst[0] = 0xFF; }

/* Trainer data */
u8 GetTrainerBattleMode(void) { return 0; }
bool32 GetTrainerFlagFromScriptPointer(const u8 *data) { return FALSE; }

/* Battle sprites (defined in battle_gfx_sfx_util.c, not compiled) — also in data_stubs.c */

/* Berry stubs (defined in berry.c, not compiled) */
bool32 IsEnigmaBerryValid(void) { return FALSE; }
u8 ItemIdToBerryType(u16 item) { return 0; }

/* Recorded battle (defined in recorded_battle.c, not compiled) */
void RecordedBattle_SetTrainerInfo(void) {}
void RecordedBattle_SetFrontierPassFlagFromHword(u16 flags) {}

/* gMoveNames is defined in battle_desktop/stubs/text_data.c */
