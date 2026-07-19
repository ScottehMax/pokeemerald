#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#include "global.h"
#include "battle.h"
#include "battle_setup.h"
#include "field_screen_effect.h"
#include "main.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "pc_platform.h"
#include "pc_ppu.h"
#include "pc_services.h"
#include "pc_shared.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/pokedex.h"
#include "constants/pokemon.h"
#include "constants/species.h"

#define NS_PER_FRAME 16742706ull
#define GBA_CLOCK_HZ 16777216ull
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_TEST_INPUT_EVENTS 32

enum PcTestBattleState
{
    PC_TEST_BATTLE_DISABLED,
    PC_TEST_BATTLE_WAITING,
    PC_TEST_BATTLE_REQUESTED,
    PC_TEST_BATTLE_ENTERED,
    PC_TEST_BATTLE_RETURNED,
};

extern struct MusicPlayerInfo *gMPlay_PokemonCry;

struct PcDmaChannel
{
    uintptr_t source;
    uintptr_t destination;
    uintptr_t initialDestination;
    u32 count;
    u16 control;
    bool8 enabled;
};

struct PcTestInputEvent
{
    u32 frame;
    u16 keys;
    u16 duration;
    u32 period;
    u32 repetitions;
};

static struct PcSharedState *sShared;
#ifdef _WIN32
static HANDLE sSharedMapping;
#endif
static struct PcDmaChannel sDma[4];
static u64 sNextFrameTime;
static bool8 sFastForward;
static struct PcTestInputEvent sTestInputEvents[MAX_TEST_INPUT_EVENTS];
static u32 sTestInputEventCount;
static u32 sFrameCounter;
static u32 sTestBattleFrame;
static enum PcTestBattleState sTestBattleState;
static u32 sTestCenterWarpFrame;
static bool8 sTestCenterWarpPending;
static u32 sTestStorageFrame;
static bool8 sTestStoragePending;
static u32 sTestPokedexFrame;
static bool8 sTestPokedexPending;
static u64 sTimer1StartNs;
static bool8 sTestTrainerIdReportPending;

static u64 GetMonotonicNs(void)
{
#ifdef _WIN32
    static LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&now);
    return (u64)(now.QuadPart / frequency.QuadPart) * 1000000000ull
         + (u64)(now.QuadPart % frequency.QuadPart) * 1000000000ull / frequency.QuadPart;
#else
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (u64)now.tv_sec * 1000000000ull + now.tv_nsec;
#endif
}

static void SleepUntil(u64 target)
{
#ifdef _WIN32
    u64 now;

    while ((now = GetMonotonicNs()) < target)
    {
        u64 remaining = target - now;

        if (remaining > 2000000ull)
            Sleep((DWORD)(remaining / 1000000ull - 1));
        else
            SwitchToThread();
    }
#else
    struct timespec delay;
    u64 now = GetMonotonicNs();

    if (now >= target)
        return;

    delay.tv_sec = (time_t)((target - now) / 1000000000ull);
    delay.tv_nsec = (long)((target - now) % 1000000000ull);
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR)
        ;
#endif
}

static bool32 MapGbaRegion(uintptr_t address, size_t size)
{
#ifdef _WIN32
    void *mapping = VirtualAlloc((void *)address,
                                 size,
                                 MEM_RESERVE | MEM_COMMIT,
                                 PAGE_READWRITE);
#else
    void *mapping = mmap((void *)address,
                         size,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                         -1,
                         0);
#endif

#ifdef _WIN32
    return mapping != NULL && (uintptr_t)mapping == address;
#else
    return mapping != MAP_FAILED && (uintptr_t)mapping == address;
#endif
}

