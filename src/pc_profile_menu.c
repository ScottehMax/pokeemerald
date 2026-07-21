#include "global.h"

#if PLATFORM_PC

#include "bg.h"
#include "dma3.h"
#include "main.h"
#include "main_menu.h"
#include "malloc.h"
#include "menu.h"
#include "naming_screen.h"
#include "palette.h"
#include "pc_platform.h"
#include "pc_profile_menu.h"
#include "pc_profiles.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/characters.h"
#include "constants/field_weather.h"
#include "constants/rgb.h"
#include "constants/songs.h"

#define PROFILE_VISIBLE_ROWS 6

enum
{
    WIN_HEADER,
    WIN_LIST,
    WIN_LEGEND,
};

enum
{
    PROFILE_STATE_WAIT_INPUT,
    PROFILE_STATE_INPUT,
    PROFILE_STATE_CONFIRM_ARCHIVE,
    PROFILE_STATE_SWITCH,
    PROFILE_STATE_EXIT,
};

struct PcProfileMenu
{
    u32 count;
    u32 selected;
    u32 top;
    u8 state;
    u16 bg0Tilemap[BG_SCREEN_SIZE / sizeof(u16)];
};

static EWRAM_DATA struct PcProfileMenu *sProfileMenu;
static EWRAM_DATA u8 sProfileName[PC_PROFILE_NAME_MAX + 1];
static EWRAM_DATA bool8 sCreateProfile;

static const u8 sText_SelectProfile[] = _("SELECT PROFILE");
static const u8 sText_Empty[] = _("NEW GAME");
static const u8 sText_SaveData[] = _("SAVE DATA");
static const u8 sText_Active[] = _("ACTIVE");
static const u8 sText_ButtonA[] = _("{A_BUTTON}");
static const u8 sText_ButtonStart[] = _("{START_BUTTON}");
static const u8 sText_ButtonSelect[] = _("{SELECT_BUTTON}");
static const u8 sText_ButtonB[] = _("{B_BUTTON}");
static const u8 sText_Open[] = _("OPEN");
static const u8 sText_New[] = _("NEW");
static const u8 sText_Archive[] = _("ARCHIVE");
static const u8 sText_Back[] = _("BACK");
static const u8 sText_ConfirmArchive[] = _("Archive this profile?  {A_BUTTON} YES  {B_BUTTON} NO");
static const u8 sText_CantArchive[] = _("The active profile cannot be archived.");
static const u8 sText_CreateFailed[] = _("That profile could not be created.");
static const u8 sText_ArchiveFailed[] = _("That profile could not be archived.");

static const u8 sTextColors[] =
{
    TEXT_COLOR_TRANSPARENT,
    TEXT_COLOR_DARK_GRAY,
    TEXT_COLOR_LIGHT_GRAY,
};

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
};

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_HEADER] = {
        .bg = 0, .tilemapLeft = 2, .tilemapTop = 1, .width = 26, .height = 2,
        .paletteNum = 15, .baseBlock = 1,
    },
    [WIN_LIST] = {
        .bg = 0, .tilemapLeft = 2, .tilemapTop = 5, .width = 26, .height = 11,
        .paletteNum = 15, .baseBlock = 53,
    },
    [WIN_LEGEND] = {
        .bg = 0, .tilemapLeft = 0, .tilemapTop = 18, .width = 30, .height = 2,
        .paletteNum = 15, .baseBlock = 339,
    },
    DUMMY_WIN_TEMPLATE,
};

static void CB2_InitPcProfileMenu(void);
static void CB2_PcProfileMenu(void);
static void VBlankCB_PcProfileMenu(void);
static void Task_PcProfileMenu(u8 taskId);

static void AsciiToGameString(u8 *dest, size_t destSize, const char *source)
{
    size_t i = 0;

    while (i + 1 < destSize && source[i] != '\0')
    {
        unsigned char c = (unsigned char)source[i];

        if (c >= 'A' && c <= 'Z')
            dest[i] = CHAR_A + c - 'A';
        else if (c >= 'a' && c <= 'z')
            dest[i] = CHAR_A + c - 'a';
        else if (c >= '0' && c <= '9')
            dest[i] = CHAR_0 + c - '0';
        else if (c == '-')
            dest[i] = CHAR_HYPHEN;
        else
            dest[i] = CHAR_SPACE;
        i++;
    }
    dest[i] = EOS;
}

