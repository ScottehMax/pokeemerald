#include "global.h"

#if PLATFORM_PC

#include "bg.h"
#include "data.h"
#include "dma3.h"
#include "list_menu.h"
#include "malloc.h"
#include "main.h"
#include "menu.h"
#include "menu_helpers.h"
#include "overworld.h"
#include "palette.h"
#include "pc_services.h"
#include "pc_storage_menu.h"
#include "pokemon.h"
#include "pokemon_icon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "strings.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/field_weather.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/species.h"

#define STORAGE_COLUMNS 2
#define STORAGE_VISIBLE_ROWS 4
#define STORAGE_VISIBLE_MONS (STORAGE_COLUMNS * STORAGE_VISIBLE_ROWS)
#define STORAGE_PANE_WIDTH 112
#define STORAGE_BG_TILE 0x40
#define STORAGE_PANE_BASE_TILE 0x41
#define STORAGE_PANE_TILE_COUNT 32
#define STORAGE_CURSOR_BASE_TILE 0x141
#define STORAGE_LEGEND_BASE_TILE 0x149
#define STORAGE_BORDER_TILE 0x214
#define STORAGE_BORDER_PAL 14

enum
{
    STORAGE_STATE_WAIT_INPUT,
    STORAGE_STATE_INPUT,
    STORAGE_STATE_CONFIRM_IMPORT,
    STORAGE_STATE_MESSAGE,
    STORAGE_STATE_ENTER_BOX,
    STORAGE_STATE_EXIT,
};

struct PcStorageMenu
{
    u32 fileCount;
    u32 selectedFile;
    u16 topRow;
    u8 backgroundWindowId;
    u8 paneWindowIds[STORAGE_VISIBLE_MONS];
    u8 cursorWindowId;
    u8 legendWindowId;
    u8 scrollArrowTaskId;
    u8 iconSpriteIds[STORAGE_VISIBLE_MONS];
    u8 state;
    u16 bg0Tilemap[BG_SCREEN_SIZE / sizeof(u16)];
    u16 bg1Tilemap[BG_SCREEN_SIZE / sizeof(u16)];
};

static EWRAM_DATA struct PcStorageMenu *sPcStorageMenu = NULL;

static const u8 sText_AddFromBox[] = _("{R_BUTTON} ADD FROM BOX");
static const u8 sText_Back[] = _("{B_BUTTON} BACK");
static const u8 sText_NoStoredPokemon[] = _("No stored\nPOKéMON.");
static const u8 sText_MoveToBox[] = _("Move this POKéMON to a BOX?");
static const u8 sText_BoxFull[] = _("The POKéMON BOXES are full.");
static const u8 sText_ImportFailed[] = _("The .ek3 file could not be loaded.");
static const u8 sText_SaveFailed[] = _("The POKéMON could not be saved.");
static const u8 sText_Imported[] = _("The POKéMON was moved to BOX {STR_VAR_1}.");
static const u8 sText_ImportedCopyLeft[] = _("Moved to BOX {STR_VAR_1}.\nThe .ek3 file was kept.");
static const u8 sText_StorageUnavailable[] = _("The storage folder could not be opened.");
static const u8 sText_Level[] = _("Lv.");
static const u8 sText_InvalidEk3[] = _("INVALID .ek3");

static const u8 sStorageTextColors[] =
{
    TEXT_COLOR_TRANSPARENT,
    TEXT_COLOR_DARK_GRAY,
    TEXT_COLOR_LIGHT_GRAY,
};

static const struct WindowTemplate sStorageGridWindow =
{
    .bg = 1,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 1,
    .height = 1,
    .paletteNum = 15,
    .baseBlock = STORAGE_BG_TILE,
};

static const struct WindowTemplate sStorageLegendWindow =
{
    .bg = 1,
    .tilemapLeft = 1,
    .tilemapTop = 17,
    .width = 28,
    .height = 2,
    .paletteNum = 15,
    .baseBlock = STORAGE_LEGEND_BASE_TILE,
};

