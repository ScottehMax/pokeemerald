#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_gfx_sfx_util.h"
#include "battle_overworld_scene.h"
#include "battle_util2.h"
#include "bg.h"
#include "data.h"
#include "debug_menu.h"
#include "event_object_movement.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "pokemon.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "strings.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/battle.h"
#include "constants/event_objects.h"
#include "constants/layouts.h"
#include "constants/moves.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/species.h"
#include "data/ow_battle_preview_layout_names.h"

extern const struct MapLayout *const gMapLayouts[];

#define OW_BATTLE_PREVIEW_LAYOUT_COUNT LAYOUT_SOOTOPOLIS_CITY_MYSTERY_EVENTS_HOUSE_1F_STAIRS_UNBLOCKED
#define OW_BATTLE_PREVIEW_DEFAULT_LAYOUT LAYOUT_ROUTE101
#define OW_BATTLE_PREVIEW_MAP_WIDTH 15
#define OW_BATTLE_PREVIEW_MAP_HEIGHT 10
#define OW_BATTLE_PREVIEW_BASE_Y 56
#define OW_BATTLE_DEBUG_FONT FONT_SMALL_NARROW
#define OW_BATTLE_DEBUG_PALETTE 1
#define OW_BATTLE_DEBUG_ROW_HEIGHT 9
#define OW_BATTLE_ANIM_BASE_Y OW_BATTLE_PREVIEW_BASE_Y

enum
{
    WIN_DEBUG_MENU,
    WIN_DEBUG_PREVIEW
};

static void CB2_DebugMenu(void);
static void VBlankCB_DebugMenu(void);
static void Task_DebugMenuInput(u8 taskId);
static void Task_OwBattlePreviewInput(u8 taskId);
static void Task_OwBattleAnimInput(u8 taskId);
static void InitDebugMenuBgsAndWindows(void);
static void InitOwBattlePreview(void);
static void InitOwBattleAnimPreview(void);
static void DrawOwBattlePreviewText(void);
static void DrawOwBattleAnimText(void);
static void PrintOwBattleDebugText(const u8 *str, u8 x, u8 y);
static void RefreshOwBattlePreview(void);
static void ClampOwBattlePreviewOffset(void);
static const struct MapLayout *GetOwBattlePreviewLayout(void);
static const u8 *GetOwBattlePreviewLayoutName(u16 layoutId);
static void ResetOwBattlePreviewOffset(void);
static void CreateOwBattlePreviewSprites(void);
static void SetupOwBattleAnimBattleState(void);
static void FreeOwBattleAnimBattleState(void);
static void RefreshOwBattleAnimScene(void);
static void CreateOwBattleAnimSprites(void);
static void UpdateOwBattleAnimMons(void);
static void CopyDebugMonToBattleMon(struct BattlePokemon *dst, struct Pokemon *src);
static void ChangeOwBattleAnimSelection(s16 delta);
static void PlayOwBattleAnimMove(void);
static void RunOwBattleAnimScript(void);

static EWRAM_DATA u16 sOwBattlePreviewLayoutId = 0;
static EWRAM_DATA u16 sOwBattlePreviewX = 0;
static EWRAM_DATA u16 sOwBattlePreviewY = 0;
static EWRAM_DATA bool8 sOwBattlePreviewInitialized = FALSE;
static EWRAM_DATA bool8 sOwBattlePreviewHelpVisible = FALSE;
static EWRAM_DATA u16 sOwBattleAnimPlayerSpecies = 0;
static EWRAM_DATA u16 sOwBattleAnimOpponentSpecies = 0;
static EWRAM_DATA u16 sOwBattleAnimPlayerTrainerGfx = 0;
static EWRAM_DATA u16 sOwBattleAnimOpponentTrainerGfx = 0;
static EWRAM_DATA u16 sOwBattleAnimMove = 0;
static EWRAM_DATA u8 sOwBattleAnimAttacker = 0;
static EWRAM_DATA u8 sOwBattleAnimCursor = 0;
static EWRAM_DATA bool8 sOwBattleAnimHelpVisible = FALSE;
static EWRAM_DATA bool8 sOwBattleAnimInitialized = FALSE;
static EWRAM_DATA bool8 sOwBattleAnimWasActive = FALSE;
static EWRAM_DATA bool8 sOwBattleAnimOldInBattle = FALSE;
static EWRAM_DATA u32 sOwBattleAnimOldBattleTypeFlags = 0;
static EWRAM_DATA struct Pokemon sOwBattleAnimPlayerPartyBackup;
static EWRAM_DATA struct Pokemon sOwBattleAnimEnemyPartyBackup;