static bool32 GameStringToAscii(char *dest, size_t destSize, const u8 *source)
{
    size_t i = 0;

    while (i + 1 < destSize && source[i] != EOS)
    {
        u8 c = source[i];

        if (c >= CHAR_A && c <= CHAR_Z)
            dest[i] = 'A' + c - CHAR_A;
        else if (c >= CHAR_a && c <= CHAR_z)
            dest[i] = 'A' + c - CHAR_a;
        else if (c >= CHAR_0 && c <= CHAR_9)
            dest[i] = '0' + c - CHAR_0;
        else if (c == CHAR_HYPHEN)
            dest[i] = '-';
        else if (c == CHAR_SPACE)
            dest[i] = ' ';
        else
            return FALSE;
        i++;
    }
    while (i != 0 && dest[i - 1] == ' ')
        i--;
    dest[i] = '\0';
    return i != 0;
}

static bool32 IsCurrentProfile(const struct PcProfileInfo *profile)
{
    return strcmp(profile->savePath, PcPlatformGetSavePath()) == 0;
}

static void PrintLegend(const u8 *text)
{
    FillWindowPixelBuffer(WIN_LEGEND, PIXEL_FILL(1));
    AddTextPrinterParameterized4(WIN_LEGEND, FONT_SMALL, 4, 1, 0, 0,
                                 sTextColors, TEXT_SKIP_DRAW, text);
    PutWindowTilemap(WIN_LEGEND);
    CopyWindowToVram(WIN_LEGEND, COPYWIN_GFX);
}

static void PrintCommandLegend(void)
{
    static const u8 *const icons[] =
    {
        sText_ButtonA,
        sText_ButtonStart,
        sText_ButtonSelect,
        sText_ButtonB,
    };
    static const u8 *const labels[] =
    {
        sText_Open,
        sText_New,
        sText_Archive,
        sText_Back,
    };
    u32 widths[ARRAY_COUNT(icons)];
    u32 totalWidth = 0;
    u32 gap;
    u32 x;
    u32 i;

    FillWindowPixelBuffer(WIN_LEGEND, PIXEL_FILL(1));
    for (i = 0; i < ARRAY_COUNT(icons); i++)
    {
        widths[i] = GetStringWidth(FONT_SMALL, icons[i], 0)
                  + 2
                  + GetStringWidth(FONT_SMALL, labels[i], 0);
        totalWidth += widths[i];
    }

    gap = (DISPLAY_WIDTH - totalWidth) / (ARRAY_COUNT(icons) + 1);
    x = gap;
    for (i = 0; i < ARRAY_COUNT(icons); i++)
    {
        u32 iconWidth = GetStringWidth(FONT_SMALL, icons[i], 0);

        AddTextPrinterParameterized4(WIN_LEGEND, FONT_SMALL, x, 1, 0, 0,
                                     sTextColors, TEXT_SKIP_DRAW, icons[i]);
        AddTextPrinterParameterized4(WIN_LEGEND, FONT_SMALL, x + iconWidth + 2, 1, 0, 0,
                                     sTextColors, TEXT_SKIP_DRAW, labels[i]);
        x += widths[i] + gap;
    }
    PutWindowTilemap(WIN_LEGEND);
    CopyWindowToVram(WIN_LEGEND, COPYWIN_GFX);
}

