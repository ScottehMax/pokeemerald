/*
 * data_stubs.c - Stub definitions for battle data tables, battle scripts,
 *                and functions from uncompiled source modules.
 *
 * These replace: battle_bg.c, battle_gfx_sfx_util.c, recorded_battle.c,
 *                link.c, link_rfu.c, sound.c, tv.c, frontier_util.c,
 *                trainer_hill.c, pokemon_storage_system.c, etc.
 */

#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_bg.h"
#include "battle_gfx_sfx_util.h"
#include "battle_interface.h"
#include "battle_main.h"
#include "battle_message.h"
#include "bg.h"
#include "data.h"
#include "decompress.h"
#include "evolution_scene.h"
#include "field_specials.h"
#include "frontier_util.h"
#include "item.h"
#include "link.h"
#include "menu.h"
#include "menu_specialized.h"
#include "overworld.h"
#include "palette.h"
#include "party_menu.h"
#include "pokeball.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "recorded_battle.h"
#include "roamer.h"
#include "script_menu.h"
#include "sound.h"
#include "trainer_hill.h"
#include "tv.h"
#include "window.h"
#include "apprentice.h"
#include "international_string_util.h"
#include "constants/battle_move_effects.h"
#include "constants/battle_script_commands.h"

/* ===========================================================================
 * Battle script pointer tables (indexed by ball type / etc.)
 * NOTE: BattleScript_* labels and gBattleScriptsForMoveEffects are now
 * provided by battle_desktop/generated/battle_scripts.c (Option A).
 * =========================================================================== */

static const u8 sBattleScriptEnd[] = {B_SCR_OP_END};

const u8 *const gBattlescriptsForBallThrow[ITEMS_COUNT] = {
    [0 ... (ITEMS_COUNT - 1)] = sBattleScriptEnd,
};

const u8 *const gBattlescriptsForRunningByItem[1] = {sBattleScriptEnd};
const u8 *const gBattlescriptsForUsingItem[2]     = {sBattleScriptEnd, sBattleScriptEnd};
const u8 *const gBattlescriptsForSafariActions[4] = {
    sBattleScriptEnd, sBattleScriptEnd, sBattleScriptEnd, sBattleScriptEnd
};

/* AI script end sentinel: opcode 0x5A = Cmd_end in sBattleAICmdTable.
 * This is different from the battle-script end opcode (B_SCR_OP_END = 0x43).
 * Using the wrong opcode here causes Cmd_if_any_move_disabled_or_encored to
 * be called instead, which reads garbage data and loops forever (especially
 * in doubles where AI_SCRIPT_DOUBLE_BATTLE forces script execution). */
static const u8 sAIScriptEnd[] = {0x5A}; /* Cmd_end */

/* AI scripts table (one entry per AI logic ID; use 32 slots for safety) */
const u8 *const gBattleAI_ScriptsTable[32] = {
    [0 ... 31] = sAIScriptEnd,
};

/* ===========================================================================
 * Sprite / picture tables
 * =========================================================================== */

static const struct SpriteFrameImage sDummyFrameImage = {NULL, 0};

const struct SpriteFrameImage gBattlerPicTable_PlayerLeft[]   = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_OpponentLeft[] = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_PlayerRight[]  = {sDummyFrameImage};
const struct SpriteFrameImage gBattlerPicTable_OpponentRight[]= {sDummyFrameImage};

const struct SpriteFrameImage gTrainerBackPicTable_Brendan[]        = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_May[]             = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Red[]             = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Leaf[]            = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_RubySapphireBrendan[] = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_RubySapphireMay[] = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Wally[]           = {sDummyFrameImage};
const struct SpriteFrameImage gTrainerBackPicTable_Steven[]          = {sDummyFrameImage};

static const union AffineAnimCmd sDummyAffineAnimEnd  = {.type = AFFINEANIMCMDTYPE_END};
static const union AffineAnimCmd *const sDummyAffineAnimSeq[] = {&sDummyAffineAnimEnd, NULL};

const union AffineAnimCmd *const gAffineAnims_BattleSpritePlayerSide[]   = {sDummyAffineAnimSeq[0], NULL};
const union AffineAnimCmd *const gAffineAnims_BattleSpriteOpponentSide[] = {sDummyAffineAnimSeq[0], NULL};