static const struct WindowTemplate sStorageYesNoWindow =
{
    .bg = 0,
    .tilemapLeft = 21,
    .tilemapTop = 9,
    .width = 5,
    .height = 4,
    .paletteNum = 15,
    .baseBlock = 0x220,
};

static const struct WindowTemplate sStorageMessageWindows[] =
{
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 15,
        .width = 27,
        .height = 4,
        .paletteNum = 15,
        .baseBlock = 0x194,
    },
    DUMMY_WIN_TEMPLATE,
};

static const struct BgTemplate sStorageBgTemplates[] =
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
    {
        .bg = 1,
        .charBaseIndex = 0,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
};

static void Task_PcStorageMenu(u8 taskId);
static bool32 CreateStorageGrid(void);
static void DestroyStorageGrid(void);
static void DrawStorageGrid(void);
static void PrintStorageMessage(const u8 *message);
static void ClearStorageMessage(void);
static bool32 ReadValidStorageMon(u32 index, struct BoxPokemon *mon);
static bool32 ImportStorageMon(u32 index, u8 *boxId);
static bool32 MoveStorageCursor(u16 keys);
static bool32 RebuildStorageGrid(void);
static void CleanupStorageScreen(bool32 destroyGrid);
static void CB2_InitPcStorageMenu(void);
static void CB2_PcStorageMenu(void);
static void VBlankCB_PcStorageMenu(void);

static void DrawStorageBorder(void)
{
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 0, 0,  0,  1,  1, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 1, 1,  0, 28,  1, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 2, 29, 0,  1,  1, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 3, 0,  1,  1, 18, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 5, 29, 1,  1, 18, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 6, 0, 19,  1,  1, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 7, 1, 19, 28,  1, STORAGE_BORDER_PAL);
    FillBgTilemapBufferRect(1, STORAGE_BORDER_TILE + 8, 29, 19, 1,  1, STORAGE_BORDER_PAL);
}

static void PrintStorageMessage(const u8 *message)
{
    DrawDialogueFrame(0, TRUE);
    FillWindowPixelBuffer(0, PIXEL_FILL(1));
    AddTextPrinterParameterized2(0,
                                 FONT_NORMAL,
                                 message,
                                 0,
                                 NULL,
                                 TEXT_COLOR_DARK_GRAY,
                                 TEXT_COLOR_WHITE,
                                 TEXT_COLOR_LIGHT_GRAY);
    CopyWindowToVram(0, COPYWIN_GFX);
}

static void ClearStorageMessage(void)
{
    ClearDialogWindowAndFrameToTransparent(0, TRUE);
    DrawStorageGrid();
}

static bool32 ReadValidStorageMon(u32 index, struct BoxPokemon *mon)
{
    u16 species;

    if (!PcStorageReadMon(index, mon))
        return FALSE;
    species = GetBoxMonData(mon, MON_DATA_SPECIES);
    return species != SPECIES_NONE
        && species < NUM_SPECIES
        && !GetBoxMonData(mon, MON_DATA_SANITY_IS_BAD_EGG);
}

static void DestroyStorageIcons(void)
{
    u8 i;

    for (i = 0; i < STORAGE_VISIBLE_MONS; i++)
    {
        if (sPcStorageMenu->iconSpriteIds[i] < MAX_SPRITES)
        {
            FreeAndDestroyMonIconSprite(&gSprites[sPcStorageMenu->iconSpriteIds[i]]);
            sPcStorageMenu->iconSpriteIds[i] = MAX_SPRITES;
        }
    }
}