static void DrawProfileList(void)
{
    u32 row;

    DrawStdWindowFrame(WIN_HEADER, FALSE);
    DrawStdWindowFrame(WIN_LIST, FALSE);
    AddTextPrinterParameterized4(WIN_HEADER, FONT_NORMAL, 0, 1, 0, 0,
                                 sTextColors, TEXT_SKIP_DRAW, sText_SelectProfile);
    for (row = 0; row < PROFILE_VISIBLE_ROWS && sProfileMenu->top + row < sProfileMenu->count; row++)
    {
        struct PcProfileInfo profile;
        u8 name[PC_PROFILE_NAME_MAX + 1];
        const u8 *status;
        u8 y = row * 14;

        if (PcProfilesGet(sProfileMenu->top + row, &profile) != 0)
            continue;
        AsciiToGameString(name, sizeof(name), profile.name);
        if (sProfileMenu->top + row == sProfileMenu->selected)
            AddTextPrinterParameterized4(WIN_LIST, FONT_SMALL, 0, y, 0, 0,
                                         sTextColors, TEXT_SKIP_DRAW, gText_SelectorArrow2);
        AddTextPrinterParameterized4(WIN_LIST, FONT_SMALL, 12, y, 0, 0,
                                     sTextColors, TEXT_SKIP_DRAW, name);
        status = IsCurrentProfile(&profile) ? sText_Active
               : profile.hasSave ? sText_SaveData : sText_Empty;
        AddTextPrinterParameterized4(WIN_LIST, FONT_SMALL,
                                     GetStringRightAlignXOffset(FONT_SMALL, status, 200),
                                     y, 0, 0, sTextColors, TEXT_SKIP_DRAW, status);
    }
    PutWindowTilemap(WIN_HEADER);
    PutWindowTilemap(WIN_LIST);
    CopyWindowToVram(WIN_HEADER, COPYWIN_FULL);
    CopyWindowToVram(WIN_LIST, COPYWIN_FULL);
    PrintCommandLegend();
}

static void CleanupProfileMenu(void)
{
    SetVBlankCallback(NULL);
    PcProfilesFree();
    FreeAllWindowBuffers();
    Free(sProfileMenu);
    sProfileMenu = NULL;
}

static void ReturnFromNamingScreen(void)
{
    sCreateProfile = TRUE;
    SetMainCallback2(CB2_InitPcProfileMenu);
}

static void StartProfileNaming(void)
{
    memset(sProfileName, EOS, sizeof(sProfileName));
    CleanupProfileMenu();
    DoNamingScreen(NAMING_SCREEN_PROFILE, sProfileName, 0, 0, 0, ReturnFromNamingScreen);
}

static void Task_PcProfileMenu(u8 taskId)
{
    struct PcProfileInfo profile;

    switch (sProfileMenu->state)
    {
    case PROFILE_STATE_WAIT_INPUT:
        if (!gPaletteFade.active && gMain.heldKeys == 0)
            sProfileMenu->state = PROFILE_STATE_INPUT;
        break;
    case PROFILE_STATE_INPUT:
        if (JOY_NEW(DPAD_UP) && sProfileMenu->selected != 0)
        {
            sProfileMenu->selected--;
            if (sProfileMenu->selected < sProfileMenu->top)
                sProfileMenu->top--;
            PlaySE(SE_SELECT);
            DrawProfileList();
        }
        else if (JOY_NEW(DPAD_DOWN) && sProfileMenu->selected + 1 < sProfileMenu->count)
        {
            sProfileMenu->selected++;
            if (sProfileMenu->selected >= sProfileMenu->top + PROFILE_VISIBLE_ROWS)
                sProfileMenu->top++;
            PlaySE(SE_SELECT);
            DrawProfileList();
        }
        else if (JOY_NEW(A_BUTTON) && PcProfilesGet(sProfileMenu->selected, &profile) == 0)
        {
            PlaySE(SE_SELECT);
            if (IsCurrentProfile(&profile))
            {
                FadeScreen(FADE_TO_BLACK, 0);
                sProfileMenu->state = PROFILE_STATE_EXIT;
            }
            else
            {
                FadeScreen(FADE_TO_BLACK, 0);
                sProfileMenu->state = PROFILE_STATE_SWITCH;
            }
        }
        else if (JOY_NEW(START_BUTTON))
        {
            PlaySE(SE_SELECT);
            StartProfileNaming();
        }
        else if (JOY_NEW(SELECT_BUTTON) && PcProfilesGet(sProfileMenu->selected, &profile) == 0)
        {
            PlaySE(SE_SELECT);
            if (profile.isDefault || IsCurrentProfile(&profile))
                PrintLegend(sText_CantArchive);
            else
            {
                PrintLegend(sText_ConfirmArchive);
                sProfileMenu->state = PROFILE_STATE_CONFIRM_ARCHIVE;
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            FadeScreen(FADE_TO_BLACK, 0);
            sProfileMenu->state = PROFILE_STATE_EXIT;
        }
        break;
    case PROFILE_STATE_CONFIRM_ARCHIVE:
        if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            sProfileMenu->state = PROFILE_STATE_INPUT;
            if (PcProfilesArchive(sProfileMenu->selected) != 0)
            {
                PrintLegend(sText_ArchiveFailed);
            }
            else if (PcProfilesScan(PcPlatformGetDefaultSavePath()) == 0)
            {
                sProfileMenu->count = PcProfilesCount();
                if (sProfileMenu->selected >= sProfileMenu->count)
                    sProfileMenu->selected = sProfileMenu->count - 1;
                if (sProfileMenu->top > sProfileMenu->selected)
                    sProfileMenu->top = sProfileMenu->selected;
                DrawProfileList();
            }
            else
            {
                FadeScreen(FADE_TO_BLACK, 0);
                sProfileMenu->state = PROFILE_STATE_EXIT;
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            sProfileMenu->state = PROFILE_STATE_INPUT;
            PrintCommandLegend();
        }
        break;
    case PROFILE_STATE_SWITCH:
        if (!gPaletteFade.active && PcProfilesGet(sProfileMenu->selected, &profile) == 0)
        {
            DestroyTask(taskId);
            CleanupProfileMenu();
            PcPlatformSwitchProfile(profile.savePath, profile.storagePath);
        }
        break;
    case PROFILE_STATE_EXIT:
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            CleanupProfileMenu();
            SetMainCallback2(CB2_InitMainMenu);
        }
        break;
    }
}

