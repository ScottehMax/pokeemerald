#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_controllers.h"
#include "battle_main.h"
#include "battle_message.h"
#include "battle_setup.h"
#include "bg.h"
#include "data.h"
#include "gpu_regs.h"
#include "intro.h"
#include "item.h"
#include "link_viewer.h"
#include "load_save.h"
#include "main.h"
#include "m4a.h"
#include "malloc.h"
#include "menu.h"
#include "new_game.h"
#include "palette.h"
#include "pokemon.h"
#include "random.h"
#include "save.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "string_util.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "constants/abilities.h"
#include "constants/battle_anim.h"
#include "constants/link_viewer.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/trainers.h"

// JOY Bus registers (not in io_reg.h as plain register macros).
#define REG_LV_JOYCNT     (*(vu16 *)REG_ADDR_JOYCNT)
#define REG_LV_JOYSTAT    (*(vu16 *)REG_ADDR_JOYSTAT)
#define REG_LV_JOY_RECV_L (*(vu16 *)REG_ADDR_JOY_RECV_L)
#define REG_LV_JOY_RECV_H (*(vu16 *)REG_ADDR_JOY_RECV_H)
#define REG_LV_JOY_TRANS_L (*(vu16 *)REG_ADDR_JOY_TRANS_L)
#define REG_LV_JOY_TRANS_H (*(vu16 *)REG_ADDR_JOY_TRANS_H)

#define RCNT_JOYBUS_MODE 0xC000
#define JOYCNT_RESET 0x01
#define JOYCNT_RECV  0x02
#define JOYCNT_TRANS 0x04
#define JOYCNT_IRQ   0x40

#define RX_BUFFER_SIZE 2048 // power of two
#define RX_MASK (RX_BUFFER_SIZE - 1)
#define MAX_MSG_LENGTH 255

#define DEFAULT_TEXT_DELAY 40
#define TEXT_WRAP_WIDTH 200
#define MAX_MSGS_PER_FRAME 8
#define END_DELAY 150

enum {
    VIEWER_STATE_WAITING,
    VIEWER_STATE_BATTLE,
};

struct ViewerMsg
{
    u8 length; // type + payload
    u8 type;
    u8 data[MAX_MSG_LENGTH];
};

struct ViewerBattleState
{
    struct ViewerMsg msg; // the message being shown
    bool8 (*op)(void);    // shows msg; returns TRUE when done
    u8 stage;
    u16 timer;
    u8 textDelay;
    bool8 monOut[NUM_BATTLE_SIDES];
    u8 pendingSlot;
    bool8 ending;
};

struct ViewerSetup
{
    u8 names[NUM_BATTLE_SIDES][PLAYER_NAME_LENGTH + 1];
    u8 genders[NUM_BATTLE_SIDES];
    u8 opponentClass;
    u16 bgm;
    u8 environment;
    u8 textDelay;
};

EWRAM_DATA bool8 gLinkViewerActive = FALSE;

static EWRAM_DATA u8 sRxBuffer[RX_BUFFER_SIZE] = {0};
static EWRAM_DATA vu16 sRxHead = 0; // written by the serial interrupt
static EWRAM_DATA vu16 sRxTail = 0; // written by the main loop
static EWRAM_DATA vu8 sLinkSeen = FALSE;
static EWRAM_DATA vu8 sNeedSync = FALSE;
static EWRAM_DATA vu16 sConsumed = 0; // stream bytes taken from the buffer since the last reset
static EWRAM_DATA struct ViewerMsg sPeekMsg = {0};
static EWRAM_DATA struct ViewerBattleState sBattle = {0};
static EWRAM_DATA struct ViewerSetup sSetup = {0};
static EWRAM_DATA u8 sScreenStatus = 0;
static EWRAM_DATA u8 sScreenStatusShown = 0;

static void CB2_LinkViewer_InitWaitScreen(void);
static void CB2_LinkViewer_WaitScreen(void);
static void CB2_LinkViewer_StartBattle(void);
static void VBlankCB_LinkViewer(void);
static void SerialCB_LinkViewer(void);

static const u8 sText_Title[] = _("LINK BATTLE VIEWER");
static const u8 sText_WaitingForLink[] = _("Waiting for the link…\nIn mGBA: File > Connect to Dolphin");
static const u8 sText_Connected[] = _("Link connected.\nWaiting for a battle…");
static const u8 sText_ReceivingTeams[] = _("Receiving teams…");
static const u8 sText_Vs[] = _(" vs. ");
static const u8 sText_DefaultPlayer[] = _("PLAYER");
static const u8 sText_DefaultOpponent[] = _("FOE");

static const struct BgTemplate sWaitScreenBgTemplates[] =
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

#define WIN_TITLE  0
#define WIN_STATUS 1

static const struct WindowTemplate sWaitScreenWindowTemplates[] =
{
    [WIN_TITLE] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 1,
        .width = 28,
        .height = 2,
        .paletteNum = 15,
        .baseBlock = 1,
    },
    [WIN_STATUS] = {
        .bg = 0,
        .tilemapLeft = 1,
        .tilemapTop = 6,
        .width = 28,
        .height = 8,
        .paletteNum = 15,
        .baseBlock = 1 + 28 * 2,
    },
    DUMMY_WIN_TEMPLATE
};

// ---------------------------------------------------------------------------
// JOY Bus receiver

static u16 RxCount(void)
{
    return (sRxHead - sRxTail) & RX_MASK;
}