static const u8 sText_Debug[] = _("DEBUG");
static const u8 sText_OwBattlePreview[] = _("OW BATTLE PREVIEW");
static const u8 sText_OwBattleAnim[] = _("OW ANIM CHECK");
static const u8 sText_Cancel[] = _("CANCEL");
static const u8 sText_Id[] = _("ID");
static const u8 sText_X[] = _("X");
static const u8 sText_Y[] = _("Y");
static const u8 sText_Size[] = _("SIZE");
static const u8 sText_Slash[] = _("/");
static const u8 sText_Times[] = _("x");
static const u8 sText_PreviewHelp1[] = _("D-PAD OFFSET  L/R MAP");
static const u8 sText_PreviewHelp2[] = _("SEL HELP  B BACK");
static const u8 sText_PlayerMon[] = _("P MON");
static const u8 sText_OpponentMon[] = _("O MON");
static const u8 sText_PlayerTrainer[] = _("P TRN");
static const u8 sText_OpponentTrainer[] = _("O TRN");
static const u8 sText_Move[] = _("MOVE");
static const u8 sText_Side[] = _("SIDE");
static const u8 sText_Player[] = _("PLAYER");
static const u8 sText_Opponent[] = _("OPPONENT");
static const u8 sText_AnimHelp1[] = _("UP/DN ROW  L/R VALUE");
static const u8 sText_AnimHelp2[] = _("A PLAY  SEL HELP  B BACK");
static const u8 sTextColor_TransparentBg[] = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GRAY};

static const struct MenuAction sDebugMenuActions[] =
{
    {sText_OwBattlePreview, {NULL}},
    {sText_OwBattleAnim, {NULL}},
    {sText_Cancel, {NULL}},
};

static const struct BgTemplate sDebugMenuBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 24,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 28,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 27,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0
    },
    {
        .bg = 3,
        .charBaseIndex = 3,
        .mapBaseIndex = 26,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0
    }
};

static const struct WindowTemplate sDebugMenuWindowTemplates[] =
{
    [WIN_DEBUG_MENU] = {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 2,
        .width = 26,
        .height = 8,
        .paletteNum = OW_BATTLE_DEBUG_PALETTE,
        .baseBlock = 1
    },
    [WIN_DEBUG_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 10,
        .paletteNum = OW_BATTLE_DEBUG_PALETTE,
        .baseBlock = 1
    },
    DUMMY_WIN_TEMPLATE
};

static void CB2_DebugMenu(void)
{
    RunTasks();
    BattleOverworldScene_KeepBaseBackgroundVisible();
    BattleOverworldScene_UpdateBackgroundAnimation();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_DebugMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    BattleOverworldScene_TransferBackgroundAnimation();
    ScanlineEffect_InitHBlankDmaTransfer();
    SetGpuReg(REG_OFFSET_BG0HOFS, gBattle_BG0_X);
    SetGpuReg(REG_OFFSET_BG0VOFS, gBattle_BG0_Y);
    SetGpuReg(REG_OFFSET_BG1HOFS, gBattle_BG1_X);
    SetGpuReg(REG_OFFSET_BG1VOFS, gBattle_BG1_Y);
    SetGpuReg(REG_OFFSET_BG2HOFS, gBattle_BG2_X);
    SetGpuReg(REG_OFFSET_BG2VOFS, gBattle_BG2_Y);
    SetGpuReg(REG_OFFSET_BG3HOFS, gBattle_BG3_X);
    SetGpuReg(REG_OFFSET_BG3VOFS, gBattle_BG3_Y);
    SetGpuReg(REG_OFFSET_WIN0H, gBattle_WIN0H);
    SetGpuReg(REG_OFFSET_WIN0V, gBattle_WIN0V);
    SetGpuReg(REG_OFFSET_WIN1H, gBattle_WIN1H);
    SetGpuReg(REG_OFFSET_WIN1V, gBattle_WIN1V);
}

