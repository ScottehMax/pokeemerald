#include "global.h"
#include "battle_overworld_scene.h"
#include "bg.h"
#include "debug_menu.h"
#include "event_object_movement.h"
#include "gpu_regs.h"
#include "main.h"
#include "menu.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/battle.h"
#include "constants/event_objects.h"
#include "constants/layouts.h"
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

enum
{
    WIN_DEBUG_MENU,
    WIN_DEBUG_PREVIEW
};

static void CB2_DebugMenu(void);
static void VBlankCB_DebugMenu(void);
static void Task_DebugMenuInput(u8 taskId);
static void Task_OwBattlePreviewInput(u8 taskId);
static void InitDebugMenuBgsAndWindows(void);
static void InitOwBattlePreview(void);
static void DrawOwBattlePreviewText(void);
static void RefreshOwBattlePreview(void);
static void ClampOwBattlePreviewOffset(void);
static const struct MapLayout *GetOwBattlePreviewLayout(void);
static const u8 *GetOwBattlePreviewLayoutName(u16 layoutId);
static void ResetOwBattlePreviewOffset(void);
static void CreateOwBattlePreviewSprites(void);

static EWRAM_DATA u16 sOwBattlePreviewLayoutId = 0;
static EWRAM_DATA u16 sOwBattlePreviewX = 0;
static EWRAM_DATA u16 sOwBattlePreviewY = 0;
static EWRAM_DATA bool8 sOwBattlePreviewInitialized = FALSE;
static EWRAM_DATA bool8 sOwBattlePreviewHelpVisible = FALSE;

static const u8 sText_Debug[] = _("DEBUG");
static const u8 sText_OwBattlePreview[] = _("OW BATTLE PREVIEW");
static const u8 sText_Cancel[] = _("CANCEL");
static const u8 sText_Id[] = _("ID");
static const u8 sText_X[] = _("X");
static const u8 sText_Y[] = _("Y");
static const u8 sText_Size[] = _("SIZE");
static const u8 sText_Slash[] = _("/");
static const u8 sText_Times[] = _("x");
static const u8 sText_PreviewHelp1[] = _("D-PAD OFFSET  L/R MAP");
static const u8 sText_PreviewHelp2[] = _("SEL HELP  B BACK");

static const struct MenuAction sDebugMenuActions[] =
{
    {sText_OwBattlePreview, {NULL}},
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
        .height = 6,
        .paletteNum = 1,
        .baseBlock = 1
    },
    [WIN_DEBUG_PREVIEW] = {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 0,
        .width = 30,
        .height = 8,
        .paletteNum = 1,
        .baseBlock = 1
    },
    DUMMY_WIN_TEMPLATE
};

static void CB2_DebugMenu(void)
{
    RunTasks();
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
}

void CB2_InitDebugMenu(void)
{
    SetVBlankCallback(NULL);
    InitDebugMenuBgsAndWindows();
    FillWindowPixelBuffer(WIN_DEBUG_MENU, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_DEBUG_MENU, FONT_NORMAL, sText_Debug, 8, 1, TEXT_SKIP_DRAW, NULL);
    PrintMenuTable(WIN_DEBUG_MENU, ARRAY_COUNT(sDebugMenuActions), sDebugMenuActions);
    InitMenuInUpperLeftCornerNormal(WIN_DEBUG_MENU, ARRAY_COUNT(sDebugMenuActions), 0);
    PutWindowTilemap(WIN_DEBUG_MENU);
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
    ResetPaletteFade();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ScanlineEffect_Stop();
    InitWindows(sDebugMenuWindowTemplates);
    DeactivateAllTextPrinters();
    Menu_LoadStdPalAt(BG_PLTT_ID(1));
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
    ClearWindowTilemap(WIN_DEBUG_MENU);
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

    if (!sOwBattlePreviewHelpVisible)
    {
        FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(1));
        ClearWindowTilemap(WIN_DEBUG_PREVIEW);
        CopyWindowToVram(WIN_DEBUG_PREVIEW, COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        return;
    }

    PutWindowTilemap(WIN_DEBUG_PREVIEW);
    FillWindowPixelBuffer(WIN_DEBUG_PREVIEW, PIXEL_FILL(1));
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_Id, 0, 0, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewLayoutId, STR_CONV_MODE_LEADING_ZEROS, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 16, 0, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_Slash, 40, 0, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, OW_BATTLE_PREVIEW_LAYOUT_COUNT, STR_CONV_MODE_LEADING_ZEROS, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 48, 0, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, GetOwBattlePreviewLayoutName(sOwBattlePreviewLayoutId), 80, 0, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_X, 0, 16, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewX, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 16, 16, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_Y, 48, 16, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, sOwBattlePreviewY, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 64, 16, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_Size, 96, 16, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, layout->width, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 136, 16, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_Times, 160, 16, TEXT_SKIP_DRAW, NULL);
    ConvertIntToDecimalStringN(text, layout->height, STR_CONV_MODE_LEFT_ALIGN, 3);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, text, 168, 16, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_PreviewHelp1, 0, 32, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(WIN_DEBUG_PREVIEW, FONT_NORMAL, sText_PreviewHelp2, 0, 48, TEXT_SKIP_DRAW, NULL);
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