// Reads a word from JOY_RECV if one is waiting and there is room for it.
// Interrupts must be off (or this must be the serial interrupt).
static void TryReceiveWord(void)
{
    u16 lo, hi, head;

    if (!(REG_LV_JOYSTAT & LV_JOYSTAT_RECV))
        return;
    if (RX_BUFFER_SIZE - 1 - ((sRxHead - sRxTail) & RX_MASK) < 4)
        return; // full: leave the word unread, which holds off the host

    lo = REG_LV_JOY_RECV_L;
    hi = REG_LV_JOY_RECV_H;
    head = sRxHead;
    sRxBuffer[head] = lo;
    sRxBuffer[(head + 1) & RX_MASK] = lo >> 8;
    sRxBuffer[(head + 2) & RX_MASK] = hi;
    sRxBuffer[(head + 3) & RX_MASK] = hi >> 8;
    sRxHead = (head + 4) & RX_MASK;
    sLinkSeen = TRUE;
}

static void SerialCB_LinkViewer(void)
{
    u16 joyCnt = REG_LV_JOYCNT;

    if (joyCnt & JOYCNT_RESET)
    {
        // The host (re)started its stream.
        sRxHead = 0;
        sRxTail = 0;
        sLinkSeen = TRUE;
        sNeedSync = FALSE;
        sConsumed = 0;
        REG_LV_JOY_TRANS_L = 0;
        REG_LV_JOYSTAT = LV_JOYSTAT_READY;
    }
    TryReceiveWord();
    REG_LV_JOYCNT = JOYCNT_IRQ | JOYCNT_RESET | JOYCNT_RECV | JOYCNT_TRANS; // acknowledge
}

// Picks up a word the interrupt had to leave unread because the buffer was full.
static void PumpReceiver(void)
{
    u16 ime = REG_IME;

    REG_IME = 0;
    TryReceiveWord();
    REG_IME = ime;
}

static void SetJoyStat(void)
{
    REG_LV_JOYSTAT = sNeedSync ? (LV_JOYSTAT_READY | LV_JOYSTAT_NEED_SYNC) : LV_JOYSTAT_READY;
}

static void InitReceiver(void)
{
    u16 ime = REG_IME;

    REG_IME = 0;
    REG_RCNT = RCNT_JOYBUS_MODE;
    REG_LV_JOY_TRANS_L = sConsumed;
    REG_LV_JOY_TRANS_H = LV_MAGIC;
    REG_LV_JOYCNT = JOYCNT_IRQ | JOYCNT_RESET | JOYCNT_RECV | JOYCNT_TRANS;
    SetSerialCallback(SerialCB_LinkViewer);
    SetJoyStat();
    REG_IME = ime;
    EnableInterrupts(INTR_FLAG_SERIAL);
}

// Removes bytes from the buffer and reports the running count to the host.
static void RxDiscard(u16 count)
{
    u16 ime = REG_IME;

    REG_IME = 0; // a reset from the host in between would zero these
    sRxTail = (sRxTail + count) & RX_MASK;
    sConsumed += count;
    REG_LV_JOY_TRANS_L = sConsumed;
    REG_IME = ime;
}

static u8 RxPeekByte(u16 offset)
{
    return sRxBuffer[(sRxTail + offset) & RX_MASK];
}

// Copies the next complete message into sPeekMsg without removing it.
static bool8 PeekMessage(void)
{
    u16 i, count;
    u8 length;

    PumpReceiver();
    count = RxCount();
    while (count != 0 && RxPeekByte(0) == 0) // padding
    {
        RxDiscard(1);
        count--;
    }
    if (count == 0)
        return FALSE;

    length = RxPeekByte(0);
    if (count < length + 1)
        return FALSE;

    sPeekMsg.length = length;
    sPeekMsg.type = RxPeekByte(1);
    for (i = 1; i < length; i++)
        sPeekMsg.data[i - 1] = RxPeekByte(i + 1);
    for (; i < MAX_MSG_LENGTH; i++)
        sPeekMsg.data[i - 1] = EOS;
    return TRUE;
}

static void ConsumeMessage(void)
{
    RxDiscard(sPeekMsg.length + 1);
}

static u16 Read16(const u8 *data)
{
    return data[0] | (data[1] << 8);
}

// ---------------------------------------------------------------------------
// Teams

static struct Pokemon *GetSideParty(u8 side)
{
    return side == B_SIDE_PLAYER ? gPlayerParty : gEnemyParty;
}

static u8 GetSideBattler(u8 side)
{
    return GetBattlerAtPosition(side == B_SIDE_PLAYER ? B_POSITION_PLAYER_LEFT : B_POSITION_OPPONENT_LEFT);
}

static u32 StatusToStatus1(u8 status)
{
    switch (status)
    {
    case LV_STATUS_SLP:
        return 2;
    case LV_STATUS_PSN:
        return STATUS1_POISON;
    case LV_STATUS_BRN:
        return STATUS1_BURN;
    case LV_STATUS_FRZ:
        return STATUS1_FREEZE;
    case LV_STATUS_PAR:
        return STATUS1_PARALYSIS;
    case LV_STATUS_TOX:
        return STATUS1_TOXIC_POISON;
    default:
        return 0;
    }
}

static u32 RandomPersonality(void)
{
    return (Random() << 16) | Random();
}