static bool32 MapGbaMemory(void)
{
    static const struct
    {
        uintptr_t address;
        size_t size;
#ifdef _WIN32
        const char *name;
#endif
    } regions[] =
    {
        {EWRAM_START, 0x40000,
#ifdef _WIN32
         "EWRAM"
#endif
        },
        {IWRAM_START, 0x8000,
#ifdef _WIN32
         "IWRAM"
#endif
        },
        {REG_BASE, 0x1000,
#ifdef _WIN32
         "I/O registers"
#endif
        },
        {PLTT, 0x1000,
#ifdef _WIN32
         "palette RAM"
#endif
        },
        {VRAM, VRAM_SIZE,
#ifdef _WIN32
         "VRAM"
#endif
        },
        {OAM, 0x1000,
#ifdef _WIN32
         "OAM"
#endif
        },
    };
    size_t i;

    for (i = 0; i < ARRAY_SIZE(regions); i++)
    {
        if (!MapGbaRegion(regions[i].address, regions[i].size))
        {
#ifdef _WIN32
            DWORD error = GetLastError();
            MEMORY_BASIC_INFORMATION memory;

            if (VirtualQuery((void *)regions[i].address, &memory, sizeof(memory)) != 0
             && memory.State != MEM_FREE)
            {
                fprintf(stderr,
                        "could not reserve %s at 0x%08lx: Windows error %lu; "
                        "0x%08lx-0x%08lx is already in use (state 0x%lx, type 0x%lx)\n",
                        regions[i].name,
                        (unsigned long)regions[i].address,
                        (unsigned long)error,
                        (unsigned long)(uintptr_t)memory.BaseAddress,
                        (unsigned long)((uintptr_t)memory.BaseAddress + memory.RegionSize),
                        (unsigned long)memory.State,
                        (unsigned long)memory.Type);
            }
            else
            {
                fprintf(stderr,
                        "could not reserve %s at 0x%08lx: Windows error %lu\n",
                        regions[i].name,
                        (unsigned long)regions[i].address,
                        (unsigned long)error);
            }
#endif
            return FALSE;
        }
    }

    return TRUE;
}

static void ParseTestInputEvents(const char *spec)
{
    const char *cursor = spec;

    sTestInputEventCount = 0;
    while (cursor != NULL && *cursor != '\0' && sTestInputEventCount < ARRAY_SIZE(sTestInputEvents))
    {
        struct PcTestInputEvent *event = &sTestInputEvents[sTestInputEventCount];
        char *end;
        unsigned long frame = strtoul(cursor, &end, 0);
        unsigned long keys;
        unsigned long duration = 2;
        unsigned long period = 0;
        unsigned long repetitions = 1;

        if (end == cursor || *end != ':')
            break;
        cursor = end + 1;
        keys = strtoul(cursor, &end, 0);
        if (end == cursor)
            break;
        if (*end == ':')
        {
            cursor = end + 1;
            duration = strtoul(cursor, &end, 0);
            if (end == cursor)
                break;
        }
        if (*end == ':')
        {
            cursor = end + 1;
            period = strtoul(cursor, &end, 0);
            if (end == cursor || *end != ':')
                break;
            cursor = end + 1;
            repetitions = strtoul(cursor, &end, 0);
            if (end == cursor)
                break;
        }
        if ((*end != '\0' && *end != ',')
         || frame > UINT32_MAX
         || keys > KEYS_MASK
         || duration == 0
         || duration > UINT16_MAX
         || repetitions == 0
         || repetitions > UINT32_MAX
         || (period != 0 && (period < duration || period > UINT32_MAX))
         || (u64)frame + (u64)period * (repetitions - 1) + duration > UINT32_MAX)
            break;

        event->frame = (u32)frame;
        event->keys = (u16)keys;
        event->duration = (u16)duration;
        event->period = (u32)period;
        event->repetitions = (u32)repetitions;
        sTestInputEventCount++;
        cursor = *end == ',' ? end + 1 : end;
    }
}

static void ParseTestBattleFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestBattleFrame = UINT32_MAX;
    sTestBattleState = PC_TEST_BATTLE_DISABLED;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_BATTLE_AT value: %s\n", spec);
        return;
    }

    sTestBattleFrame = (u32)frame;
    sTestBattleState = PC_TEST_BATTLE_WAITING;
}

static void ParseTestCenterWarpFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestCenterWarpFrame = UINT32_MAX;
    sTestCenterWarpPending = FALSE;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_CENTER_AT value: %s\n", spec);
        return;
    }

    sTestCenterWarpFrame = (u32)frame;
    sTestCenterWarpPending = TRUE;
}