static const union AnimCmd sDummyAnimEnd = {.type = -1};
static const union AnimCmd *const sDummyAnimTable[] = {&sDummyAnimEnd, NULL};
const union AnimCmd *const gAnims_MonPic[] = {&sDummyAnimEnd};

/* Mon front picture coordinates */
const struct MonCoords gMonFrontPicCoords[NUM_SPECIES + 1] = {{0}};

/* Palette tables */
const struct CompressedSpritePalette gMonPaletteTable[NUM_SPECIES + 1]      = {{{NULL}, 0}};
const struct CompressedSpritePalette gMonShinyPaletteTable[NUM_SPECIES + 1] = {{{NULL}, 0}};

/* Animation tables */
const union AnimCmd *const *const gMonFrontAnimsPtrTable[NUM_SPECIES + 1]   = {NULL};
const union AnimCmd *const *const gTrainerFrontAnimsPtrTable[256]            = {NULL};
const union AnimCmd *const *const gTrainerBackAnimsPtrTable[256]             = {NULL};

/* Castform sprite coordinates */
const struct MonCoords gCastformFrontSpriteCoords[4] = {{0}};

/* Trainer back pic table (CompressedSpriteSheet, functionally unused) */
const struct CompressedSpriteSheet gTrainerBackPicTable[1] = {{{NULL}, 0, 0}};

/* ===========================================================================
 * Pokémon/trainer name tables
 * =========================================================================== */

/* EOS = 0xFF in GF string encoding */
#define GF_EOS 0xFF

/* gSpeciesNames is in battle_desktop/stubs/text_data.c */
const u8 gTrainerClassNames[256][13] = {
    [0 ... 255] = {GF_EOS},
    [1] = {'C','H','A','M','P','I','O','N', GF_EOS},
};
/* gTrainers: trainerName must be GF-EOS (0xFF) terminated */
const struct Trainer gTrainers[2] = {
    [0] = { .trainerName = {GF_EOS} },
    [1] = { .trainerClass = 1, .trainerName = {'S','T','E','V','E','N', GF_EOS} },
};
const struct PokedexEntry gPokedexEntries[NUM_SPECIES + 1] = {{0}};

/* ===========================================================================
 * Battle background / window templates
 * =========================================================================== */

const struct BgTemplate gBattleBgTemplates[1] = {{0}};
static const struct WindowTemplate sDummyWindowTemplates[] = {DUMMY_WIN_TEMPLATE};
const struct WindowTemplate *const gBattleWindowTemplates[1] = {sDummyWindowTemplates};

/* Windows array for window system */
struct Window gWindows[64] = {{0}};

/* Map header stub */
struct MapHeader gMapHeader = {0};

/* ===========================================================================
 * Text string stubs
 * =========================================================================== */

const u8 gText_LinkStandby3[]                     = {0xFF};
const u8 gText_PkmnTransferredSomeonesPC[]         = {0xFF};
const u8 gText_PkmnTransferredSomeonesPCBoxFull[]  = {0xFF};
const u8 gText_PkmnTransferredLanettesPC[]         = {0xFF};
const u8 gText_PkmnTransferredLanettesPCBoxFull[]  = {0xFF};
const u8 BattleFrontier_BattleTowerBattleRoom_Text_RecordCouldntBeSaved[] = {0xFF};

/* ===========================================================================
 * Function stubs — battle visual/audio (battle_gfx_sfx_util.c)
 * =========================================================================== */

void HandleLowHpMusicChange(struct Pokemon *mon, u8 battler) {}
void BattleStopLowHpSound(void) {}
void HandleBattleLowHpMusicChange(void) {}
void ClearTemporarySpeciesSpriteData(u8 battler, bool8 dontClearSubstitute) {}
void AllocateMonSpritesGfx(void) {}
void FreeMonSpritesGfx(void) {}
u8 GetScaledHPFraction(s16 hp, s16 maxhp, u8 scale) { return 0; }
void StartHealthboxSlideIn(u8 battler) {}
bool8 BattleInitAllSprites(u8 *state1, u8 *battler) { return TRUE; }

/* ===========================================================================
 * Function stubs — battle background (battle_bg.c)
 * =========================================================================== */

void InitBattleBgsVideo(void) {}
void LoadBattleMenuWindowGfx(void) {}
void LoadBattleTextboxAndBackground(void) {}
void DrawBattleEntryBackground(void) {}
bool8 LoadChosenBattleElement(u8 caseId) { return FALSE; }
void FillAroundBattleWindows(void) {}
void InitLinkBattleVsScreen(u8 taskId) {}