// LVMSG_MON: side, slot, species:16, level, hp:16, maxHP:16, status, flags, nickname
static void ApplyMonMsg(const u8 *data)
{
    u8 side = data[0] & 1;
    u8 slot = data[1];
    u16 species = Read16(&data[2]);
    u8 level = data[4];
    u16 hp = Read16(&data[5]);
    u16 maxHP = Read16(&data[7]);
    u8 status = data[9];
    u8 flags = data[10];
    const u8 *nickname = &data[11];
    struct Pokemon *mon;
    u32 personality, otId, status1;
    u8 name[POKEMON_NAME_LENGTH + 1];
    u16 i;
    u8 wantGender, gender;
    bool8 shiny;

    if (slot >= PARTY_SIZE)
        return;
    mon = &GetSideParty(side)[slot];
    if (species == SPECIES_NONE || species >= NUM_SPECIES)
    {
        ZeroMonData(mon);
        return;
    }
    if (level < MIN_LEVEL)
        level = MIN_LEVEL;
    if (level > MAX_LEVEL)
        level = MAX_LEVEL;

    if (flags & LV_MONFLAG_MALE)
        wantGender = MON_MALE;
    else if (flags & LV_MONFLAG_FEMALE)
        wantGender = MON_FEMALE;
    else
        wantGender = MON_GENDERLESS; // don't care

    // Keep an existing mon (and its personality) when only its HP or status changes.
    personality = GetMonData(mon, MON_DATA_PERSONALITY);
    otId = GetMonData(mon, MON_DATA_OT_ID);
    gender = GetGenderFromSpeciesAndPersonality(species, personality);
    shiny = IsShinyOtIdPersonality(otId, personality);
    if (GetMonData(mon, MON_DATA_SPECIES) != species
     || GetMonData(mon, MON_DATA_LEVEL) != level
     || (wantGender != MON_GENDERLESS && gender != wantGender)
     || shiny != ((flags & LV_MONFLAG_SHINY) != 0))
    {
        personality = RandomPersonality();
        if (wantGender != MON_GENDERLESS)
        {
            for (i = 0; i < 256 && GetGenderFromSpeciesAndPersonality(species, personality) != wantGender; i++)
                personality = RandomPersonality();
        }
        // Shiny when the OT id halves XOR the personality halves to < 8.
        otId = ((personality >> 16) ^ (personality & 0xFFFF)) << 16;
        if (!(flags & LV_MONFLAG_SHINY))
            otId ^= 0xFFFF0000;
        CreateMon(mon, species, level, USE_RANDOM_IVS, TRUE, personality, OT_ID_PRESET, otId);
    }

    if (maxHP == 0)
        maxHP = 1;
    if (hp > maxHP)
        hp = maxHP;
    SetMonData(mon, MON_DATA_MAX_HP, &maxHP);
    SetMonData(mon, MON_DATA_HP, &hp);
    status1 = (hp == 0) ? 0 : StatusToStatus1(status);
    SetMonData(mon, MON_DATA_STATUS, &status1);

    if (nickname[0] == EOS)
        StringCopy(name, gSpeciesNames[species]);
    else
        StringCopyN(name, nickname, POKEMON_NAME_LENGTH);
    name[POKEMON_NAME_LENGTH] = EOS;
    SetMonData(mon, MON_DATA_NICKNAME, name);
}

// LVMSG_TRAINER: side, gender, class, name
static void ApplyTrainerMsg(const u8 *data)
{
    u8 side = data[0] & 1;

    sSetup.genders[side] = data[1] ? FEMALE : MALE;
    if (side == B_SIDE_OPPONENT)
        sSetup.opponentClass = data[2] % 5;
    StringCopyN(sSetup.names[side], &data[3], PLAYER_NAME_LENGTH);
    sSetup.names[side][PLAYER_NAME_LENGTH] = EOS;
}

static void ResetSetup(void)
{
    u8 side;

    ZeroPlayerPartyMons();
    ZeroEnemyPartyMons();
    for (side = 0; side < NUM_BATTLE_SIDES; side++)
    {
        sSetup.names[side][0] = EOS;
        sSetup.genders[side] = MALE;
    }
    sSetup.opponentClass = 4;
}

static void CopyPartyMonToBattleMon(u8 battler, struct Pokemon *mon)
{
    struct BattlePokemon *battleMon = &gBattleMons[battler];
    u16 species = GetMonData(mon, MON_DATA_SPECIES);
    u8 i;

    memset(battleMon, 0, sizeof(*battleMon));
    battleMon->species = species;
    battleMon->attack = GetMonData(mon, MON_DATA_ATK);
    battleMon->defense = GetMonData(mon, MON_DATA_DEF);
    battleMon->speed = GetMonData(mon, MON_DATA_SPEED);
    battleMon->spAttack = GetMonData(mon, MON_DATA_SPATK);
    battleMon->spDefense = GetMonData(mon, MON_DATA_SPDEF);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        battleMon->moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i);
        battleMon->pp[i] = GetMonData(mon, MON_DATA_PP1 + i);
    }
    for (i = 0; i < NUM_BATTLE_STATS; i++)
        battleMon->statStages[i] = DEFAULT_STAT_STAGE;
    battleMon->abilityNum = GetMonData(mon, MON_DATA_ABILITY_NUM);
    battleMon->ability = GetAbilityBySpecies(species, battleMon->abilityNum);
    battleMon->types[0] = gSpeciesInfo[species].types[0];
    battleMon->types[1] = gSpeciesInfo[species].types[1];
    battleMon->hp = GetMonData(mon, MON_DATA_HP);
    battleMon->maxHP = GetMonData(mon, MON_DATA_MAX_HP);
    battleMon->level = GetMonData(mon, MON_DATA_LEVEL);
    battleMon->friendship = GetMonData(mon, MON_DATA_FRIENDSHIP);
    battleMon->personality = GetMonData(mon, MON_DATA_PERSONALITY);
    battleMon->otId = GetMonData(mon, MON_DATA_OT_ID);
    battleMon->status1 = GetMonData(mon, MON_DATA_STATUS);
    GetMonData(mon, MON_DATA_NICKNAME, battleMon->nickname);
    battleMon->nickname[POKEMON_NAME_LENGTH] = EOS;
}

// ---------------------------------------------------------------------------
// Boot and the waiting screen