static void CB2_InitPcProfileMenu(void)
{
    char asciiName[PC_PROFILE_NAME_MAX + 1];
    struct PcProfileInfo created;
    bool8 createFailed = FALSE;
    u32 i;

    SetVBlankCallback(NULL);
    DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
    DmaClear32(3, OAM, OAM_SIZE);
    DmaClear16(3, PLTT, PLTT_SIZE);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WIN1H, 0);
    SetGpuReg(REG_OFFSET_WIN1V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    ResetBgsAndClearDma3BusyFlags(0);
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();

    sProfileMenu = AllocZeroed(sizeof(*sProfileMenu));
    if (sProfileMenu == NULL)
    {
        SetMainCallback2(CB2_InitMainMenu);
        return;
    }
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    SetBgTilemapBuffer(0, sProfileMenu->bg0Tilemap);
    if (!InitWindows(sWindowTemplates))
    {
        Free(sProfileMenu);
        sProfileMenu = NULL;
        SetMainCallback2(CB2_InitMainMenu);
        return;
    }
    DeactivateAllTextPrinters();
    LoadMessageBoxAndBorderGfx();
    ShowBg(0);

    if (PcProfilesScan(PcPlatformGetDefaultSavePath()) != 0)
    {
        CleanupProfileMenu();
        SetMainCallback2(CB2_InitMainMenu);
        return;
    }
    if (sCreateProfile)
    {
        sCreateProfile = FALSE;
        if (GameStringToAscii(asciiName, sizeof(asciiName), sProfileName))
        {
            if (PcProfilesCreate(asciiName, &created) == 0)
            {
                CleanupProfileMenu();
                PcPlatformSwitchProfile(created.savePath, created.storagePath);
            }
            createFailed = TRUE;
        }
    }
    sProfileMenu->count = PcProfilesCount();
    for (i = 0; i < sProfileMenu->count; i++)
    {
        struct PcProfileInfo profile;

        if (PcProfilesGet(i, &profile) == 0 && IsCurrentProfile(&profile))
        {
            sProfileMenu->selected = i;
            if (i >= PROFILE_VISIBLE_ROWS)
                sProfileMenu->top = i - PROFILE_VISIBLE_ROWS + 1;
            break;
        }
    }
    DrawProfileList();
    if (createFailed)
        PrintLegend(sText_CreateFailed);
    CreateTask(Task_PcProfileMenu, 80);
    sProfileMenu->state = PROFILE_STATE_WAIT_INPUT;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_PcProfileMenu);
    SetMainCallback2(CB2_PcProfileMenu);
}

static void CB2_PcProfileMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_PcProfileMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void ShowPcProfileMenu(void)
{
    sCreateProfile = FALSE;
    sProfileName[0] = EOS;
    SetMainCallback2(CB2_InitPcProfileMenu);
}

#endif