/* ===========================================================================
 * Function stubs — link / RFU (link.c, link_rfu.c)
 * =========================================================================== */

void SetLinkStandbyCallback(void) {}
void SetCloseLinkCallback(void) {}
void LoadWirelessStatusIndicatorSpriteGfx(void) {}
void CreateWirelessStatusIndicatorSprite(u8 x, u8 y) {}
u8 GetLinkPlayerCount_2(void) { return 1; }
void DestroyTask_RfuIdle(void) {}

/* ===========================================================================
 * Function stubs — recorded battle (recorded_battle.c)
 * =========================================================================== */

void RecordedBattle_SetBattlerAction(u8 battler, u8 action) {}
void RecordedBattle_ClearBattlerAction(u8 battler, u8 numActions) {}
void RecordedBattle_CopyBattlerMoves(void) {}
void RecordedBattle_CheckMovesetChanges(u8 case_) {}
u32 RecordedBattle_GetFrontierPassFlag(void) { return 0; }
void RecordedBattle_SetPlaybackFinished(void) {}
bool32 MoveRecordedBattleToSaveData(void) { return FALSE; }
u32 GetAiScriptsInRecordedBattle(void) { return 0; }
u8 GetTextSpeedInRecordedBattle(void) { return 0; }

/* ===========================================================================
 * Function stubs — sound / music (sound.c)
 * =========================================================================== */

void ResetMapMusic(void) {}
void FadeOutMapMusic(u8 speed) {}
void PlayCry_Normal(u16 species, s8 pan) {}
void PlayCry_NormalNoDucking(u16 species, s8 pan, s8 volume, u8 priority) {}
bool8 IsCryFinished(void) { return TRUE; }
void StopCryAndClearCrySongs(void) {}
void m4aMPlayAllStop(void) {}

/* ===========================================================================
 * Function stubs — palette (palette.c)
 * =========================================================================== */

void ResetPaletteFade(void) { gPaletteFade.active = FALSE; }
void BeginFastPaletteFade(u8 submode) {}

/* ===========================================================================
 * Function stubs — party menu (party_menu.c)
 * =========================================================================== */

bool8 IsMultiBattle(void) { return FALSE; }
void BufferBattlePartyCurrentOrderBySide(u8 battler, u8 flankId) {}
void SwitchPartyOrderLinkMulti(u8 battler, u8 slot, u8 slot2) {}
void SwitchPartyMonSlots(u8 slot, u8 slot2)
{
    /* gBattlePartyCurrentOrder packs two 4-bit party IDs per byte.
     * Slot n: high nibble if (n & 1) == 0, low nibble if (n & 1) == 1. */
    u8 id1 = (slot  & 1) ? (gBattlePartyCurrentOrder[slot  / 2] & 0x0F)
                         : (gBattlePartyCurrentOrder[slot  / 2] >> 4);
    u8 id2 = (slot2 & 1) ? (gBattlePartyCurrentOrder[slot2 / 2] & 0x0F)
                         : (gBattlePartyCurrentOrder[slot2 / 2] >> 4);
    /* Write id2 into slot's position */
    if (slot & 1)
        gBattlePartyCurrentOrder[slot  / 2] = (gBattlePartyCurrentOrder[slot  / 2] & 0xF0) | id2;
    else
        gBattlePartyCurrentOrder[slot  / 2] = (gBattlePartyCurrentOrder[slot  / 2] & 0x0F) | (id2 << 4);
    /* Write id1 into slot2's position */
    if (slot2 & 1)
        gBattlePartyCurrentOrder[slot2 / 2] = (gBattlePartyCurrentOrder[slot2 / 2] & 0xF0) | id1;
    else
        gBattlePartyCurrentOrder[slot2 / 2] = (gBattlePartyCurrentOrder[slot2 / 2] & 0x0F) | (id1 << 4);
}
u8 GetPartyIdFromBattlePartyId(u8 battlePartyId) { return battlePartyId; }
void ShowPartyMenuToShowcaseMultiBattleParty(void) {}
u8 *GetMonNickname(struct Pokemon *mon, u8 *dest) { if (dest) dest[0] = 0xFF; return dest; }