void CB2_LinkViewer_Boot(void)
{
    SetSaveBlocksPointers(GetSaveBlocksPointersBaseOffset());
    ResetMenuAndMonGlobals();
    Save_ResetSaveCounters();
    LoadGameSave(SAVE_NORMAL);
    if (gSaveFileStatus == SAVE_STATUS_EMPTY || gSaveFileStatus == SAVE_STATUS_CORRUPT)
        Sav2_ClearSetDefault();
    SetPokemonCryStereo(gSaveBlock2Ptr->optionsSound);
    InitHeap(gHeap, HEAP_SIZE);

    if (JOY_HELD(SELECT_BUTTON))
    {
        // Normal game (the save is already loaded).
        SetMainCallback2(CB2_InitCopyrightScreenAfterTitleScreen);
        return;
    }

    gSaveBlock2Ptr->optionsTextSpeed = OPTIONS_TEXT_SPEED_FAST;
    gSaveBlock2Ptr->optionsBattleSceneOff = FALSE;
    gSaveBlock2Ptr->optionsBattleStyle = OPTIONS_BATTLE_STYLE_SET;

    sRxHead = 0;
    sRxTail = 0;
    sConsumed = 0;
    sLinkSeen = FALSE;
    sNeedSync = TRUE;
    ResetSetup();
    gMain.state = 0;
    SetMainCallback2(CB2_LinkViewer_InitWaitScreen);
}

static void VBlankCB_LinkViewer(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

enum {
    STATUS_NO_LINK = 1,
    STATUS_CONNECTED,
    STATUS_RECEIVING,
};

static void PrintWaitScreenStatus(void)
{
    u8 *str = gStringVar4;

    FillWindowPixelBuffer(WIN_STATUS, PIXEL_FILL(1));
    if (sScreenStatus == STATUS_NO_LINK)
    {
        StringCopy(str, sText_WaitingForLink);
    }
    else if (sScreenStatus == STATUS_CONNECTED)
    {
        StringCopy(str, sText_Connected);
    }
    else
    {
        str = StringCopy(str, sText_ReceivingTeams);
        if (sSetup.names[B_SIDE_PLAYER][0] != EOS || sSetup.names[B_SIDE_OPPONENT][0] != EOS)
        {
            *str++ = CHAR_NEWLINE;
            str = StringCopy(str, sSetup.names[B_SIDE_PLAYER]);
            str = StringCopy(str, sText_Vs);
            StringCopy(str, sSetup.names[B_SIDE_OPPONENT]);
        }
    }
    AddTextPrinterParameterized(WIN_STATUS, FONT_NORMAL, gStringVar4, 0, 1, TEXT_SKIP_DRAW, NULL);
    CopyWindowToVram(WIN_STATUS, COPYWIN_FULL);
    sScreenStatusShown = sScreenStatus;
}

static void CB2_LinkViewer_InitWaitScreen(void)
{
    switch (gMain.state)
    {
    case 0:
    default:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_BG0HOFS, 0);
        SetGpuReg(REG_OFFSET_BG0VOFS, 0);
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        ScanlineEffect_Stop();
        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)(PLTT + 2), PLTT_SIZE - 2);
        ResetPaletteFade();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        gPlttBufferUnfaded[0] = RGB(5, 10, 14);
        gPlttBufferFaded[0] = RGB(5, 10, 14);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sWaitScreenBgTemplates, ARRAY_COUNT(sWaitScreenBgTemplates));
        InitWindows(sWaitScreenWindowTemplates);
        DeactivateAllTextPrinters();
        LoadPalette(gStandardMenuPalette, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(1));
        AddTextPrinterParameterized(WIN_TITLE, FONT_NORMAL, sText_Title, 0, 1, TEXT_SKIP_DRAW, NULL);
        PutWindowTilemap(WIN_TITLE);
        CopyWindowToVram(WIN_TITLE, COPYWIN_FULL);
        PutWindowTilemap(WIN_STATUS);
        sScreenStatus = sLinkSeen ? STATUS_CONNECTED : STATUS_NO_LINK;
        PrintWaitScreenStatus();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
        ShowBg(0);
        InitReceiver();
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        EnableInterrupts(INTR_FLAG_VBLANK);
        SetVBlankCallback(VBlankCB_LinkViewer);
        gLinkViewerActive = FALSE;
        gMain.state = 1;
        break;
    case 1:
        UpdatePaletteFade();
        if (!gPaletteFade.active)
            SetMainCallback2(CB2_LinkViewer_WaitScreen);
        break;
    }
}

static void CB2_LinkViewer_WaitScreen(void)
{
    u8 i;

    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    UpdatePaletteFade();

    if (gPaletteFade.active)
        return;

    for (i = 0; i < MAX_MSGS_PER_FRAME && PeekMessage(); i++)
    {
        ConsumeMessage();
        switch (sPeekMsg.type)
        {
        case LVMSG_RESET:
            ResetSetup();
            sScreenStatus = STATUS_CONNECTED;
            break;
        case LVMSG_TRAINER:
            ApplyTrainerMsg(sPeekMsg.data);
            sScreenStatus = STATUS_RECEIVING;
            sScreenStatusShown = 0; // names may have changed
            break;
        case LVMSG_MON:
            ApplyMonMsg(sPeekMsg.data);
            sScreenStatus = STATUS_RECEIVING;
            break;
        case LVMSG_START:
            sSetup.bgm = Read16(&sPeekMsg.data[0]);
            sSetup.environment = sPeekMsg.data[2];
            sSetup.textDelay = sPeekMsg.data[3];
            if (GetMonData(&gPlayerParty[0], MON_DATA_SPECIES) != SPECIES_NONE
             && GetMonData(&gEnemyParty[0], MON_DATA_SPECIES) != SPECIES_NONE)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                SetMainCallback2(CB2_LinkViewer_StartBattle);
                return;
            }
            break;
        default:
            // Battle events that arrive outside a battle are dropped.
            break;
        }
    }

    if (sScreenStatus == STATUS_NO_LINK && sLinkSeen)
        sScreenStatus = STATUS_CONNECTED;
    if (sScreenStatus != sScreenStatusShown)
        PrintWaitScreenStatus();
}