static void ParseTestStorageFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestStorageFrame = UINT32_MAX;
    sTestStoragePending = FALSE;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_STORAGE_AT value: %s\n", spec);
        return;
    }

    sTestStorageFrame = (u32)frame;
    sTestStoragePending = TRUE;
}

static void ParseTestPokedexFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestPokedexFrame = UINT32_MAX;
    sTestPokedexPending = FALSE;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_POKEDEX_AT value: %s\n", spec);
        return;
    }

    sTestPokedexFrame = (u32)frame;
    sTestPokedexPending = TRUE;
}

bool32 PcPlatformInit(const char *sharedPath)
{
    const char *testInput;
#ifndef _WIN32
    struct stat info;
    int fd;
#endif

    if (sizeof(void *) != 4)
    {
        fprintf(stderr, "pokeemerald-core must use a 32-bit ABI\n");
        return FALSE;
    }

    // Windows chooses addresses for MapViewOfFile automatically. Claim the
    // hardware ranges before opening the frontend mapping so it cannot land on
    // EWRAM, IWRAM, registers, palette RAM, VRAM, or OAM.
    if (!MapGbaMemory())
    {
#ifdef _WIN32
        fprintf(stderr, "could not reserve the GBA memory map\n");
#else
        fprintf(stderr, "could not reserve the GBA memory map: %s\n", strerror(errno));
#endif
        return FALSE;
    }

#ifdef _WIN32
    sSharedMapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, sharedPath);
    if (sSharedMapping == NULL)
    {
        fprintf(stderr, "could not open SDL frontend shared memory: Windows error %lu\n", GetLastError());
        return FALSE;
    }
    sShared = MapViewOfFile(sSharedMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*sShared));
    if (sShared == NULL)
    {
        fprintf(stderr, "could not map SDL frontend shared memory: Windows error %lu\n", GetLastError());
        CloseHandle(sSharedMapping);
        sSharedMapping = NULL;
        return FALSE;
    }