static void DrawStorageGrid(void)
{
    u32 firstFile = sPcStorageMenu->topRow * STORAGE_COLUMNS;
    u32 slot;
    u8 text[POKEMON_NAME_LENGTH + 8];
    u8 nickname[POKEMON_NAME_LENGTH + 1];

    DestroyStorageIcons();
    FillWindowPixelBuffer(sPcStorageMenu->backgroundWindowId, PIXEL_FILL(1));
    CopyWindowToVram(sPcStorageMenu->backgroundWindowId, COPYWIN_GFX);
    FillBgTilemapBufferRect(1, STORAGE_BG_TILE, 1, 1, 28, 18, 15);
    for (slot = 0; slot < STORAGE_VISIBLE_MONS; slot++)
        FillWindowPixelBuffer(sPcStorageMenu->paneWindowIds[slot], PIXEL_FILL(1));
    FillWindowPixelBuffer(sPcStorageMenu->cursorWindowId, PIXEL_FILL(1));
    FillWindowPixelBuffer(sPcStorageMenu->legendWindowId, PIXEL_FILL(1));

    if (sPcStorageMenu->fileCount == 0)
        AddTextPrinterParameterized4(sPcStorageMenu->paneWindowIds[0],
                                     FONT_SMALL,
                                     0,
                                     1,
                                     0,
                                     0,
                                     sStorageTextColors,
                                     TEXT_SKIP_DRAW,
                                     sText_NoStoredPokemon);

    for (slot = 0; slot < STORAGE_VISIBLE_MONS; slot++)
    {
        struct BoxPokemon mon;
        u32 file = firstFile + slot;

        if (file >= sPcStorageMenu->fileCount)
            break;

        if (!ReadValidStorageMon(file, &mon))
        {
            AddTextPrinterParameterized4(sPcStorageMenu->paneWindowIds[slot],
                                         FONT_SMALL,
                                         0,
                                         8,
                                         0,
                                         0,
                                         sStorageTextColors,
                                         TEXT_SKIP_DRAW,
                                         sText_InvalidEk3);
            continue;
        }

        GetBoxMonData(&mon, MON_DATA_NICKNAME, nickname);
        StringCopy_Nickname(text, nickname);
        AddTextPrinterParameterized4(sPcStorageMenu->paneWindowIds[slot],
                                     FONT_SMALL,
                                     0,
                                     1,
                                     0,
                                     0,
                                     sStorageTextColors,
                                     TEXT_SKIP_DRAW,
                                     text);
        StringCopy(text, sText_Level);
        ConvertIntToDecimalStringN(StringCopy(text, sText_Level),
                                   GetLevelFromBoxMonExp(&mon),
                                   STR_CONV_MODE_LEFT_ALIGN,
                                   3);
        AddTextPrinterParameterized4(sPcStorageMenu->paneWindowIds[slot],
                                     FONT_SMALL,
                                     0,
                                     16,
                                     0,
                                     0,
                                     sStorageTextColors,
                                     TEXT_SKIP_DRAW,
                                     text);

        sPcStorageMenu->iconSpriteIds[slot] = CreateMonIcon(
            GetBoxMonData(&mon, MON_DATA_SPECIES),
            SpriteCB_MonIcon,
            32 + (slot % STORAGE_COLUMNS) * STORAGE_PANE_WIDTH,
            24 + (slot / STORAGE_COLUMNS) * 32,
            0,
            GetBoxMonData(&mon, MON_DATA_PERSONALITY),
            TRUE);
    }

    if (sPcStorageMenu->fileCount != 0)
    {
        slot = sPcStorageMenu->selectedFile - firstFile;
        SetWindowAttribute(sPcStorageMenu->cursorWindowId,
                           WINDOW_TILEMAP_LEFT,
                           1 + (slot % STORAGE_COLUMNS) * 14);
        SetWindowAttribute(sPcStorageMenu->cursorWindowId,
                           WINDOW_TILEMAP_TOP,
                           1 + (slot / STORAGE_COLUMNS) * 4);
        AddTextPrinterParameterized4(sPcStorageMenu->cursorWindowId,
                                     FONT_SMALL,
                                     0,
                                     8,
                                     0,
                                     0,
                                     sStorageTextColors,
                                     TEXT_SKIP_DRAW,
                                     gText_SelectorArrow2);
        PutWindowTilemap(sPcStorageMenu->cursorWindowId);
    }

    AddTextPrinterParameterized4(sPcStorageMenu->legendWindowId,
                                 FONT_SMALL,
                                 0,
                                 1,
                                 0,
                                 0,
                                 sStorageTextColors,
                                 TEXT_SKIP_DRAW,
                                 sText_AddFromBox);
    AddTextPrinterParameterized4(sPcStorageMenu->legendWindowId,
                                 FONT_SMALL,
                                 GetStringRightAlignXOffset(FONT_SMALL, sText_Back, 224),
                                 1,
                                 0,
                                 0,
                                 sStorageTextColors,
                                 TEXT_SKIP_DRAW,
                                 sText_Back);

    for (slot = 0; slot < STORAGE_VISIBLE_MONS; slot++)
    {
        PutWindowTilemap(sPcStorageMenu->paneWindowIds[slot]);
        CopyWindowToVram(sPcStorageMenu->paneWindowIds[slot], COPYWIN_GFX);
    }
    PutWindowTilemap(sPcStorageMenu->legendWindowId);
    DrawStorageBorder();
    CopyWindowToVram(sPcStorageMenu->cursorWindowId, COPYWIN_GFX);
    CopyWindowToVram(sPcStorageMenu->legendWindowId, COPYWIN_GFX);
    CopyBgTilemapBufferToVram(1);
}