static void CB2_LinkViewer_StartBattle(void)
{
    UpdatePaletteFade();
    if (gPaletteFade.active)
        return;

    StringCopy(gSaveBlock2Ptr->playerName, sSetup.names[B_SIDE_PLAYER]);
    if (gSaveBlock2Ptr->playerName[0] == EOS)
        StringCopy(gSaveBlock2Ptr->playerName, sText_DefaultPlayer);
    gSaveBlock2Ptr->playerGender = sSetup.genders[B_SIDE_PLAYER];

    FreeAllWindowBuffers();
    SetVBlankCallback(NULL);
    gLinkViewerActive = TRUE;
    gBattleTypeFlags = BATTLE_TYPE_TRAINER | BATTLE_TYPE_SECRET_BASE;
    gTrainerBattleOpponent_A = TRAINER_SECRET_BASE;
    gMain.savedCallback = CB2_LinkViewer_InitWaitScreen;
    gMain.callback1 = NULL;
    gMain.state = 0;
    memset(&sBattle, 0, sizeof(sBattle));
    sBattle.textDelay = sSetup.textDelay ? sSetup.textDelay : DEFAULT_TEXT_DELAY;
    sBattle.monOut[B_SIDE_PLAYER] = TRUE;
    sBattle.monOut[B_SIDE_OPPONENT] = TRUE;
    PlayBGM(sSetup.bgm ? sSetup.bgm : MUS_VS_TRAINER);
    SetMainCallback2(CB2_InitBattle);
}

void LinkViewer_SetUpOpponentTrainer(void)
{
    struct SecretBase *base = gBattleResources->secretBase;
    u8 i;

    for (i = 0; i < PLAYER_NAME_LENGTH; i++)
        base->trainerName[i] = sSetup.names[B_SIDE_OPPONENT][i];
    if (base->trainerName[0] == EOS)
        StringCopyN(base->trainerName, sText_DefaultOpponent, PLAYER_NAME_LENGTH);
    base->gender = sSetup.genders[B_SIDE_OPPONENT];
    base->trainerId[0] = sSetup.opponentClass;
    base->language = GAME_LANGUAGE;
}

u8 LinkViewer_GetBattleEnvironment(void)
{
    if (sSetup.environment <= BATTLE_ENVIRONMENT_PLAIN)
        return sSetup.environment;
    return BATTLE_ENVIRONMENT_BUILDING;
}

// ---------------------------------------------------------------------------
// Text

static u8 *AppendName(u8 *dst, const u8 *src, u16 maxLength)
{
    u16 i;

    for (i = 0; i < maxLength && src[i] != EOS; i++)
        *dst++ = src[i];
    *dst = EOS;
    return dst;
}

// Expands LV_PH_* placeholders from src into gDisplayedStringBattle.
static void ExpandViewerText(const u8 *src, u16 srcLength)
{
    u8 *dst = gDisplayedStringBattle;
    u8 *end = gDisplayedStringBattle + sizeof(gDisplayedStringBattle) - 1 - 16;
    u16 i = 0, id;

    while (i < srcLength && src[i] != EOS && dst < end)
    {
        if (src[i] == PLACEHOLDER_BEGIN && i + 1 < srcLength)
        {
            switch (src[i + 1])
            {
            case LV_PH_MOVE:
                id = Read16(&src[i + 2]);
                if (id < MOVES_COUNT)
                    dst = AppendName(dst, gMoveNames[id], MOVE_NAME_LENGTH);
                i += 4;
                continue;
            case LV_PH_ITEM:
                id = Read16(&src[i + 2]);
                dst = AppendName(dst, GetItemName(id), ITEM_NAME_LENGTH);
                i += 4;
                continue;
            case LV_PH_ABILITY:
                id = src[i + 2];
                if (id < ABILITIES_COUNT)
                    dst = AppendName(dst, gAbilityNames[id], ABILITY_NAME_LENGTH);
                i += 3;
                continue;
            case LV_PH_SPECIES:
                id = Read16(&src[i + 2]);
                if (id < NUM_SPECIES)
                    dst = AppendName(dst, gSpeciesNames[id], POKEMON_NAME_LENGTH);
                i += 4;
                continue;
            }
        }
        *dst++ = src[i++];
    }
    *dst = EOS;
}

static u16 GetWordWidth(const u8 *word, u16 length)
{
    u8 buffer[32];
    u16 i;

    if (length >= ARRAY_COUNT(buffer))
        length = ARRAY_COUNT(buffer) - 1;
    for (i = 0; i < length; i++)
        buffer[i] = word[i];
    buffer[i] = EOS;
    return GetStringWidth(FONT_NORMAL, buffer, 0);
}

// Breaks gDisplayedStringBattle into lines that fit the message box. The
// second line follows a newline; later lines scroll in (autoScroll is on).
static void WrapViewerText(void)
{
    u8 *str = gDisplayedStringBattle;
    u16 spaceWidth = GetWordWidth(gText_Space2, 1);
    u16 lineWidth = 0, wordWidth, wordStart, wordLength, i = 0;
    u8 line = 0;
    s16 lastSpace = -1;

    while (str[i] != EOS)
    {
        if (str[i] == CHAR_NEWLINE || str[i] == CHAR_PROMPT_SCROLL || str[i] == CHAR_PROMPT_CLEAR)
        {
            if (str[i] == CHAR_NEWLINE && line > 0)
                str[i] = CHAR_PROMPT_SCROLL;
            line++;
            lineWidth = 0;
            lastSpace = -1;
            i++;
            continue;
        }
        if (str[i] == CHAR_SPACE)
        {
            lastSpace = i;
            i++;
            continue;
        }

        wordStart = i;
        while (str[i] != EOS && str[i] != CHAR_SPACE && str[i] != CHAR_NEWLINE
            && str[i] != CHAR_PROMPT_SCROLL && str[i] != CHAR_PROMPT_CLEAR)
            i++;
        wordLength = i - wordStart;
        wordWidth = GetWordWidth(&str[wordStart], wordLength);

        if (lineWidth != 0 && lastSpace >= 0 && lineWidth + spaceWidth + wordWidth > TEXT_WRAP_WIDTH)
        {
            str[lastSpace] = (line == 0) ? CHAR_NEWLINE : CHAR_PROMPT_SCROLL;
            line++;
            lineWidth = wordWidth;
        }
        else
        {
            if (lineWidth != 0)
                lineWidth += spaceWidth;
            lineWidth += wordWidth;
        }
        lastSpace = -1;
    }
}