void CB2_InitDebugMenu(void)
{
    SetVBlankCallback(NULL);
    InitDebugMenuBgsAndWindows();
    DrawStdWindowFrame(WIN_DEBUG_MENU, FALSE);
    AddTextPrinterParameterized(WIN_DEBUG_MENU, FONT_NORMAL, sText_Debug, 8, 1, TEXT_SKIP_DRAW, NULL);
    PrintMenuTable(WIN_DEBUG_MENU, ARRAY_COUNT(sDebugMenuActions), sDebugMenuActions);
    InitMenuInUpperLeftCornerNormal(WIN_DEBUG_MENU, ARRAY_COUNT(sDebugMenuActions), 0);
    CopyWindowToVram(WIN_DEBUG_MENU, COPYWIN_FULL);
    CreateTask(Task_DebugMenuInput, 0);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_DebugMenu);
    SetMainCallback2(CB2_DebugMenu);
}

static void InitDebugMenuBgsAndWindows(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sDebugMenuBgTemplates, ARRAY_COUNT(sDebugMenuBgTemplates));
    DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
    DmaClear32(3, OAM, OAM_SIZE);
    DmaClear16(3, PLTT, PLTT_SIZE);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    ResetPaletteFade();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ScanlineEffect_Stop();
    InitWindows(sDebugMenuWindowTemplates);
    DeactivateAllTextPrinters();
    LoadMessageBoxAndBorderGfx();
    Menu_LoadStdPalAt(BG_PLTT_ID(OW_BATTLE_DEBUG_PALETTE));
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    HideBg(3);
}

static void Task_DebugMenuInput(u8 taskId)
{
    s8 input;

    if (gPaletteFade.active)
        return;

    input = Menu_ProcessInputNoWrap();
    switch (input)
    {
    case 0:
        PlaySE(SE_SELECT);
        DestroyTask(taskId);
        InitOwBattlePreview();
        break;
    case 1:
        PlaySE(SE_SELECT);
        DestroyTask(taskId);
        InitOwBattleAnimPreview();
        break;
    case 2:
    case MENU_B_PRESSED:
        PlaySE(SE_SELECT);
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
        break;
    }
}

static void InitOwBattlePreview(void)
{
    FillBgTilemapBufferRect(0, 0, 0, 0, 32, 32, 0);
    ClearStdWindowAndFrame(WIN_DEBUG_MENU, FALSE);
    HideBg(0);
    ShowBg(3);
    sOwBattlePreviewInitialized = FALSE;
    sOwBattlePreviewHelpVisible = TRUE;
    sOwBattlePreviewLayoutId = OW_BATTLE_PREVIEW_DEFAULT_LAYOUT;
    ResetOwBattlePreviewOffset();
    RefreshOwBattlePreview();
    CreateOwBattlePreviewSprites();
    PutWindowTilemap(WIN_DEBUG_PREVIEW);
    ShowBg(0);
    CopyBgTilemapBufferToVram(0);
    CreateTask(Task_OwBattlePreviewInput, 0);
}

static void Task_OwBattlePreviewInput(u8 taskId)
{
    bool8 changed = FALSE;

    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DestroyTask(taskId);
        BattleOverworldScene_StopBackgroundAnimation();
        FreeAllWindowBuffers();
        gMain.state = 0;
        SetMainCallback2(CB2_InitDebugMenu);
        return;
    }
    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        sOwBattlePreviewHelpVisible ^= TRUE;
        DrawOwBattlePreviewText();
    }
    if (JOY_NEW(L_BUTTON))
    {
        if (sOwBattlePreviewLayoutId <= 1)
            sOwBattlePreviewLayoutId = OW_BATTLE_PREVIEW_LAYOUT_COUNT;
        else
            sOwBattlePreviewLayoutId--;
        ResetOwBattlePreviewOffset();
        changed = TRUE;
    }
    if (JOY_NEW(R_BUTTON))
    {
        sOwBattlePreviewLayoutId++;
        if (sOwBattlePreviewLayoutId > OW_BATTLE_PREVIEW_LAYOUT_COUNT)
            sOwBattlePreviewLayoutId = 1;
        ResetOwBattlePreviewOffset();
        changed = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT) && sOwBattlePreviewX != 0)
    {
        sOwBattlePreviewX--;
        changed = TRUE;
    }
    if (JOY_NEW(DPAD_RIGHT))
    {
        sOwBattlePreviewX++;
        changed = TRUE;
    }
    if (JOY_NEW(DPAD_UP) && sOwBattlePreviewY != 0)
    {
        sOwBattlePreviewY--;
        changed = TRUE;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        sOwBattlePreviewY++;
        changed = TRUE;
    }

    if (changed)
        RefreshOwBattlePreview();
}