static bool32 CreateStorageGrid(void)
{
    struct WindowTemplate window;
    u8 i;
    u16 rowCount;

    sPcStorageMenu->fileCount = PcStorageCount();
    sPcStorageMenu->backgroundWindowId = AddWindow(&sStorageGridWindow);
    if (sPcStorageMenu->backgroundWindowId == WINDOW_NONE)
        return FALSE;
    for (i = 0; i < STORAGE_VISIBLE_MONS; i++)
    {
        window = CreateWindowTemplate(
            1,
            7 + (i % STORAGE_COLUMNS) * 14,
            1 + (i / STORAGE_COLUMNS) * 4,
            8,
            4,
            15,
            STORAGE_PANE_BASE_TILE + i * STORAGE_PANE_TILE_COUNT);
        sPcStorageMenu->paneWindowIds[i] = AddWindow(&window);
        if (sPcStorageMenu->paneWindowIds[i] == WINDOW_NONE)
            goto fail_panes;
    }
    window = CreateWindowTemplate(1, 1, 1, 2, 4, 15, STORAGE_CURSOR_BASE_TILE);
    sPcStorageMenu->cursorWindowId = AddWindow(&window);
    if (sPcStorageMenu->cursorWindowId == WINDOW_NONE)
        goto fail_panes;
    sPcStorageMenu->legendWindowId = AddWindow(&sStorageLegendWindow);
    if (sPcStorageMenu->legendWindowId == WINDOW_NONE)
        goto fail_cursor;

    for (i = 0; i < STORAGE_VISIBLE_MONS; i++)
        sPcStorageMenu->iconSpriteIds[i] = MAX_SPRITES;
    LoadMonIconPalettes();

    sPcStorageMenu->scrollArrowTaskId = TASK_NONE;
    rowCount = (sPcStorageMenu->fileCount + STORAGE_COLUMNS - 1) / STORAGE_COLUMNS;
    if (rowCount > STORAGE_VISIBLE_ROWS)
    {
        sPcStorageMenu->scrollArrowTaskId = AddScrollIndicatorArrowPairParameterized(
            SCROLL_ARROW_UP,
            220,
            16,
            120,
            rowCount - STORAGE_VISIBLE_ROWS,
            0x6E,
            0x6E,
            &sPcStorageMenu->topRow);
    }
    DrawStorageGrid();
    return TRUE;

fail_cursor:
    RemoveWindow(sPcStorageMenu->cursorWindowId);
fail_panes:
    while (i != 0)
        RemoveWindow(sPcStorageMenu->paneWindowIds[--i]);
    RemoveWindow(sPcStorageMenu->backgroundWindowId);
    return FALSE;
}