// ---------------------------------------------------------------------------
// Battle events. Each op runs while the battle controllers are idle and
// returns TRUE once its event has been shown.

static void EmitDone(u8 battler)
{
    MarkBattlerForControllerExec(battler);
}

static bool8 Op_Text(void)
{
    switch (sBattle.stage)
    {
    case 0:
        ExpandViewerText(sBattle.msg.data, sBattle.msg.length - 1);
        WrapViewerText();
        BattlePutTextOnWindow(gDisplayedStringBattle, B_WIN_MSG); // auto-scrolls in viewer battles
        sBattle.stage++;
        return FALSE;
    case 1:
        if (IsTextPrinterActive(B_WIN_MSG))
            return FALSE;
        sBattle.timer = sBattle.textDelay;
        sBattle.stage++;
        // fall through
    default:
        if (sBattle.timer != 0)
        {
            sBattle.timer--;
            return FALSE;
        }
        return TRUE;
    }
}

static bool8 Op_Wait(void)
{
    if (sBattle.stage == 0)
    {
        sBattle.timer = Read16(&sBattle.msg.data[0]);
        sBattle.stage++;
    }
    if (sBattle.timer != 0)
    {
        sBattle.timer--;
        return FALSE;
    }
    return TRUE;
}

// attackerSide, targetSide, move:16, animTurn
static bool8 Op_Move(void)
{
    u8 attacker = GetSideBattler(sBattle.msg.data[0] & 1);
    u8 target = GetSideBattler(sBattle.msg.data[1] & 1);
    u16 move = Read16(&sBattle.msg.data[2]);

    if (sBattle.stage != 0)
        return TRUE;
    if (move == MOVE_NONE || move >= MOVES_COUNT || !sBattle.monOut[GetBattlerSide(attacker)])
        return TRUE;

    gBattlerAttacker = attacker;
    gBattlerTarget = target;
    gCurrentMove = move;
    gActiveBattler = attacker;
    BtlController_EmitMoveAnimation(B_COMM_TO_CONTROLLER, move, sBattle.msg.data[4], gBattleMoves[move].power,
                                    0, gBattleMons[attacker].friendship, &gDisableStructs[attacker], 0);
    EmitDone(attacker);
    sBattle.stage++;
    return FALSE;
}

// side, effectiveness
static bool8 Op_Hit(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    u16 song;

    if (sBattle.stage != 0 || !sBattle.monOut[side])
        return TRUE;

    switch (sBattle.msg.data[1])
    {
    case LV_EFFECT_SUPER:
        song = SE_SUPER_EFFECTIVE;
        break;
    case LV_EFFECT_NOT_VERY:
        song = SE_NOT_EFFECTIVE;
        break;
    case LV_EFFECT_SILENT:
        song = 0;
        break;
    default:
        song = SE_EFFECTIVE;
        break;
    }
    if (song != 0)
        PlaySE12WithPanning(song, side == B_SIDE_PLAYER ? SOUND_PAN_ATTACKER : SOUND_PAN_TARGET);

    gActiveBattler = battler;
    BtlController_EmitHitAnimation(B_COMM_TO_CONTROLLER);
    EmitDone(battler);
    sBattle.stage++;
    return FALSE;
}

// side, hp:16
static bool8 Op_HP(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    struct Pokemon *mon = &GetSideParty(side)[gBattlerPartyIndexes[battler]];
    u16 hp = Read16(&sBattle.msg.data[1]);
    u16 maxHP = GetMonData(mon, MON_DATA_MAX_HP);
    s16 delta;

    if (hp > maxHP)
        hp = maxHP;

    switch (sBattle.stage)
    {
    case 0:
        delta = (s16)GetMonData(mon, MON_DATA_HP) - (s16)hp;
        if (delta == 0 || !sBattle.monOut[side])
        {
            sBattle.stage = 1;
            return FALSE;
        }
        gActiveBattler = battler;
        BtlController_EmitHealthBarUpdate(B_COMM_TO_CONTROLLER, delta);
        EmitDone(battler);
        sBattle.stage++;
        return FALSE;
    default:
        SetMonData(mon, MON_DATA_HP, &hp);
        if (sBattle.monOut[side])
            gBattleMons[battler].hp = hp;
        return TRUE;
    }
}

static void StartReturnToBall(u8 side)
{
    u8 battler = GetSideBattler(side);

    gActiveBattler = battler;
    BtlController_EmitReturnMonToBall(B_COMM_TO_CONTROLLER, FALSE);
    EmitDone(battler);
    sBattle.monOut[side] = FALSE;
    gBattleMons[battler].status2 = 0;
}

// side
static bool8 Op_SwitchOut(void)
{
    u8 side = sBattle.msg.data[0] & 1;

    if (sBattle.stage != 0 || !sBattle.monOut[side])
        return TRUE;
    StartReturnToBall(side);
    sBattle.stage++;
    return FALSE;
}