static void RefreshOwBattlePreview(void)
{
    ClampOwBattlePreviewOffset();
    BattleOverworldScene_LoadDebugBackground(GetOwBattlePreviewLayout(), sOwBattlePreviewX, sOwBattlePreviewY);
    DrawOwBattlePreviewText();
    sOwBattlePreviewInitialized = TRUE;
}

static void ClampOwBattlePreviewOffset(void)
{
    const struct MapLayout *layout = GetOwBattlePreviewLayout();

    if (layout->width <= OW_BATTLE_PREVIEW_MAP_WIDTH)
        sOwBattlePreviewX = 0;
    else if (sOwBattlePreviewX > layout->width - OW_BATTLE_PREVIEW_MAP_WIDTH)
        sOwBattlePreviewX = layout->width - OW_BATTLE_PREVIEW_MAP_WIDTH;

    if (layout->height <= OW_BATTLE_PREVIEW_MAP_HEIGHT)
        sOwBattlePreviewY = 0;
    else if (sOwBattlePreviewY > layout->height - OW_BATTLE_PREVIEW_MAP_HEIGHT)
        sOwBattlePreviewY = layout->height - OW_BATTLE_PREVIEW_MAP_HEIGHT;
}

static void DrawOwBattlePreviewText(void)
{
    const struct MapLayout *layout = GetOwBattlePreviewLayout();
    u8 text[4];
    u8 y;

    if (!sOwBattlePreviewHelpVisible)
    {
        FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(0));
        ClearWindowTilemap(WIN_DEBUG_PREVIEW);
        CopyWindowToVram(WIN_DEBUG_PREVIEW, COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        return;
    }

    PutWindowTilemap(WIN_DEBUG_PREVIEW);
    FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(0));

    y = 0;
    PrintOwBattleDebugText(sText_Id, 0, y);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewLayoutId, STR_CONV_MODE_LEADING_ZEROS, 3);
    PrintOwBattleDebugText(text, 16, y);
    PrintOwBattleDebugText(sText_Slash, 40, y);
    ConvertIntToDecimalStringN(text, OW_BATTLE_PREVIEW_LAYOUT_COUNT, STR_CONV_MODE_LEADING_ZEROS, 3);
    PrintOwBattleDebugText(text, 48, y);
    PrintOwBattleDebugText(GetOwBattlePreviewLayoutName(sOwBattlePreviewLayoutId), 80, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sText_X, 0, y);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewX, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 16, y);
    PrintOwBattleDebugText(sText_Y, 48, y);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewY, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 64, y);
    PrintOwBattleDebugText(sText_Size, 96, y);
    ConvertIntToDecimalStringN(text, layout->width, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 136, y);
    PrintOwBattleDebugText(sText_Times, 160, y);
    ConvertIntToDecimalStringN(text, layout->height, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 168, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sText_PreviewHelp1, 0, y);
    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sText_PreviewHelp2, 0, y);
    CopyWindowToVram(WIN_DEBUG_PREVIEW, sOwBattlePreviewInitialized ? COPYWIN_GFX : COPYWIN_FULL);
    CopyBgTilemapBufferToVram(0);
}

static const struct MapLayout *GetOwBattlePreviewLayout(void)
{
    return gMapLayouts[sOwBattlePreviewLayoutId - 1];
}

static const u8 *GetOwBattlePreviewLayoutName(u16 layoutId)
{
    return sOwBattlePreviewLayoutNames[layoutId - 1];
}

static void ResetOwBattlePreviewOffset(void)
{
    if (!BattleOverworldScene_GetBackgroundOffsetForLayout(sOwBattlePreviewLayoutId, &sOwBattlePreviewX, &sOwBattlePreviewY))
    {
        sOwBattlePreviewX = 0;
        sOwBattlePreviewY = 0;
    }
}

static void CreateOwBattlePreviewSprites(void)
{
    u8 spriteId;

    spriteId = CreateObjectGraphicsSprite(OBJ_EVENT_GFX_BRENDAN_NORMAL, SpriteCallbackDummy, 36, OW_BATTLE_PREVIEW_BASE_Y, 1);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].coordOffsetEnabled = FALSE;
        StartSpriteAnim(&gSprites[spriteId], GetFaceDirectionAnimNum(DIR_EAST));
    }

    spriteId = CreateObjectGraphicsSprite(OBJ_EVENT_GFX_YOUNGSTER, SpriteCallbackDummy, 212, OW_BATTLE_PREVIEW_BASE_Y, 0);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].coordOffsetEnabled = FALSE;
        StartSpriteAnim(&gSprites[spriteId], GetFaceDirectionAnimNum(DIR_WEST));
    }

    BattleOverworldScene_CreatePreviewMonSprite(SPECIES_PIKACHU, B_POSITION_PLAYER_LEFT, 76, OW_BATTLE_PREVIEW_BASE_Y, 1);
    BattleOverworldScene_CreatePreviewMonSprite(SPECIES_DUSKULL, B_POSITION_OPPONENT_LEFT, 164, OW_BATTLE_PREVIEW_BASE_Y, 1);
}