static void DestroyStorageGrid(void)
{
    u8 i;

    if (sPcStorageMenu->scrollArrowTaskId != TASK_NONE)
        RemoveScrollIndicatorArrowPair(sPcStorageMenu->scrollArrowTaskId);
    DestroyStorageIcons();
    FreeMonIconPalettes();
    ClearWindowTilemap(sPcStorageMenu->backgroundWindowId);
    for (i = 0; i < STORAGE_VISIBLE_MONS; i++)
    {
        ClearWindowTilemap(sPcStorageMenu->paneWindowIds[i]);
        RemoveWindow(sPcStorageMenu->paneWindowIds[i]);
    }
    ClearWindowTilemap(sPcStorageMenu->cursorWindowId);
    ClearWindowTilemap(sPcStorageMenu->legendWindowId);
    RemoveWindow(sPcStorageMenu->backgroundWindowId);
    RemoveWindow(sPcStorageMenu->cursorWindowId);
    RemoveWindow(sPcStorageMenu->legendWindowId);
}

static bool32 ImportStorageMon(u32 index, u8 *boxId)
{
    struct BoxPokemon mon;
    struct Pokedex oldPokedex;
    u8 oldSeen1[NUM_DEX_FLAG_BYTES];
    u8 oldSeen2[NUM_DEX_FLAG_BYTES];
    s16 boxPosition;
    u8 firstBox = StorageGetCurrentBox();
    u8 i;

    *boxId = 0xFF;
    if (!ReadValidStorageMon(index, &mon))
        return FALSE;
    for (i = 0; i < TOTAL_BOXES_COUNT; i++)
    {
        *boxId = (firstBox + i) % TOTAL_BOXES_COUNT;
        boxPosition = GetFirstFreeBoxSpot(*boxId);
        if (boxPosition >= 0)
            break;
    }
    if (i == TOTAL_BOXES_COUNT)
    {
        *boxId = TOTAL_BOXES_COUNT;
        return FALSE;
    }

    oldPokedex = gSaveBlock2Ptr->pokedex;
    memcpy(oldSeen1, gSaveBlock1Ptr->seen1, sizeof(oldSeen1));
    memcpy(oldSeen2, gSaveBlock1Ptr->seen2, sizeof(oldSeen2));
    SetBoxMonAt(*boxId, boxPosition, &mon);
    UpdatePokedexForReceivedBoxMon(&mon);
    if (TrySavingData(SAVE_NORMAL) != SAVE_STATUS_OK)
    {
        ZeroBoxMonAt(*boxId, boxPosition);
        gSaveBlock2Ptr->pokedex = oldPokedex;
        memcpy(gSaveBlock1Ptr->seen1, oldSeen1, sizeof(oldSeen1));
        memcpy(gSaveBlock1Ptr->seen2, oldSeen2, sizeof(oldSeen2));
        *boxId = 0xFE;
        return FALSE;
    }
    return TRUE;
}

static bool32 MoveStorageCursor(u16 keys)
{
    u32 oldSelection = sPcStorageMenu->selectedFile;
    u32 selectedRow;

    if (sPcStorageMenu->fileCount == 0)
        return FALSE;

    if ((keys & DPAD_LEFT) && (sPcStorageMenu->selectedFile % STORAGE_COLUMNS) != 0)
        sPcStorageMenu->selectedFile--;
    else if ((keys & DPAD_RIGHT)
          && (sPcStorageMenu->selectedFile % STORAGE_COLUMNS) == 0
          && sPcStorageMenu->selectedFile + 1 < sPcStorageMenu->fileCount)
        sPcStorageMenu->selectedFile++;
    else if ((keys & DPAD_UP) && sPcStorageMenu->selectedFile >= STORAGE_COLUMNS)
        sPcStorageMenu->selectedFile -= STORAGE_COLUMNS;
    else if ((keys & DPAD_DOWN)
          && sPcStorageMenu->selectedFile + STORAGE_COLUMNS < sPcStorageMenu->fileCount)
        sPcStorageMenu->selectedFile += STORAGE_COLUMNS;

    if (oldSelection == sPcStorageMenu->selectedFile)
        return FALSE;

    selectedRow = sPcStorageMenu->selectedFile / STORAGE_COLUMNS;
    if (selectedRow < sPcStorageMenu->topRow)
        sPcStorageMenu->topRow = selectedRow;
    else if (selectedRow >= sPcStorageMenu->topRow + STORAGE_VISIBLE_ROWS)
        sPcStorageMenu->topRow = selectedRow - STORAGE_VISIBLE_ROWS + 1;
    return TRUE;
}