// side, slot
static bool8 Op_SwitchIn(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 slot = sBattle.msg.data[1];
    u8 battler = GetSideBattler(side);
    struct Pokemon *mon;

    if (slot >= PARTY_SIZE)
        return TRUE;
    mon = &GetSideParty(side)[slot];

    switch (sBattle.stage)
    {
    case 0:
        sBattle.stage++;
        if (sBattle.monOut[side])
        {
            StartReturnToBall(side);
            return FALSE;
        }
        // fall through
    case 1:
        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE)
            return TRUE;
        gBattlerPartyIndexes[battler] = slot;
        CopyPartyMonToBattleMon(battler, mon);
        memset(&gDisableStructs[battler], 0, sizeof(gDisableStructs[battler]));
        gActiveBattler = battler;
        BtlController_EmitSwitchInAnim(B_COMM_TO_CONTROLLER, slot, FALSE);
        EmitDone(battler);
        sBattle.monOut[side] = TRUE;
        sBattle.stage = 2;
        return FALSE;
    default:
        return TRUE;
    }
}

// side
static bool8 Op_Faint(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    struct Pokemon *mon = &GetSideParty(side)[gBattlerPartyIndexes[battler]];
    u16 hp = 0;
    u32 status = 0;

    switch (sBattle.stage)
    {
    case 0:
        if (!sBattle.monOut[side])
            return TRUE;
        gActiveBattler = battler;
        BtlController_EmitFaintingCry(B_COMM_TO_CONTROLLER);
        EmitDone(battler);
        sBattle.stage++;
        return FALSE;
    case 1:
        gActiveBattler = battler;
        BtlController_EmitFaintAnimation(B_COMM_TO_CONTROLLER);
        EmitDone(battler);
        sBattle.stage++;
        return FALSE;
    default:
        SetMonData(mon, MON_DATA_HP, &hp);
        SetMonData(mon, MON_DATA_STATUS, &status);
        gBattleMons[battler].hp = 0;
        gBattleMons[battler].status2 = 0;
        sBattle.monOut[side] = FALSE;
        return TRUE;
    }
}

// side, status, playAnim
static bool8 Op_Status(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    struct Pokemon *mon = &GetSideParty(side)[gBattlerPartyIndexes[battler]];
    u32 status1 = StatusToStatus1(sBattle.msg.data[1]);

    switch (sBattle.stage)
    {
    case 0:
        SetMonData(mon, MON_DATA_STATUS, &status1);
        if (!sBattle.monOut[side])
            return TRUE;
        gBattleMons[battler].status1 = status1;
        sBattle.stage++;
        if (status1 != 0 && sBattle.msg.data[2])
        {
            gActiveBattler = battler;
            BtlController_EmitStatusAnimation(B_COMM_TO_CONTROLLER, FALSE, status1);
            EmitDone(battler);
            return FALSE;
        }
        // fall through
    case 1:
        gActiveBattler = battler;
        BtlController_EmitStatusIconUpdate(B_COMM_TO_CONTROLLER, status1, gBattleMons[battler].status2);
        EmitDone(battler);
        sBattle.stage = 2;
        return FALSE;
    default:
        return TRUE;
    }
}

// side, which
static bool8 Op_StatusAnim(void)
{
    static const u32 sVolatiles[] = {
        [LV_VOLATILE_CONFUSION] = STATUS2_CONFUSION,
        [LV_VOLATILE_ATTRACT]   = STATUS2_INFATUATION,
        [LV_VOLATILE_CURSE]     = STATUS2_CURSED,
        [LV_VOLATILE_NIGHTMARE] = STATUS2_NIGHTMARE,
    };
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    u8 which = sBattle.msg.data[1];

    if (sBattle.stage != 0 || !sBattle.monOut[side] || which >= ARRAY_COUNT(sVolatiles))
        return TRUE;
    gActiveBattler = battler;
    BtlController_EmitStatusAnimation(B_COMM_TO_CONTROLLER, TRUE, sVolatiles[which]);
    EmitDone(battler);
    sBattle.stage++;
    return FALSE;
}

// side, stat, delta
static bool8 Op_StatChange(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);
    u8 stat = sBattle.msg.data[1];
    s8 delta = (s8)sBattle.msg.data[2];
    s8 stage;
    u16 animArg;

    if (sBattle.stage != 0 || !sBattle.monOut[side] || stat < STAT_ATK || stat > STAT_EVASION || delta == 0)
        return TRUE;

    stage = gBattleMons[battler].statStages[stat] + delta;
    if (stage < MIN_STAT_STAGE)
        stage = MIN_STAT_STAGE;
    if (stage > MAX_STAT_STAGE)
        stage = MAX_STAT_STAGE;
    gBattleMons[battler].statStages[stat] = stage;

    if (delta > 0)
        animArg = (delta >= 2 ? STAT_ANIM_PLUS2 : STAT_ANIM_PLUS1) + stat - 1;
    else
        animArg = (delta <= -2 ? STAT_ANIM_MINUS2 : STAT_ANIM_MINUS1) + stat - 1;

    gActiveBattler = battler;
    BtlController_EmitBattleAnimation(B_COMM_TO_CONTROLLER, B_ANIM_STATS_CHANGE, animArg);
    EmitDone(battler);
    sBattle.stage++;
    return FALSE;
}

// side, animId, arg:16
static bool8 Op_Anim(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);

    if (sBattle.stage != 0 || !sBattle.monOut[side])
        return TRUE;
    gBattlerAttacker = battler;
    gBattlerTarget = battler;
    gActiveBattler = battler;
    BtlController_EmitBattleAnimation(B_COMM_TO_CONTROLLER, sBattle.msg.data[1], Read16(&sBattle.msg.data[2]));
    EmitDone(battler);
    sBattle.stage++;
    return FALSE;
}