#else
    fd = open(sharedPath, O_RDWR);
    if (fd < 0 || fstat(fd, &info) != 0 || info.st_size < (off_t)sizeof(*sShared))
    {
        if (fd >= 0)
            close(fd);
        fprintf(stderr, "could not open SDL frontend shared memory: %s\n", strerror(errno));
        return FALSE;
    }

    sShared = mmap(NULL, sizeof(*sShared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (sShared == MAP_FAILED)
    {
        sShared = NULL;
        fprintf(stderr, "could not map SDL frontend shared memory: %s\n", strerror(errno));
        return FALSE;
    }
#endif
    if (sShared->magic != PC_SHARED_MAGIC)
    {
        fprintf(stderr, "invalid SDL frontend shared memory\n");
        PcPlatformShutdown();
        return FALSE;
    }

    if (!PcServicesInit(sShared->savePath))
    {
        sShared->coreError = 2;
        return FALSE;
    }

    REG_KEYINPUT = KEYS_MASK;
    // Game callbacks run during VBlank. Line 161 also makes SetGpuReg apply
    // immediately, matching the register ordering relied on during boot.
    REG_VCOUNT = 161;
    sFastForward = getenv("POKEEMERALD_PC_FAST") != NULL;
    testInput = getenv("POKEEMERALD_PC_TEST_INPUT");
    ParseTestInputEvents(testInput);
    ParseTestBattleFrame(getenv("POKEEMERALD_PC_TEST_BATTLE_AT"));
    ParseTestCenterWarpFrame(getenv("POKEEMERALD_PC_TEST_CENTER_AT"));
    ParseTestStorageFrame(getenv("POKEEMERALD_PC_TEST_STORAGE_AT"));
    ParseTestPokedexFrame(getenv("POKEEMERALD_PC_TEST_POKEDEX_AT"));
    sTestTrainerIdReportPending = getenv("POKEEMERALD_PC_TEST_REPORT_ID") != NULL;
    sFrameCounter = 0;
    sTimer1StartNs = 0;
    sNextFrameTime = GetMonotonicNs();
    __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
    __atomic_store_n(&sShared->testBattleOutcome, 0, __ATOMIC_RELEASE);
    __atomic_store_n(&sShared->coreReady, 1, __ATOMIC_RELEASE);
    return TRUE;
}

void PcPlatformShutdown(void)
{
    PcServicesShutdown();
    if (sShared != NULL)
    {
        __atomic_store_n(&sShared->coreReady, 0, __ATOMIC_RELEASE);
#ifdef _WIN32
        UnmapViewOfFile(sShared);
#else
        munmap(sShared, sizeof(*sShared));
#endif
        sShared = NULL;
    }
#ifdef _WIN32
    if (sSharedMapping != NULL)
    {
        CloseHandle(sSharedMapping);
        sSharedMapping = NULL;
    }
#endif
}

void PcPlatformWaitForFrame(void)
{
    u64 now;
    u32 keys;
    u32 i;

    sNextFrameTime += NS_PER_FRAME;
    if (!sFastForward)
        SleepUntil(sNextFrameTime);
    now = GetMonotonicNs();
    if (now > sNextFrameTime + NS_PER_FRAME * 4)
        sNextFrameTime = now;

    if (__atomic_load_n(&sShared->quit, __ATOMIC_ACQUIRE))
    {
        PcPlatformShutdown();
        exit(0);
    }

    keys = __atomic_load_n(&sShared->keys, __ATOMIC_ACQUIRE) & KEYS_MASK;
    for (i = 0; i < sTestInputEventCount; i++)
    {
        const struct PcTestInputEvent *event = &sTestInputEvents[i];
        u32 elapsed;

        if (sFrameCounter < event->frame)
            continue;
        elapsed = sFrameCounter - event->frame;
        if (event->period == 0)
        {
            if (elapsed < event->duration)
                keys |= event->keys;
        }
        else if (elapsed / event->period < event->repetitions
              && elapsed % event->period < event->duration)
            keys |= event->keys;
    }
    sFrameCounter++;
    REG_KEYINPUT = (u16)(KEYS_MASK & ~keys);
    PcDmaRunVBlank();
}

void PcPlatformStartTimer1(void)
{
    REG_TM1CNT_L = 0;
    REG_TM1CNT_H = TIMER_ENABLE;
    sTimer1StartNs = GetMonotonicNs();
}

u16 PcPlatformStopTimer1(void)
{
    u64 elapsedNs = GetMonotonicNs() - sTimer1StartNs;
    u64 cycles = (elapsedNs / 1000000000ull) * GBA_CLOCK_HZ
               + (elapsedNs % 1000000000ull) * GBA_CLOCK_HZ / 1000000000ull;

    REG_TM1CNT_L = (u16)cycles;
    REG_TM1CNT_H = 0;
    return REG_TM1CNT_L;
}

void PcPlatformPresentFrame(PcInterruptCallback hblankCallback)
{
    u32 buffer = (__atomic_load_n(&sShared->frameBufferIndex, __ATOMIC_RELAXED) + 1)
               % PC_FRAME_BUFFER_COUNT;

    PcPpuRender(sShared->pixels[buffer], hblankCallback);
    __atomic_store_n(&sShared->frameBufferIndex, buffer, __ATOMIC_RELEASE);
    __atomic_add_fetch(&sShared->frameSequence, 1, __ATOMIC_RELEASE);
}

void PcPlatformQueueAudio(const s16 *samples, u32 frameCount)
{
    u32 generated = frameCount;
    u32 read = __atomic_load_n(&sShared->audioRead, __ATOMIC_ACQUIRE);
    u32 write = __atomic_load_n(&sShared->audioWrite, __ATOMIC_RELAXED);
    u32 available = PC_AUDIO_BUFFER_FRAMES - (write - read);
    u32 nonzero = 0;
    u32 clipped = 0;
    u32 i;

    for (i = 0; i < generated * 2; i++)
    {
        s32 value = samples[i];
        u32 amplitude = value < 0 ? (u32)-value : (u32)value;

        if (amplitude != 0)
            nonzero++;
        if (amplitude >= 32767)
            clipped++;
        if (amplitude > __atomic_load_n(&sShared->audioPeak, __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->audioPeak, amplitude, __ATOMIC_RELAXED);
    }
    __atomic_add_fetch(&sShared->audioFramesGenerated, generated, __ATOMIC_RELEASE);
    __atomic_add_fetch(&sShared->audioSamplesNonzero, nonzero, __ATOMIC_RELAXED);
    __atomic_add_fetch(&sShared->audioSamplesClipped, clipped, __ATOMIC_RELAXED);

    if (frameCount > available)
        frameCount = available;

    for (i = 0; i < frameCount; i++)
    {
        u32 index = (write + i) & (PC_AUDIO_BUFFER_FRAMES - 1);
        sShared->audio[index * 2] = samples[i * 2];
        sShared->audio[index * 2 + 1] = samples[i * 2 + 1];
    }
    __atomic_store_n(&sShared->audioWrite, write + frameCount, __ATOMIC_RELEASE);
}

void PcPlatformRunTestHooks(void)
{
    if (sTestTrainerIdReportPending
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        u32 trainerId = (u32)gSaveBlock2Ptr->playerTrainerId[0]
                      | (u32)gSaveBlock2Ptr->playerTrainerId[1] << 8
                      | (u32)gSaveBlock2Ptr->playerTrainerId[2] << 16
                      | (u32)gSaveBlock2Ptr->playerTrainerId[3] << 24;

        fprintf(stderr, "PC trainer ID test: visible=%05u full=%08x at frame %u\n",
                trainerId & 0xFFFF,
                trainerId,
                sFrameCounter);
        sTestTrainerIdReportPending = FALSE;
    }

    if (sTestPokedexPending
     && sFrameCounter >= sTestPokedexFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_SEEN);
        GetSetPokedexFlag(NATIONAL_DEX_TREECKO, FLAG_SET_CAUGHT);
        gMPlay_PokemonCry = NULL;
        CleanupOverworldWindowsAndTilemaps();
        SetMainCallback2(CB2_OpenPokedex);
        sTestPokedexPending = FALSE;
        fprintf(stderr, "PC Pokedex test: menu requested at frame %u\n", sFrameCounter);
    }

    if (sTestStoragePending
     && sFrameCounter >= sTestStorageFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        if (!CheckBoxMonSanityAt(0, 0))
            CreateBoxMonAt(0, 0, SPECIES_TREECKO, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        PcStorageTestEnterMoveMons();
        sTestStoragePending = FALSE;
        fprintf(stderr, "PC storage test: menu requested at frame %u\n", sFrameCounter);
    }

    if (sTestCenterWarpPending
     && sFrameCounter >= sTestCenterWarpFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        if (CalculatePlayerPartyCount() == 0)
            CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        SetWarpDestinationToMapWarp(MAP_GROUP(MAP_OLDALE_TOWN_POKEMON_CENTER_1F),
                                    MAP_NUM(MAP_OLDALE_TOWN_POKEMON_CENTER_1F),
                                    0);
        DoWarp();
        sTestCenterWarpPending = FALSE;
        fprintf(stderr, "PC center test: warp requested at frame %u\n", sFrameCounter);
    }

    if (sTestBattleState == PC_TEST_BATTLE_WAITING
     && sFrameCounter >= sTestBattleFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        ZeroPlayerPartyMons();
        ZeroEnemyPartyMons();
        CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        CreateMon(&gEnemyParty[0], SPECIES_WINGULL, 2, 0, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
        SetMonMoveSlot(&gPlayerParty[0], MOVE_POUND, 0);
        CalculatePlayerPartyCount();
        BattleSetup_StartWildBattle();
        sTestBattleState = PC_TEST_BATTLE_REQUESTED;
        __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
        fprintf(stderr, "PC battle test: wild battle requested at frame %u\n", sFrameCounter);
    }
    else if (sTestBattleState == PC_TEST_BATTLE_REQUESTED && gMain.inBattle)
    {
        sTestBattleState = PC_TEST_BATTLE_ENTERED;
        __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
        fprintf(stderr, "PC battle test: entered battle at frame %u\n", sFrameCounter);
    }
    else if (sTestBattleState == PC_TEST_BATTLE_ENTERED && !gMain.inBattle)
    {
        sTestBattleState = PC_TEST_BATTLE_RETURNED;
        __atomic_store_n(&sShared->testBattleOutcome, gBattleOutcome, __ATOMIC_RELEASE);
        __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
        fprintf(stderr, "PC battle test: returned from battle at frame %u with outcome %u\n",
                sFrameCounter,
                gBattleOutcome);
    }
}

void PcPlatformSoftReset(void)
{
    PcPlatformShutdown();
    exit(PC_CORE_EXIT_SOFT_RESET);
}

static void RunDmaTransfer(struct PcDmaChannel *dma)
{
    uintptr_t source = dma->source;
    uintptr_t destination = dma->destination;
    u32 unitSize = (dma->control & DMA_32BIT) ? 4 : 2;
    u32 sourceMode = dma->control & (DMA_SRC_DEC | DMA_SRC_FIXED);
    u32 destinationMode = dma->control & (DMA_DEST_DEC | DMA_DEST_FIXED | DMA_DEST_RELOAD);
    u32 i;

    for (i = 0; i < dma->count; i++)
    {
        if (unitSize == 4)
            *(vu32 *)destination = *(const vu32 *)source;
        else
            *(vu16 *)destination = *(const vu16 *)source;

        if (sourceMode == DMA_SRC_DEC)
            source -= unitSize;
        else if (sourceMode != DMA_SRC_FIXED)
            source += unitSize;

        if (destinationMode == DMA_DEST_DEC)
            destination -= unitSize;
        else if (destinationMode != DMA_DEST_FIXED && destinationMode != DMA_DEST_RELOAD)
            destination += unitSize;
        else if (destinationMode == DMA_DEST_RELOAD)
            destination += unitSize;
    }

    dma->source = source;
    dma->destination = destinationMode == DMA_DEST_RELOAD ? dma->initialDestination : destination;
}

void PcDmaSet(u8 channel, const void *src, void *dest, u32 control)
{
    struct PcDmaChannel *dma;
    vu32 *registers;
    u32 startMode;

    if (channel >= ARRAY_SIZE(sDma))
        return;

    dma = &sDma[channel];
    dma->source = (uintptr_t)src;
    dma->destination = (uintptr_t)dest;
    dma->initialDestination = (uintptr_t)dest;
    dma->count = control & 0xFFFF;
    if (dma->count == 0)
        dma->count = channel == 3 ? 0x10000 : 0x4000;
    dma->control = (u16)(control >> 16);
    dma->enabled = (dma->control & DMA_ENABLE) != 0;

    registers = (vu32 *)(REG_ADDR_DMA0 + channel * 12);
    registers[0] = (u32)(uintptr_t)src;
    registers[1] = (u32)(uintptr_t)dest;
    registers[2] = control;

    if (!dma->enabled)
        return;

    startMode = dma->control & DMA_START_MASK;
    if (startMode == DMA_START_NOW)
    {
        RunDmaTransfer(dma);
        dma->enabled = FALSE;
        ((vu16 *)registers)[5] &= ~DMA_ENABLE;
    }
}

void PcDmaStop(u8 channel)
{
    if (channel < ARRAY_SIZE(sDma))
    {
        sDma[channel].enabled = FALSE;
        *(vu16 *)(REG_ADDR_DMA0 + channel * 12 + 10) &= ~DMA_ENABLE;
    }
}

static void RunDmasForTiming(u16 timing)
{
    u8 channel;

    for (channel = 0; channel < ARRAY_SIZE(sDma); channel++)
    {
        struct PcDmaChannel *dma = &sDma[channel];

        if (!dma->enabled || (dma->control & DMA_START_MASK) != timing)
            continue;

        RunDmaTransfer(dma);
        if (!(dma->control & DMA_REPEAT))
            dma->enabled = FALSE;
    }
}

void PcDmaRunVBlank(void)
{
    RunDmasForTiming(DMA_START_VBLANK);
}

void PcDmaRunHBlank(void)
{
    RunDmasForTiming(DMA_START_HBLANK);
}