static bool32 RebuildStorageGrid(void)
{
    bool32 scanSucceeded;
    u32 rowCount;
    u8 taskId;

    DestroyStorageGrid();
    scanSucceeded = PcStorageScan();
    sPcStorageMenu->fileCount = PcStorageCount();
    if (sPcStorageMenu->fileCount == 0)
        sPcStorageMenu->selectedFile = 0;
    else if (sPcStorageMenu->selectedFile >= sPcStorageMenu->fileCount)
        sPcStorageMenu->selectedFile = sPcStorageMenu->fileCount - 1;

    rowCount = (sPcStorageMenu->fileCount + STORAGE_COLUMNS - 1) / STORAGE_COLUMNS;
    if (rowCount > STORAGE_VISIBLE_ROWS)
        rowCount -= STORAGE_VISIBLE_ROWS;
    else
        rowCount = 0;
    if (sPcStorageMenu->topRow > rowCount)
        sPcStorageMenu->topRow = rowCount;
    if (CreateStorageGrid())
    {
        if (!scanSucceeded)
            PrintStorageMessage(sText_StorageUnavailable);
        return TRUE;
    }

    taskId = FindTaskIdByFunc(Task_PcStorageMenu);
    if (taskId != TASK_NONE)
        DestroyTask(taskId);
    CleanupStorageScreen(FALSE);
    PcStorageExitToPcMenu();
    return FALSE;
}

static void CleanupStorageScreen(bool32 destroyGrid)
{
    SetVBlankCallback(NULL);
    if (destroyGrid)
        DestroyStorageGrid();
    FreeAllWindowBuffers();
    Free(sPcStorageMenu);
    sPcStorageMenu = NULL;
}