// weather, playAnim
static bool8 Op_Weather(void)
{
    static const u16 sWeatherFlags[] = {
        [LV_WEATHER_NONE] = 0,
        [LV_WEATHER_RAIN] = B_WEATHER_RAIN_TEMPORARY,
        [LV_WEATHER_SUN]  = B_WEATHER_SUN_TEMPORARY,
        [LV_WEATHER_SAND] = B_WEATHER_SANDSTORM_TEMPORARY,
        [LV_WEATHER_HAIL] = B_WEATHER_HAIL_TEMPORARY,
    };
    static const u8 sWeatherAnims[] = {
        [LV_WEATHER_NONE] = 0,
        [LV_WEATHER_RAIN] = B_ANIM_RAIN_CONTINUES,
        [LV_WEATHER_SUN]  = B_ANIM_SUN_CONTINUES,
        [LV_WEATHER_SAND] = B_ANIM_SANDSTORM_CONTINUES,
        [LV_WEATHER_HAIL] = B_ANIM_HAIL_CONTINUES,
    };
    u8 weather = sBattle.msg.data[0];
    u8 battler;

    if (sBattle.stage != 0 || weather >= ARRAY_COUNT(sWeatherFlags))
        return TRUE;
    gBattleWeather = sWeatherFlags[weather];
    if (weather == LV_WEATHER_NONE || !sBattle.msg.data[1])
        return TRUE;

    battler = GetSideBattler(B_SIDE_PLAYER);
    gActiveBattler = battler;
    gBattlerAttacker = battler;
    gBattlerTarget = battler;
    BtlController_EmitBattleAnimation(B_COMM_TO_CONTROLLER, sWeatherAnims[weather], 0);
    EmitDone(battler);
    sBattle.stage++;
    return FALSE;
}

static bool8 Op_SE(void)
{
    PlaySE(Read16(&sBattle.msg.data[0]));
    return TRUE;
}

static bool8 Op_Cry(void)
{
    u8 side = sBattle.msg.data[0] & 1;
    u8 battler = GetSideBattler(side);

    if (sBattle.monOut[side])
        PlayCry_Normal(gBattleMons[battler].species, side == B_SIDE_PLAYER ? -25 : 25);
    return TRUE;
}

// Leaves the battle: winner side (2 = tie), or 0xFF to leave at once.
static bool8 Op_End(void)
{
    u8 winner = sBattle.msg.data[0];

    switch (sBattle.stage)
    {
    case 0:
        if (winner == 0xFF)
        {
            sBattle.stage = 2;
            return FALSE;
        }
        if (winner == B_SIDE_PLAYER)
            PlayBGM(MUS_VICTORY_TRAINER);
        else
            FadeOutBGM(4);
        sBattle.timer = END_DELAY;
        sBattle.stage++;
        return FALSE;
    case 1:
        if (sBattle.timer != 0 && !JOY_NEW(A_BUTTON | B_BUTTON))
        {
            sBattle.timer--;
            return FALSE;
        }
        sBattle.stage++;
        return FALSE;
    case 2:
        sBattle.ending = TRUE;
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_QuitRecordedBattle);
        sBattle.stage++;
        return FALSE;
    default:
        return FALSE; // stays here until the battle is freed
    }
}

static bool8 (*GetOp(u8 type))(void)
{
    switch (type)
    {
    case LVMSG_TEXT:        return Op_Text;
    case LVMSG_MOVE:        return Op_Move;
    case LVMSG_HIT:         return Op_Hit;
    case LVMSG_HP:          return Op_HP;
    case LVMSG_SWITCH_OUT:  return Op_SwitchOut;
    case LVMSG_SWITCH_IN:   return Op_SwitchIn;
    case LVMSG_FAINT:       return Op_Faint;
    case LVMSG_STATUS:      return Op_Status;
    case LVMSG_STAT_CHANGE: return Op_StatChange;
    case LVMSG_ANIM:        return Op_Anim;
    case LVMSG_END:         return Op_End;
    case LVMSG_WAIT:        return Op_Wait;
    case LVMSG_WEATHER:     return Op_Weather;
    case LVMSG_SE:          return Op_SE;
    case LVMSG_STATUS_ANIM: return Op_StatusAnim;
    case LVMSG_CRY:         return Op_Cry;
    default:                return NULL;
    }
}

static void StartOp(bool8 (*op)(void))
{
    sBattle.msg = sPeekMsg;
    sBattle.op = op;
    sBattle.stage = 0;
}

void LinkViewer_BattleMainFunc(void)
{
    u8 i;
    bool8 (*op)(void);

    if (gBattleControllerExecFlags)
        return;

    for (i = 0; i < MAX_MSGS_PER_FRAME; i++)
    {
        if (sBattle.op != NULL)
        {
            if (!sBattle.op())
                break;
            sBattle.op = NULL;
            if (gBattleControllerExecFlags)
                break;
        }
        if (sBattle.ending || !PeekMessage())
            break;

        switch (sPeekMsg.type)
        {
        case LVMSG_START:
            // A new battle: leave this one and let the waiting screen start it.
            sPeekMsg.data[0] = 0xFF;
            StartOp(Op_End);
            break;
        case LVMSG_RESET:
            ConsumeMessage();
            ResetSetup();
            sPeekMsg.data[0] = 0xFF;
            StartOp(Op_End);
            break;
        case LVMSG_TRAINER:
            ConsumeMessage();
            ApplyTrainerMsg(sPeekMsg.data);
            break;
        case LVMSG_MON:
            ConsumeMessage();
            ApplyMonMsg(sPeekMsg.data);
            break;
        default:
            ConsumeMessage();
            op = GetOp(sPeekMsg.type);
            if (op != NULL)
                StartOp(op);
            break;
        }
    }
}