enum
{
    OW_ANIM_ROW_PLAYER_MON,
    OW_ANIM_ROW_OPPONENT_MON,
    OW_ANIM_ROW_PLAYER_TRAINER,
    OW_ANIM_ROW_OPPONENT_TRAINER,
    OW_ANIM_ROW_MOVE,
    OW_ANIM_ROW_SIDE,
    OW_ANIM_ROW_COUNT
};

static void InitOwBattleAnimPreview(void)
{
    FillBgTilemapBufferRect(0, 0, 0, 0, 32, 32, 0);
    ClearStdWindowAndFrame(WIN_DEBUG_MENU, FALSE);
    HideBg(0);
    ShowBg(2);
    ShowBg(3);

    sOwBattleAnimPlayerSpecies = SPECIES_SUICUNE;
    sOwBattleAnimOpponentSpecies = SPECIES_PORYGON2;
    sOwBattleAnimPlayerTrainerGfx = OBJ_EVENT_GFX_BRENDAN_NORMAL;
    sOwBattleAnimOpponentTrainerGfx = OBJ_EVENT_GFX_YOUNGSTER;
    sOwBattleAnimMove = MOVE_POUND;
    sOwBattleAnimAttacker = B_POSITION_PLAYER_LEFT;
    sOwBattleAnimCursor = 0;
    sOwBattleAnimHelpVisible = TRUE;
    sOwBattleAnimInitialized = FALSE;
    sOwBattleAnimWasActive = FALSE;
    sOwBattlePreviewLayoutId = OW_BATTLE_PREVIEW_DEFAULT_LAYOUT;
    ResetOwBattlePreviewOffset();

    SetupOwBattleAnimBattleState();
    RefreshOwBattleAnimScene();
    PutWindowTilemap(WIN_DEBUG_PREVIEW);
    ShowBg(0);
    CopyBgTilemapBufferToVram(0);
    CreateTask(Task_OwBattleAnimInput, 0);
}

static void Task_OwBattleAnimInput(u8 taskId)
{
    bool8 sceneChanged = FALSE;
    bool8 textChanged = FALSE;

    RunOwBattleAnimScript();
    if (gAnimScriptActive)
        return;

    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        DestroyTask(taskId);
        BattleOverworldScene_StopBackgroundAnimation();
        FreeOwBattleAnimBattleState();
        FreeAllWindowBuffers();
        gMain.state = 0;
        SetMainCallback2(CB2_InitDebugMenu);
        return;
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        PlayOwBattleAnimMove();
        DrawOwBattleAnimText();
        return;
    }
    if (JOY_NEW(SELECT_BUTTON))
    {
        PlaySE(SE_SELECT);
        sOwBattleAnimHelpVisible ^= TRUE;
        DrawOwBattleAnimText();
    }
    if (JOY_NEW(DPAD_UP))
    {
        if (sOwBattleAnimCursor == 0)
            sOwBattleAnimCursor = OW_ANIM_ROW_COUNT - 1;
        else
            sOwBattleAnimCursor--;
        textChanged = TRUE;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        sOwBattleAnimCursor++;
        if (sOwBattleAnimCursor >= OW_ANIM_ROW_COUNT)
            sOwBattleAnimCursor = 0;
        textChanged = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT) || JOY_NEW(L_BUTTON))
    {
        ChangeOwBattleAnimSelection(JOY_NEW(L_BUTTON) ? -10 : -1);
        if (sOwBattleAnimCursor <= OW_ANIM_ROW_OPPONENT_TRAINER)
            sceneChanged = TRUE;
        else
            textChanged = TRUE;
    }
    if (JOY_NEW(DPAD_RIGHT) || JOY_NEW(R_BUTTON))
    {
        ChangeOwBattleAnimSelection(JOY_NEW(R_BUTTON) ? 10 : 1);
        if (sOwBattleAnimCursor <= OW_ANIM_ROW_OPPONENT_TRAINER)
            sceneChanged = TRUE;
        else
            textChanged = TRUE;
    }

    if (sceneChanged)
        RefreshOwBattleAnimScene();
    else if (textChanged)
        DrawOwBattleAnimText();
}