/* ===========================================================================
 * Function stubs — evolution / pokedex
 * =========================================================================== */

void BeginEvolutionScene(struct Pokemon *mon, u16 postEvoSpecies, bool8 canStopEvo, u8 partyId) {}
u8 DisplayCaughtMonDexPage(u16 dexNum, u32 otId, u32 personality) { return 0; }
u16 GetPokedexHeightWeight(u16 dexNum, u8 data) { return 0; }
bool32 IsNationalPokedexEnabled(void) { return FALSE; }

/* ===========================================================================
 * Function stubs — overworld / map (overworld.c)
 * =========================================================================== */

u8 GetCurrentMapType(void) { return 0; }
mapsec_u8_t GetCurrentRegionMapSectionId(void) { return 0; }

/* ===========================================================================
 * Function stubs — TV / news (tv.c)
 * =========================================================================== */

void TryPutPokemonTodayOnAir(void) {}
void TryPutBreakingNewsOnAir(void) {}

/* ===========================================================================
 * Function stubs — roamer (roamer.c)
 * =========================================================================== */

void UpdateRoamerHPStatus(struct Pokemon *mon) {}
void SetRoamerInactive(void) {}

/* ===========================================================================
 * Function stubs — frontier (frontier_util.c)
 * =========================================================================== */

u8 GetFrontierBrainTrainerClass(void) { return 0; }
void CopyFrontierBrainTrainerName(u8 *dst) { if (dst) dst[0] = 0xFF; }
void CopyFrontierTrainerText(u8 whichText, u16 trainerId) {}

/* ===========================================================================
 * Function stubs — trainer hill (trainer_hill.c)
 * =========================================================================== */

bool8 InTrainerHillChallenge(void) { return FALSE; }
u8 GetTrainerHillOpponentClass(u16 trainerId) { return 0; }
void GetTrainerHillTrainerName(u8 *dst, u16 trainerId) { if (dst) dst[0] = 0xFF; }
void InitTrainerHillBattleStruct(void) {}
void FreeTrainerHillBattleStruct(void) {}
void CopyTrainerHillTrainerText(u8 which, u16 trainerId) {}
u8 GetTrainerEncounterMusicIdInTrainerHill(u16 trainerId) { return 0; }

/* ===========================================================================
 * Function stubs — pokemon storage (pokemon_storage_system.c)
 * =========================================================================== */

u8 StorageGetCurrentBox(void) { return 0; }
u32 GetBoxMonDataAt(u8 boxId, u8 boxPosition, s32 request) { return 0; }
u8 *GetBoxNamePtr(u8 boxId) { return NULL; }
bool8 ShouldShowBoxWasFullMessage(void) { return FALSE; }
void SetPCBoxToSendMon(u8 boxId) {}

/* ===========================================================================
 * Function stubs — window / BG (window.c, bg.c)
 * =========================================================================== */

void CopyToWindowPixelBuffer(u8 windowId, const void *src, u16 size, u16 tileOffset) {}
void CopyToBgTilemapBufferRect_ChangePalette(u8 bg, const void *src, u8 destX, u8 destY,
                                              u8 rectWidth, u8 rectHeight, u8 palette) {}
int ConvertPixelWidthToTileWidth(int width) { return (width + 7) / 8; }
/* DoBgAffineSet defined in util.c (compiled) */

/* ===========================================================================
 * Function stubs — misc
 * =========================================================================== */

void DrawLevelUpWindowPg1(u16 windowId, u16 *statsBefore, u16 *statsAfter,
                          u8 bgClr, u8 fgClr, u8 shadowClr) {}
void DrawLevelUpWindowPg2(u16 windowId, u16 *currStats,
                          u8 bgClr, u8 fgClr, u8 shadowClr) {}
void GetMonLevelUpWindowStats(struct Pokemon *mon, u16 *currStats) {}
u8 GetPlayerTextSpeedDelay(void) { return 1; }

const u8 *GetApprenticeNameInLanguage(u32 apprenticeId, s32 language) { return NULL; }

u8 GetItemHoldEffect(u16 itemId) { return 0; }
u8 GetItemHoldEffectParam(u16 itemId) { return 0; }
void CopyItemName(u16 itemId, u8 *dst) { if (dst) dst[0] = 0xFF; }

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s16 numCalcs) {}

u8 RecordedBattle_BufferNewBattlerData(u8 *dst) { return 0; }