static void Task_PcStorageMenu(u8 taskId)
{
    s32 input;

    switch (sPcStorageMenu->state)
    {
    case STORAGE_STATE_WAIT_INPUT:
        if (!gPaletteFade.active && gMain.heldKeys == 0)
            sPcStorageMenu->state = STORAGE_STATE_INPUT;
        break;
    case STORAGE_STATE_INPUT:
        if (JOY_NEW(R_BUTTON))
        {
            PlaySE(SE_SELECT);
            FadeScreen(FADE_TO_BLACK, 0);
            sPcStorageMenu->state = STORAGE_STATE_ENTER_BOX;
        }
        else if (JOY_NEW(B_BUTTON))
        {
            PlaySE(SE_SELECT);
            FadeScreen(FADE_TO_BLACK, 0);
            sPcStorageMenu->state = STORAGE_STATE_EXIT;
        }
        else if (JOY_NEW(A_BUTTON) && sPcStorageMenu->fileCount != 0)
        {
            PlaySE(SE_SELECT);
            PrintStorageMessage(sText_MoveToBox);
            CreateYesNoMenu(&sStorageYesNoWindow, 0x214, 14, 0);
            sPcStorageMenu->state = STORAGE_STATE_CONFIRM_IMPORT;
        }
        else if (MoveStorageCursor(gMain.newKeys & DPAD_ANY))
        {
            PlaySE(SE_SELECT);
            DrawStorageGrid();
        }
        break;
    case STORAGE_STATE_CONFIRM_IMPORT:
        input = Menu_ProcessInputNoWrapClearOnChoose();
        switch (input)
        {
        case 0:
        {
            u8 boxId;

            if (!ImportStorageMon(sPcStorageMenu->selectedFile, &boxId))
            {
                PlaySE(SE_FAILURE);
                if (boxId == TOTAL_BOXES_COUNT)
                    PrintStorageMessage(sText_BoxFull);
                else if (boxId == 0xFE)
                    PrintStorageMessage(sText_SaveFailed);
                else
                    PrintStorageMessage(sText_ImportFailed);
            }
            else
            {
                ConvertIntToDecimalStringN(gStringVar1,
                                           boxId + 1,
                                           STR_CONV_MODE_LEFT_ALIGN,
                                           2);
                if (PcStorageDeleteMon(sPcStorageMenu->selectedFile))
                    StringExpandPlaceholders(gStringVar4, sText_Imported);
                else
                    StringExpandPlaceholders(gStringVar4, sText_ImportedCopyLeft);
                PrintStorageMessage(gStringVar4);
            }
            sPcStorageMenu->state = STORAGE_STATE_MESSAGE;
            break;
        }
        case 1:
        case MENU_B_PRESSED:
            ClearStorageMessage();
            sPcStorageMenu->state = STORAGE_STATE_INPUT;
            break;
        }
        break;
    case STORAGE_STATE_MESSAGE:
        if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            ClearDialogWindowAndFrameToTransparent(0, TRUE);
            if (!RebuildStorageGrid())
                break;
            sPcStorageMenu->state = STORAGE_STATE_INPUT;
        }
        break;
    case STORAGE_STATE_ENTER_BOX:
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            CleanupStorageScreen(TRUE);
            PcStorageEnterBox();
        }
        break;
    case STORAGE_STATE_EXIT:
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            CleanupStorageScreen(TRUE);
            PcStorageExitToPcMenu();
        }
        break;
    }
}

static void CB2_PcStorageMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_PcStorageMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_InitPcStorageMenu(void)
{
    bool32 scanSucceeded;

    SetVBlankCallback(NULL);
    DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
    DmaClear32(3, OAM, OAM_SIZE);
    DmaClear16(3, PLTT, PLTT_SIZE);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    ResetBgsAndClearDma3BusyFlags(0);
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    FreeAllSpritePalettes();
    ResetPaletteFade();

    sPcStorageMenu = AllocZeroed(sizeof(*sPcStorageMenu));
    if (sPcStorageMenu == NULL)
    {
        PcStorageExitToPcMenu();
        return;
    }

    InitBgsFromTemplates(0, sStorageBgTemplates, ARRAY_COUNT(sStorageBgTemplates));
    SetBgTilemapBuffer(0, sPcStorageMenu->bg0Tilemap);
    SetBgTilemapBuffer(1, sPcStorageMenu->bg1Tilemap);
    if (!InitWindows(sStorageMessageWindows))
    {
        CleanupStorageScreen(FALSE);
        PcStorageExitToPcMenu();
        return;
    }
    DeactivateAllTextPrinters();
    LoadMessageBoxAndBorderGfx();

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    ShowBg(0);
    ShowBg(1);

    scanSucceeded = PcStorageScan();
    if (!CreateStorageGrid())
    {
        CleanupStorageScreen(FALSE);
        PcStorageExitToPcMenu();
        return;
    }
    if (!scanSucceeded)
        PrintStorageMessage(sText_StorageUnavailable);

    CreateTask(Task_PcStorageMenu, 80);
    sPcStorageMenu->state = scanSucceeded ? STORAGE_STATE_WAIT_INPUT : STORAGE_STATE_MESSAGE;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(VBlankCB_PcStorageMenu);
    SetMainCallback2(CB2_PcStorageMenu);
}

void ShowPcStorageMenu(void)
{
    SetMainCallback2(CB2_InitPcStorageMenu);
}

void PcStorageMenuReturnToField(void)
{
    CleanupOverworldWindowsAndTilemaps();
    ShowPcStorageMenu();
}

#endif // PLATFORM_PC