static void SetupOwBattleAnimBattleState(void)
{
    u8 i;

    sOwBattleAnimOldInBattle = gMain.inBattle;
    sOwBattleAnimOldBattleTypeFlags = gBattleTypeFlags;
    sOwBattleAnimPlayerPartyBackup = gPlayerParty[0];
    sOwBattleAnimEnemyPartyBackup = gEnemyParty[0];

    gBattleTypeFlags = BATTLE_TYPE_TRAINER;
    gMain.inBattle = TRUE;
    AllocateBattleResources();
    AllocateBattleSpritesData();
    AllocateMonSpritesGfx();
    BattleOverworldScene_Reset();
    SetBgTilemapBuffer(1, gBattleAnimBgTilemapBuffer);
    SetBgTilemapBuffer(2, gBattleAnimBgTilemapBuffer);

    gBattlersCount = 2;
    gBattlerPositions[0] = B_POSITION_PLAYER_LEFT;
    gBattlerPositions[1] = B_POSITION_OPPONENT_LEFT;
    gBattlerPartyIndexes[0] = 0;
    gBattlerPartyIndexes[1] = 0;
    for (i = 0; i < MAX_BATTLERS_COUNT; i++)
    {
        gBattlerSpriteIds[i] = SPRITE_NONE;
        gHealthboxSpriteIds[i] = SPRITE_NONE;
        gBattleMonForms[i] = 0;
    }
    gBattle_WIN0H = 0;
    gBattle_WIN0V = 0;
    gBattle_WIN1H = 0;
    gBattle_WIN1V = 0;
}

static void FreeOwBattleAnimBattleState(void)
{
    if (gAnimScriptActive)
        ClearBattleAnimationVars();

    ResetSpriteData();
    FreeAllSpritePalettes();
    BattleOverworldScene_ResetSpriteReferences();
    FreeMonSpritesGfx();
    FreeBattleSpritesData();
    FreeBattleResources();

    gPlayerParty[0] = sOwBattleAnimPlayerPartyBackup;
    gEnemyParty[0] = sOwBattleAnimEnemyPartyBackup;
    gBattleTypeFlags = sOwBattleAnimOldBattleTypeFlags;
    gMain.inBattle = sOwBattleAnimOldInBattle;
}

static void RefreshOwBattleAnimScene(void)
{
    BattleOverworldScene_LoadDebugBackground(GetOwBattlePreviewLayout(), sOwBattlePreviewX, sOwBattlePreviewY);
    UpdateOwBattleAnimMons();
    CreateOwBattleAnimSprites();
    DrawOwBattleAnimText();
    sOwBattleAnimInitialized = TRUE;
}

static void UpdateOwBattleAnimMons(void)
{
    CreateMon(&gPlayerParty[0], sOwBattleAnimPlayerSpecies, 50, USE_RANDOM_IVS, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CreateMon(&gEnemyParty[0], sOwBattleAnimOpponentSpecies, 50, USE_RANDOM_IVS, FALSE, 0, OT_ID_PLAYER_ID, 0);
    CopyDebugMonToBattleMon(&gBattleMons[0], &gPlayerParty[0]);
    CopyDebugMonToBattleMon(&gBattleMons[1], &gEnemyParty[0]);
}

static void CopyDebugMonToBattleMon(struct BattlePokemon *dst, struct Pokemon *src)
{
    u8 i;
    u8 nickname[POKEMON_NAME_BUFFER_SIZE];

    CpuFill32(0, dst, sizeof(*dst));
    dst->species = GetMonData(src, MON_DATA_SPECIES);
    dst->item = GetMonData(src, MON_DATA_HELD_ITEM);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        dst->moves[i] = GetMonData(src, MON_DATA_MOVE1 + i);
        dst->pp[i] = GetMonData(src, MON_DATA_PP1 + i);
    }
    dst->ppBonuses = GetMonData(src, MON_DATA_PP_BONUSES);
    dst->friendship = GetMonData(src, MON_DATA_FRIENDSHIP);
    dst->experience = GetMonData(src, MON_DATA_EXP);
    dst->hpIV = GetMonData(src, MON_DATA_HP_IV);
    dst->attackIV = GetMonData(src, MON_DATA_ATK_IV);
    dst->defenseIV = GetMonData(src, MON_DATA_DEF_IV);
    dst->speedIV = GetMonData(src, MON_DATA_SPEED_IV);
    dst->spAttackIV = GetMonData(src, MON_DATA_SPATK_IV);
    dst->spDefenseIV = GetMonData(src, MON_DATA_SPDEF_IV);
    dst->personality = GetMonData(src, MON_DATA_PERSONALITY);
    dst->status1 = GetMonData(src, MON_DATA_STATUS);
    dst->level = GetMonData(src, MON_DATA_LEVEL);
    dst->hp = GetMonData(src, MON_DATA_HP);
    dst->maxHP = GetMonData(src, MON_DATA_MAX_HP);
    dst->attack = GetMonData(src, MON_DATA_ATK);
    dst->defense = GetMonData(src, MON_DATA_DEF);
    dst->speed = GetMonData(src, MON_DATA_SPEED);
    dst->spAttack = GetMonData(src, MON_DATA_SPATK);
    dst->spDefense = GetMonData(src, MON_DATA_SPDEF);
    dst->abilityNum = GetMonData(src, MON_DATA_ABILITY_NUM);
    dst->otId = GetMonData(src, MON_DATA_OT_ID);
    dst->types[0] = gSpeciesInfo[dst->species].types[0];
    dst->types[1] = gSpeciesInfo[dst->species].types[1];
    dst->ability = GetAbilityBySpecies(dst->species, dst->abilityNum);
    GetMonData(src, MON_DATA_NICKNAME, nickname);
    StringCopy_Nickname(dst->nickname, nickname);
    GetMonData(src, MON_DATA_OT_NAME, dst->otName);
    for (i = 0; i < NUM_BATTLE_STATS; i++)
        dst->statStages[i] = DEFAULT_STAT_STAGE;
}

static void CreateOwBattleAnimSprites(void)
{
    u8 spriteId;

    ResetSpriteData();
    FreeAllSpritePalettes();
    BattleOverworldScene_ResetSpriteReferences();
    gReservedSpritePaletteCount = MAX_BATTLERS_COUNT;

    spriteId = CreateObjectGraphicsSprite(sOwBattleAnimPlayerTrainerGfx, SpriteCallbackDummy, 36, OW_BATTLE_ANIM_BASE_Y, 1);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].coordOffsetEnabled = FALSE;
        StartSpriteAnim(&gSprites[spriteId], GetFaceDirectionAnimNum(DIR_EAST));
    }

    spriteId = CreateObjectGraphicsSprite(sOwBattleAnimOpponentTrainerGfx, SpriteCallbackDummy, 212, OW_BATTLE_ANIM_BASE_Y, 0);
    if (spriteId != MAX_SPRITES)
    {
        gSprites[spriteId].coordOffsetEnabled = FALSE;
        StartSpriteAnim(&gSprites[spriteId], GetFaceDirectionAnimNum(DIR_WEST));
    }

    BattleOverworldScene_CreateBattlerSprite(0);
    BattleOverworldScene_CreateBattlerSprite(1);
}

static void DrawOwBattleAnimText(void)
{
    u8 text[8];
    u8 y;

    if (!sOwBattleAnimHelpVisible)
    {
        FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(0));
        ClearWindowTilemap(WIN_DEBUG_PREVIEW);
        CopyWindowToVram(WIN_DEBUG_PREVIEW, COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        return;
    }

    PutWindowTilemap(WIN_DEBUG_PREVIEW);
    FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(0));

    y = 0;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_PLAYER_MON ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_PlayerMon, 8, y);
    ConvertIntToDecimalStringN(text, sOwBattleAnimPlayerSpecies, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 56, y);
    PrintOwBattleDebugText(gSpeciesNames[sOwBattleAnimPlayerSpecies], 88, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_OPPONENT_MON ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_OpponentMon, 8, y);
    ConvertIntToDecimalStringN(text, sOwBattleAnimOpponentSpecies, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 56, y);
    PrintOwBattleDebugText(gSpeciesNames[sOwBattleAnimOpponentSpecies], 88, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_PLAYER_TRAINER ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_PlayerTrainer, 8, y);
    ConvertIntToDecimalStringN(text, sOwBattleAnimPlayerTrainerGfx, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 64, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_OPPONENT_TRAINER ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_OpponentTrainer, 8, y);
    ConvertIntToDecimalStringN(text, sOwBattleAnimOpponentTrainerGfx, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 64, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_MOVE ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_Move, 8, y);
    ConvertIntToDecimalStringN(text, sOwBattleAnimMove, STR_CONV_MODE_LEFT_ALIGN, 3);
    PrintOwBattleDebugText(text, 56, y);
    PrintOwBattleDebugText(gMoveNames[sOwBattleAnimMove], 88, y);

    y += OW_BATTLE_DEBUG_ROW_HEIGHT;
    PrintOwBattleDebugText(sOwBattleAnimCursor == OW_ANIM_ROW_SIDE ? gText_SelectorArrow2 : gText_Space, 0, y);
    PrintOwBattleDebugText(sText_Side, 8, y);
    PrintOwBattleDebugText(sOwBattleAnimAttacker == B_POSITION_PLAYER_LEFT ? sText_Player : sText_Opponent, 56, y);

    PrintOwBattleDebugText(sText_AnimHelp1, 128, 32);
    PrintOwBattleDebugText(sText_AnimHelp2, 128, 44);
    CopyWindowToVram(WIN_DEBUG_PREVIEW, sOwBattleAnimInitialized ? COPYWIN_GFX : COPYWIN_FULL);
    CopyBgTilemapBufferToVram(0);
}

static void PrintOwBattleDebugText(const u8 *str, u8 x, u8 y)
{
    AddTextPrinterParameterized4(WIN_DEBUG_PREVIEW, OW_BATTLE_DEBUG_FONT, x, y, 0, 0, sTextColor_TransparentBg, TEXT_SKIP_DRAW, str);
}

static void ChangeOwBattleAnimSelection(s16 delta)
{
    switch (sOwBattleAnimCursor)
    {
    case OW_ANIM_ROW_PLAYER_MON:
        sOwBattleAnimPlayerSpecies = ((sOwBattleAnimPlayerSpecies - 1 + NUM_SPECIES - 1 + delta) % (NUM_SPECIES - 1)) + 1;
        break;
    case OW_ANIM_ROW_OPPONENT_MON:
        sOwBattleAnimOpponentSpecies = ((sOwBattleAnimOpponentSpecies - 1 + NUM_SPECIES - 1 + delta) % (NUM_SPECIES - 1)) + 1;
        break;
    case OW_ANIM_ROW_PLAYER_TRAINER:
        sOwBattleAnimPlayerTrainerGfx = (sOwBattleAnimPlayerTrainerGfx + NUM_OBJ_EVENT_GFX + delta) % NUM_OBJ_EVENT_GFX;
        break;
    case OW_ANIM_ROW_OPPONENT_TRAINER:
        sOwBattleAnimOpponentTrainerGfx = (sOwBattleAnimOpponentTrainerGfx + NUM_OBJ_EVENT_GFX + delta) % NUM_OBJ_EVENT_GFX;
        break;
    case OW_ANIM_ROW_MOVE:
        sOwBattleAnimMove = ((sOwBattleAnimMove - 1 + MOVES_COUNT - 1 + delta) % (MOVES_COUNT - 1)) + 1;
        break;
    case OW_ANIM_ROW_SIDE:
        sOwBattleAnimAttacker ^= 1;
        break;
    }
}

static void PlayOwBattleAnimMove(void)
{
    u8 attacker = sOwBattleAnimAttacker == B_POSITION_PLAYER_LEFT ? 0 : 1;
    u8 target = attacker ^ 1;

    ClearBattleAnimationVars();
    gActiveBattler = attacker;
    gBattlerAttacker = attacker;
    gBattlerTarget = target;
    gCurrentMove = sOwBattleAnimMove;
    gChosenMove = sOwBattleAnimMove;
    gChosenMoveByBattler[attacker] = sOwBattleAnimMove;
    gAnimMovePower = gBattleMoves[sOwBattleAnimMove].power;
    gAnimMoveDmg = 50;
    gAnimMoveTurn = 0;
    DoMoveAnim(sOwBattleAnimMove);
    sOwBattleAnimWasActive = TRUE;
}

static void RunOwBattleAnimScript(void)
{
    if (gAnimScriptActive)
        gAnimScriptCallback();
    if (sOwBattleAnimWasActive && !gAnimScriptActive)
    {
        BattleOverworldScene_RestoreBattlerSpriteAnim(0);
        BattleOverworldScene_RestoreBattlerSpriteAnim(1);
        sOwBattleAnimWasActive = FALSE;
    }
}
