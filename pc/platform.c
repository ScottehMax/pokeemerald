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
#if PLATFORM_ANDROID
#include <linux/futex.h>
#include <sys/syscall.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "battle_setup.h"
#include "contest.h"
#include "contest_util.h"
#include "event_scripts.h"
#include "field_screen_effect.h"
#include "link.h"
#include "main.h"
#include "malloc.h"
#include "overworld.h"
#include "pokedex.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "script.h"
#include "pc_diagnostics.h"
#include "pc_platform.h"
#include "pc_link.h"
#include "pc_ppu.h"
#include "pc_services.h"
#include "pc_shared.h"
#include "constants/maps.h"
#include "constants/moves.h"
#include "constants/pokedex.h"
#include "constants/pokemon.h"
#include "constants/species.h"
#include "constants/characters.h"
#include "constants/trainers.h"

#define NS_PER_FRAME 16742706ull
#define GBA_CLOCK_HZ 16777216ull
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_TEST_INPUT_EVENTS 32
#define TEST_MOVE_ANIM_READY_DELAY 600
#define TEST_MOVE_ANIM_TIMEOUT 3600
#define TEST_MOVE_ANIM_TURN_COUNT 4
#define TEST_CONTEST_MOVE_ANIM_TURN_COUNT 2

#if !defined(_WIN32) && !defined(MAP_FIXED_NOREPLACE)
#define MAP_FIXED_NOREPLACE 0x100000
#endif

enum PcTestBattleState
{
    PC_TEST_BATTLE_DISABLED,
    PC_TEST_BATTLE_WAITING,
    PC_TEST_BATTLE_REQUESTED,
    PC_TEST_BATTLE_ENTERED,
    PC_TEST_BATTLE_RETURNED,
};

enum PcTestMoveAnimState
{
    PC_TEST_MOVE_ANIM_DISABLED,
    PC_TEST_MOVE_ANIM_WAITING,
    PC_TEST_MOVE_ANIM_RUNNING,
};

enum PcTestContestResultsState
{
    PC_TEST_CONTEST_RESULTS_DISABLED,
    PC_TEST_CONTEST_RESULTS_WAITING,
    PC_TEST_CONTEST_RESULTS_REQUESTED,
    PC_TEST_CONTEST_RESULTS_ENTERED,
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
static u32 sTestFsStorageFrame;
static bool8 sTestFsStoragePending;
static u32 sTestLinkFrame;
static bool8 sTestLinkPending;
static bool8 sTestLinkReportPending;
static u32 sTestLinkBattleFrame;
static bool8 sTestLinkBattlePending;
static u8 sTestLinkCode[PC_LINK_CODE_LENGTH + 1];
static u32 sTestPokedexFrame;
static bool8 sTestPokedexPending;
static u64 sTimer1StartNs;
static bool8 sTestTrainerIdReportPending;
static u32 sTestCrashFrame;
static bool8 sTestCrashPending;
static const char *sTestCrashKind;
static enum PcTestMoveAnimState sTestMoveAnimState;
static u32 sTestMoveAnimReadyFrame;
static u32 sTestMoveAnimStartFrame;
static u16 sTestMoveAnimId;
static u16 sTestMoveAnimFirstId;
static u8 sTestMoveAnimTurn;
static bool8 sTestMoveAnimDouble;
static bool8 sTestMoveAnimContest;
static bool8 sTestMoveAnimSceneRequested;
static enum PcTestContestResultsState sTestContestResultsState;
#if PLATFORM_ANDROID
static u64 sAndroidFrameStartNs;
static u64 sAndroidLastPresentNs;
#endif

#if PLATFORM_ANDROID
// Android commonly occupies the GBA's fixed virtual addresses. Keeping the
// emulated hardware in the low-loaded core preserves 32-bit pointer storage
// without replacing mappings owned by ART or system libraries.
ALIGNED(4096) unsigned char gPcEwram[0x40000];
ALIGNED(4096) unsigned char gPcIwram[0x8000];
ALIGNED(4096) unsigned char gPcIoRegisters[0x1000];
ALIGNED(4096) unsigned char gPcPaletteRam[0x1000];
ALIGNED(4096) unsigned char gPcVram[VRAM_SIZE];
ALIGNED(4096) unsigned char gPcOam[0x1000];
#endif

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

#if PLATFORM_ANDROID
static u64 GetThreadCpuNs(void)
{
    struct timespec now;

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &now);
    return (u64)now.tv_sec * 1000000000ull + now.tv_nsec;
}

static void RecordAndroidPerformanceStall(u32 kind, u32 durationUs, u32 detail)
{
    struct PcPerformanceState *performance = &sShared->performance;
    u32 sequence = __atomic_fetch_add(&performance->stallWrite, 1, __ATOMIC_RELAXED) + 1;
    struct PcPerformanceStall *stall =
        &performance->stalls[(sequence - 1) % PC_PERFORMANCE_STALLS];

    __atomic_store_n(&stall->sequence, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->frame,
                     __atomic_load_n(&sShared->frameSequence, __ATOMIC_ACQUIRE),
                     __ATOMIC_RELAXED);
    __atomic_store_n(&stall->kind, kind, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->durationUs, durationUs, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->detail, detail, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->sequence, sequence, __ATOMIC_RELEASE);
}
#endif

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

#if !PLATFORM_ANDROID
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
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1,
                         0);

    if (mapping == MAP_FAILED && errno == EINVAL)
    {
        mapping = mmap((void *)address,
                       size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1,
                       0);
        if (mapping != MAP_FAILED && (uintptr_t)mapping != address)
        {
            munmap(mapping, size);
            errno = EEXIST;
            mapping = MAP_FAILED;
        }
    }
#endif

#ifdef _WIN32
    return mapping != NULL && (uintptr_t)mapping == address;
#else
    return mapping != MAP_FAILED && (uintptr_t)mapping == address;
#endif
}
#endif

static bool32 InitGbaMemory(void)
{
#if PLATFORM_ANDROID
    // These arrays are part of the core's low-address reservation, so any
    // pointers written into emulated 32-bit fields remain representable.
    memset(gPcEwram, 0, sizeof(gPcEwram));
    memset(gPcIwram, 0, sizeof(gPcIwram));
    memset(gPcIoRegisters, 0, sizeof(gPcIoRegisters));
    memset(gPcPaletteRam, 0, sizeof(gPcPaletteRam));
    memset(gPcVram, 0, sizeof(gPcVram));
    memset(gPcOam, 0, sizeof(gPcOam));
    return TRUE;
#else
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
#endif
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

static void ParseTestFsStorageFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestFsStorageFrame = UINT32_MAX;
    sTestFsStoragePending = FALSE;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_FS_STORAGE_AT value: %s\n", spec);
        return;
    }

    sTestFsStorageFrame = (u32)frame;
    sTestFsStoragePending = TRUE;
}

static void ParseTestLink(const char *frameSpec, const char *codeSpec)
{
    char *end;
    unsigned long frame;
    size_t length;
    size_t i;

    sTestLinkFrame = UINT32_MAX;
    sTestLinkPending = FALSE;
    sTestLinkReportPending = FALSE;
    if (frameSpec == NULL || *frameSpec == '\0' || codeSpec == NULL)
        return;

    errno = 0;
    frame = strtoul(frameSpec, &end, 0);
    length = strlen(codeSpec);
    if (errno != 0
     || end == frameSpec
     || *end != '\0'
     || frame > UINT32_MAX
     || length == 0
     || length > PC_LINK_CODE_LENGTH)
    {
        fprintf(stderr, "invalid PC link test configuration\n");
        return;
    }

    memset(sTestLinkCode, EOS, sizeof(sTestLinkCode));
    for (i = 0; i < length; i++)
    {
        if (codeSpec[i] >= 'A' && codeSpec[i] <= 'Z')
            sTestLinkCode[i] = CHAR_A + codeSpec[i] - 'A';
        else if (codeSpec[i] >= 'a' && codeSpec[i] <= 'z')
            sTestLinkCode[i] = CHAR_A + codeSpec[i] - 'a';
        else if (codeSpec[i] >= '0' && codeSpec[i] <= '9')
            sTestLinkCode[i] = CHAR_0 + codeSpec[i] - '0';
        else
        {
            fprintf(stderr, "PC link test code must be alphanumeric\n");
            return;
        }
    }
    sTestLinkFrame = (u32)frame;
    sTestLinkPending = TRUE;
}

static void ParseTestLinkBattleFrame(const char *spec)
{
    char *end;
    unsigned long frame;

    sTestLinkBattleFrame = UINT32_MAX;
    sTestLinkBattlePending = FALSE;
    if (spec == NULL || *spec == '\0')
        return;

    errno = 0;
    frame = strtoul(spec, &end, 0);
    if (errno != 0 || end == spec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_LINK_BATTLE_AT value: %s\n", spec);
        return;
    }
    sTestLinkBattleFrame = (u32)frame;
    sTestLinkBattlePending = TRUE;
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

static void ParseTestCrash(const char *frameSpec, const char *kind)
{
    char *end;
    unsigned long frame;

    sTestCrashPending = FALSE;
    if (frameSpec == NULL || *frameSpec == '\0')
        return;
    errno = 0;
    frame = strtoul(frameSpec, &end, 0);
    if (errno != 0 || end == frameSpec || *end != '\0' || frame > UINT32_MAX)
    {
        fprintf(stderr, "invalid POKEEMERALD_PC_TEST_CRASH_AT value: %s\n", frameSpec);
        return;
    }
    sTestCrashFrame = (u32)frame;
    sTestCrashKind = kind == NULL || *kind == '\0' ? "read" : kind;
    sTestCrashPending = TRUE;
}

bool32 PcPlatformInit(const char *sharedPath)
{
    const char *testInput;
    const char *testMoveAnims;
#ifndef _WIN32
    struct stat info;
    int fd;
#endif

#if PLATFORM_ANDROID
    if ((uintptr_t)&PcPlatformInit > UINT32_MAX || (uintptr_t)&sharedPath > UINT32_MAX)
    {
        fprintf(stderr, "Android game core or stack was not loaded below 4 GiB\n");
        return FALSE;
    }
#else
    if (sizeof(void *) != 4)
    {
        fprintf(stderr, "pokeemerald-core must use a 32-bit ABI\n");
        return FALSE;
    }
#endif

    // Initialize the emulated hardware before opening frontend shared memory.
    // Desktop builds claim the GBA ranges first; Android uses core-owned RAM.
    if (!InitGbaMemory())
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
    if (sShared->magic != PC_SHARED_MAGIC || sShared->version != PC_SHARED_VERSION)
    {
        fprintf(stderr,
                "invalid SDL frontend shared memory (magic 0x%08x, version %u; expected 0x%08x, version %u)\n",
                sShared->magic,
                sShared->version,
                PC_SHARED_MAGIC,
                PC_SHARED_VERSION);
        PcPlatformShutdown();
        return FALSE;
    }
    PcDiagnosticsInit(sShared);

    if (!PcServicesInit(sShared->savePath, sShared->storagePath))
    {
        sShared->coreError = 2;
        return FALSE;
    }
    if (getenv("POKEEMERALD_PC_TEST_STORAGE_SCAN") != NULL)
    {
        if (PcStorageScan())
            fprintf(stderr, "PC storage test: scanned %u files\n", PcStorageCount());
        else
            fprintf(stderr, "PC storage test: scan failed\n");
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
    ParseTestFsStorageFrame(getenv("POKEEMERALD_PC_TEST_FS_STORAGE_AT"));
    ParseTestLink(getenv("POKEEMERALD_PC_TEST_LINK_AT"),
                  getenv("POKEEMERALD_PC_TEST_LINK_CODE"));
    ParseTestLinkBattleFrame(getenv("POKEEMERALD_PC_TEST_LINK_BATTLE_AT"));
    ParseTestPokedexFrame(getenv("POKEEMERALD_PC_TEST_POKEDEX_AT"));
    ParseTestCrash(getenv("POKEEMERALD_PC_TEST_CRASH_AT"),
                   getenv("POKEEMERALD_PC_TEST_CRASH_KIND"));
    testMoveAnims = getenv("POKEEMERALD_PC_TEST_MOVE_ANIMS");
    if (testMoveAnims != NULL)
    {
        const char *firstMove = getenv("POKEEMERALD_PC_TEST_MOVE_ANIM_START");

        sTestMoveAnimDouble = strcmp(testMoveAnims, "double") == 0;
        sTestMoveAnimContest = strcmp(testMoveAnims, "contest") == 0;
        if (firstMove != NULL)
        {
            char *end;
            unsigned long value = strtoul(firstMove, &end, 0);

            if (end != firstMove && *end == '\0' && value < MOVES_COUNT)
                sTestMoveAnimFirstId = value;
            else
                fprintf(stderr, "invalid POKEEMERALD_PC_TEST_MOVE_ANIM_START value: %s\n",
                        firstMove);
        }
        sTestMoveAnimState = PC_TEST_MOVE_ANIM_WAITING;
        if (!sTestMoveAnimContest && sTestBattleState == PC_TEST_BATTLE_DISABLED)
        {
            sTestBattleFrame = 60;
            sTestBattleState = PC_TEST_BATTLE_WAITING;
        }
    }
    if (getenv("POKEEMERALD_PC_TEST_CONTEST_RESULTS") != NULL)
        sTestContestResultsState = PC_TEST_CONTEST_RESULTS_WAITING;
    sTestTrainerIdReportPending = getenv("POKEEMERALD_PC_TEST_REPORT_ID") != NULL;
    sFrameCounter = 0;
    sTimer1StartNs = 0;
    sNextFrameTime = GetMonotonicNs();
#if PLATFORM_ANDROID
    sAndroidFrameStartNs = 0;
    sAndroidLastPresentNs = 0;
#endif
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

void PcPlatformRecordExit(int status)
{
    if (sShared != NULL)
    {
        __atomic_store_n(&sShared->coreExitStatus, status, __ATOMIC_RELAXED);
        __atomic_store_n(&sShared->coreExited, 1, __ATOMIC_RELEASE);
    }
}

static void ExitCore(int status) __attribute__((noreturn));

static void ExitCore(int status)
{
    PcPlatformRecordExit(status);
    PcPlatformShutdown();
    exit(status);
}

const char *PcPlatformGetDefaultSavePath(void)
{
    return sShared->defaultSavePath;
}

const char *PcPlatformGetSavePath(void)
{
    return sShared->savePath;
}

bool32 PcPlatformShouldResumeMainMenu(void)
{
    return sShared->resumeMainMenu != 0;
}

void PcPlatformSwitchProfile(const char *savePath, const char *storagePath)
{
    if (savePath == NULL || storagePath == NULL
     || snprintf(sShared->requestedSavePath, sizeof(sShared->requestedSavePath), "%s", savePath)
        >= (int)sizeof(sShared->requestedSavePath)
     || snprintf(sShared->requestedStoragePath, sizeof(sShared->requestedStoragePath), "%s", storagePath)
        >= (int)sizeof(sShared->requestedStoragePath))
    {
        fprintf(stderr, "could not switch save profile: path is too long\n");
        ExitCore(1);
    }
    ExitCore(PC_CORE_EXIT_PROFILE_SWITCH);
}

void PcPlatformWaitForFrame(void)
{
    u64 now;
    u32 keys;
    u32 i;
#if PLATFORM_ANDROID
    bool8 resumed = FALSE;
    u64 beforeSleepNs;
#endif

#if PLATFORM_ANDROID
    now = GetMonotonicNs();
    if (sAndroidLastPresentNs != 0)
    {
        u32 callbackUs = (u32)((now - sAndroidLastPresentNs) / 1000);

        __atomic_store_n(&sShared->performance.coreCallbackUs, callbackUs, __ATOMIC_RELAXED);
        if (callbackUs > __atomic_load_n(&sShared->performance.coreMaxCallbackUs, __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->performance.coreMaxCallbackUs, callbackUs, __ATOMIC_RELAXED);
    }
#endif

    while (__atomic_load_n(&sShared->paused, __ATOMIC_ACQUIRE)
        && !__atomic_load_n(&sShared->quit, __ATOMIC_ACQUIRE))
    {
#if PLATFORM_ANDROID
        resumed = TRUE;
#endif
        SleepUntil(GetMonotonicNs() + 10000000ull);
    }
#if PLATFORM_ANDROID
    if (resumed)
        sAndroidLastPresentNs = 0;
#endif

    sNextFrameTime += NS_PER_FRAME;
#if PLATFORM_ANDROID
    beforeSleepNs = GetMonotonicNs();
#endif
    if (!sFastForward)
        SleepUntil(sNextFrameTime);
    now = GetMonotonicNs();
#if PLATFORM_ANDROID
    if (!sFastForward && beforeSleepNs < sNextFrameTime && now > sNextFrameTime)
    {
        u32 overshootUs = (u32)((now - sNextFrameTime) / 1000);

        __atomic_store_n(&sShared->performance.coreSleepOvershootUs,
                         overshootUs,
                         __ATOMIC_RELAXED);
        if (overshootUs > __atomic_load_n(&sShared->performance.coreMaxSleepOvershootUs,
                                          __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->performance.coreMaxSleepOvershootUs,
                             overshootUs,
                             __ATOMIC_RELAXED);
        if (overshootUs > 1000)
            __atomic_add_fetch(&sShared->performance.coreSleepOvershoots,
                               1,
                               __ATOMIC_RELAXED);
    }
#endif
#if PLATFORM_ANDROID
    if (!sFastForward && now > sNextFrameTime + 2000000ull)
    {
        __atomic_add_fetch(&sShared->performance.coreLateFrames, 1, __ATOMIC_RELAXED);
        sNextFrameTime = now;
    }
#else
    if (now > sNextFrameTime + NS_PER_FRAME * 4)
        sNextFrameTime = now;
#endif
#if PLATFORM_ANDROID
    sAndroidFrameStartNs = now;
#endif

    if (__atomic_load_n(&sShared->quit, __ATOMIC_ACQUIRE))
    {
        ExitCore(0);
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
    if (sTestMoveAnimState == PC_TEST_MOVE_ANIM_RUNNING
     || (sTestMoveAnimState != PC_TEST_MOVE_ANIM_DISABLED
      && sTestBattleState >= PC_TEST_BATTLE_REQUESTED))
        keys = 0;
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
    u32 frameWidth = DISPLAY_WIDTH;
#if PLATFORM_ANDROID
    u64 ppuStartNs = GetMonotonicNs();
    u64 ppuCpuStartNs = GetThreadCpuNs();
    u64 now;
    u32 vblankUs = sAndroidFrameStartNs == 0
                 ? 0
                 : (u32)((ppuStartNs - sAndroidFrameStartNs) / 1000);
#endif

    if (gMain.callback2 == CB2_Overworld
     || gMain.callback2 == CB2_OverworldBasic)
        frameWidth = PcPlatformGetOverworldViewportWidth();
    PcPpuRender(sShared->pixels[buffer], frameWidth, hblankCallback);
#if PLATFORM_ANDROID
    now = GetMonotonicNs();
    {
        u32 ppuUs = (u32)((now - ppuStartNs) / 1000);
        u32 ppuCpuUs = (u32)((GetThreadCpuNs() - ppuCpuStartNs) / 1000);
        u32 previousMax = __atomic_load_n(&sShared->performance.coreMaxPpuUs,
                                          __ATOMIC_RELAXED);

        __atomic_store_n(&sShared->performance.coreVblankUs, vblankUs, __ATOMIC_RELAXED);
        __atomic_store_n(&sShared->performance.corePpuUs, ppuUs, __ATOMIC_RELAXED);
        __atomic_store_n(&sShared->performance.corePpuCpuUs, ppuCpuUs, __ATOMIC_RELAXED);
        if (vblankUs > __atomic_load_n(&sShared->performance.coreMaxVblankUs, __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->performance.coreMaxVblankUs, vblankUs, __ATOMIC_RELAXED);
        if (ppuUs > previousMax)
        {
            __atomic_store_n(&sShared->performance.coreMaxPpuUs, ppuUs, __ATOMIC_RELAXED);
            __atomic_store_n(&sShared->performance.coreMaxPpuFrame,
                             sFrameCounter,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&sShared->performance.coreMaxPpuMapGroup,
                             sShared->diagnostics.mapGroup,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&sShared->performance.coreMaxPpuMapNum,
                             sShared->diagnostics.mapNum,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&sShared->performance.coreMaxPpuMainCallback,
                             sShared->diagnostics.mainCallback2,
                             __ATOMIC_RELAXED);
        }
        if (ppuCpuUs > __atomic_load_n(&sShared->performance.coreMaxPpuCpuUs,
                                       __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->performance.coreMaxPpuCpuUs,
                             ppuCpuUs,
                             __ATOMIC_RELAXED);
        if (ppuUs > 8000)
            __atomic_add_fetch(&sShared->performance.coreSlowPpuFrames,
                               1,
                               __ATOMIC_RELAXED);
        if (ppuUs >= 12000)
            RecordAndroidPerformanceStall(PC_PERFORMANCE_STALL_CORE_PPU,
                                          ppuUs,
                                          ppuCpuUs);
    }
    if (sAndroidLastPresentNs != 0)
    {
        u32 gapUs = (u32)((now - sAndroidLastPresentNs) / 1000);

        __atomic_store_n(&sShared->performance.coreFrameGapUs, gapUs, __ATOMIC_RELAXED);
        if (gapUs > __atomic_load_n(&sShared->performance.coreMaxFrameGapUs, __ATOMIC_RELAXED))
            __atomic_store_n(&sShared->performance.coreMaxFrameGapUs, gapUs, __ATOMIC_RELAXED);
        if (gapUs >= 25000)
            RecordAndroidPerformanceStall(PC_PERFORMANCE_STALL_CORE_FRAME_GAP,
                                          gapUs,
                                          0);
    }
    sAndroidLastPresentNs = now;
#endif
    __atomic_store_n(&sShared->frameWidth, frameWidth, __ATOMIC_RELAXED);
    __atomic_store_n(&sShared->frameHeight, DISPLAY_HEIGHT, __ATOMIC_RELAXED);
    __atomic_store_n(&sShared->frameBufferIndex, buffer, __ATOMIC_RELEASE);
    __atomic_add_fetch(&sShared->frameSequence, 1, __ATOMIC_RELEASE);
#if PLATFORM_ANDROID
    syscall(SYS_futex,
            &sShared->frameSequence,
            FUTEX_WAKE,
            1,
            NULL,
            NULL,
            0);
#endif
}

u32 PcPlatformGetOverworldViewportWidth(void)
{
    u32 width;

    if (sShared == NULL)
        return DISPLAY_WIDTH;
    width = __atomic_load_n(&sShared->requestedFrameWidth, __ATOMIC_ACQUIRE);
    if (width < DISPLAY_WIDTH)
        width = DISPLAY_WIDTH;
    if (width > PC_FRAME_MAX_WIDTH)
        width = PC_FRAME_MAX_WIDTH;
    return width;
}

void PcPlatformQueueAudio(const s16 *samples, u32 frameCount)
{
    u32 generated = frameCount;
    u32 read = __atomic_load_n(&sShared->audioRead, __ATOMIC_ACQUIRE);
    u32 write = __atomic_load_n(&sShared->audioWrite, __ATOMIC_RELAXED);
    u32 available = PC_AUDIO_BUFFER_FRAMES - (write - read);
    u32 nonzero = 0;
    u32 clipped = 0;
    u32 peak = 0;
    u32 i;

    for (i = 0; i < generated * 2; i++)
    {
        s32 value = samples[i];
        u32 amplitude = value < 0 ? (u32)-value : (u32)value;

        if (amplitude != 0)
            nonzero++;
        if (amplitude >= 32767)
            clipped++;
        if (amplitude > peak)
            peak = amplitude;
    }
    if (peak > __atomic_load_n(&sShared->audioPeak, __ATOMIC_RELAXED))
        __atomic_store_n(&sShared->audioPeak, peak, __ATOMIC_RELAXED);
    __atomic_add_fetch(&sShared->audioFramesGenerated, generated, __ATOMIC_RELEASE);
    __atomic_add_fetch(&sShared->audioSamplesNonzero, nonzero, __ATOMIC_RELAXED);
    __atomic_add_fetch(&sShared->audioSamplesClipped, clipped, __ATOMIC_RELAXED);

    if (frameCount > available)
        frameCount = available;

    if (frameCount != 0)
    {
        u32 offset = write & (PC_AUDIO_BUFFER_FRAMES - 1);
        u32 first = frameCount < PC_AUDIO_BUFFER_FRAMES - offset
                  ? frameCount
                  : PC_AUDIO_BUFFER_FRAMES - offset;

        memcpy(&sShared->audio[offset * 2], samples, first * 2 * sizeof(*samples));
        if (frameCount > first)
            memcpy(sShared->audio,
                   &samples[first * 2],
                   (frameCount - first) * 2 * sizeof(*samples));
    }
    __atomic_store_n(&sShared->audioWrite, write + frameCount, __ATOMIC_RELEASE);
}

static void StartTestMoveAnimation(void)
{
    u8 target = gBattleMoves[sTestMoveAnimId].target;

    if (!CheckHeap())
    {
        fprintf(stderr, "PC move animation test: heap corruption before move=%u turn=%u\n",
                sTestMoveAnimId,
                sTestMoveAnimTurn);
        ExitCore(3);
    }

    if (sTestMoveAnimContest)
    {
        PcContestPrepareMoveAnim(sTestMoveAnimId, sTestMoveAnimTurn != 0);
    }
    else
    {
        gBattlerAttacker = B_POSITION_PLAYER_LEFT;
        if (target == MOVE_TARGET_USER || target == MOVE_TARGET_USER_OR_SELECTED)
            gBattlerTarget = B_POSITION_PLAYER_LEFT;
        else
            gBattlerTarget = B_POSITION_OPPONENT_LEFT;
        gAnimMoveTurn = sTestMoveAnimTurn;
        gAnimMovePower = gBattleMoves[sTestMoveAnimId].power;
        gAnimMoveDmg = 50;
        gAnimFriendship = 128;
        gWeatherMoveAnim = 0;
        gAnimDisableStructPtr = &gDisableStructs[gBattlerAttacker];
    }
    gActiveBattler = gBattlerAttacker;
    sTestMoveAnimStartFrame = sFrameCounter;
    fprintf(stderr, "PC move animation test: move=%u turn=%u\n",
            sTestMoveAnimId,
            sTestMoveAnimTurn);
    DoMoveAnim(sTestMoveAnimId);
}

static void SetupTestContest(void)
{
    ZeroPlayerPartyMons();
    CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 16, 31, FALSE, 0,
              OT_ID_PLAYER_ID, 0);
    SetMonMoveSlot(&gPlayerParty[0], MOVE_POUND, 0);
    CalculatePlayerPartyCount();
    gLinkContestFlags = 0;
    gContestPlayerMonIndex = CONTESTANT_COUNT - 1;
    gContestMonPartyIndex = 0;
    gSpecialVar_ContestCategory = CONTEST_CATEGORY_COOL;
    gSpecialVar_ContestRank = CONTEST_RANK_NORMAL;
    SetContestants(gSpecialVar_ContestCategory, gSpecialVar_ContestRank);
    CreateContestMonFromParty(gContestMonPartyIndex);
}

static void RunTestMoveAnimations(void)
{
    if (sTestMoveAnimState == PC_TEST_MOVE_ANIM_DISABLED)
        return;

    if (sTestMoveAnimState == PC_TEST_MOVE_ANIM_WAITING)
    {
        if (sTestMoveAnimContest)
        {
            if (!sTestMoveAnimSceneRequested)
            {
                if (sFrameCounter < 60
                 || gMain.callback2 != CB2_Overworld
                 || gMain.inBattle)
                    return;

                SetupTestContest();
                CalculateRound1Points(gSpecialVar_ContestCategory);
                CleanupOverworldWindowsAndTilemaps();
                gMain.state = 0;
                SetMainCallback2(CB2_StartContest);
                sTestMoveAnimSceneRequested = TRUE;
                fprintf(stderr, "PC move animation test: contest requested at frame %u\n",
                        sFrameCounter);
                return;
            }

            if (!CheckHeap())
            {
                fprintf(stderr, "PC move animation test: heap corruption before contest move at frame %u\n",
                        sFrameCounter);
                ExitCore(3);
            }

            if (gContestResources == NULL || !gAnimScriptActive)
                return;

            // The first appeal establishes the contest sprites and buffers. Its
            // move script has not run yet, so replace the contest task here.
            ResetTasks();
            gAnimScriptActive = FALSE;
            gMain.callback1 = NULL;
            sTestMoveAnimId = sTestMoveAnimFirstId;
            sTestMoveAnimTurn = 0;
            sTestMoveAnimState = PC_TEST_MOVE_ANIM_RUNNING;
            StartTestMoveAnimation();
            return;
        }

        u8 attackerSpriteId = gBattlerSpriteIds[B_POSITION_PLAYER_LEFT];
        u8 targetSpriteId = gBattlerSpriteIds[B_POSITION_OPPONENT_LEFT];
        u8 attackerPartnerSpriteId = gBattlerSpriteIds[B_POSITION_PLAYER_RIGHT];
        u8 targetPartnerSpriteId = gBattlerSpriteIds[B_POSITION_OPPONENT_RIGHT];

        if (sTestBattleState != PC_TEST_BATTLE_ENTERED
         || sFrameCounter < sTestMoveAnimReadyFrame
         || attackerSpriteId >= MAX_SPRITES
         || targetSpriteId >= MAX_SPRITES
         || !gSprites[attackerSpriteId].inUse
         || !gSprites[targetSpriteId].inUse
         || (sTestMoveAnimDouble
          && (attackerPartnerSpriteId >= MAX_SPRITES
           || targetPartnerSpriteId >= MAX_SPRITES
           || !gSprites[attackerPartnerSpriteId].inUse
           || !gSprites[targetPartnerSpriteId].inUse)))
            return;

        gMain.callback1 = NULL;
        sTestMoveAnimId = sTestMoveAnimFirstId;
        sTestMoveAnimTurn = 0;
        sTestMoveAnimState = PC_TEST_MOVE_ANIM_RUNNING;
        StartTestMoveAnimation();
        return;
    }

    if (gAnimScriptActive)
    {
        gAnimScriptCallback();
        if (gAnimScriptActive
         && sFrameCounter - sTestMoveAnimStartFrame > TEST_MOVE_ANIM_TIMEOUT)
        {
            fprintf(stderr, "PC move animation test: timeout move=%u turn=%u after %u frames\n",
                    sTestMoveAnimId,
                    sTestMoveAnimTurn,
                    TEST_MOVE_ANIM_TIMEOUT);
            ExitCore(2);
        }
        return;
    }

    if (++sTestMoveAnimId >= MOVES_COUNT)
    {
        sTestMoveAnimId = sTestMoveAnimFirstId;
        u8 turnCount = sTestMoveAnimDouble
                     ? 1
                     : sTestMoveAnimContest
                     ? TEST_CONTEST_MOVE_ANIM_TURN_COUNT
                     : TEST_MOVE_ANIM_TURN_COUNT;

        if (++sTestMoveAnimTurn >= turnCount)
        {
            if (!CheckHeap())
            {
                fprintf(stderr, "PC move animation test: heap corruption after final move\n");
                ExitCore(3);
            }
            fprintf(stderr, "PC move animation test: %u moves passed for %u turn values\n",
                    MOVES_COUNT - sTestMoveAnimFirstId,
                    turnCount);
            ExitCore(0);
        }
    }
    StartTestMoveAnimation();
}

void PcPlatformRunTestHooks(void)
{
    if (sTestCrashPending && sFrameCounter >= sTestCrashFrame)
    {
        sTestCrashPending = FALSE;
        fprintf(stderr, "PC diagnostics test: triggering %s crash at frame %u\n",
                sTestCrashKind,
                sFrameCounter);
        PcDiagnosticsTriggerTestCrash(sTestCrashKind);
    }

    if (sTestLinkReportPending && gReceivedRemoteLinkPlayers)
    {
        fprintf(stderr,
                "PC link test: player data exchanged as player %u at frame %u\n",
                GetMultiplayerId() + 1,
                sFrameCounter);
        gHeldKeyCodeToSend = LINK_KEY_CODE_EMPTY;
        StartSendingKeysToLink();
        sTestLinkReportPending = FALSE;
    }

    if (sTestLinkPending
     && sFrameCounter >= sTestLinkFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        PcLinkSetCode(sTestLinkCode);
        gLinkType = LINKTYPE_TRADE_SETUP;
        OpenLinkTimed();
        sTestLinkReportPending = TRUE;
        sTestLinkPending = FALSE;
        fprintf(stderr, "PC link test: connection requested at frame %u\n", sFrameCounter);
    }

    if (sTestLinkBattlePending
     && sFrameCounter >= sTestLinkBattleFrame
     && gReceivedRemoteLinkPlayers
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        ZeroPlayerPartyMons();
        CreateMon(&gPlayerParty[0], SPECIES_TREECKO, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        SetMonMoveSlot(&gPlayerParty[0], MOVE_POUND, 0);
        CalculatePlayerPartyCount();
        SaveLinkPlayers(2);
        gLinkType = LINKTYPE_BATTLE;
        gLinkPlayers[0].linkType = LINKTYPE_BATTLE;
        ClearLinkCallback_2();
        gBattleTypeFlags = BATTLE_TYPE_LINK | BATTLE_TYPE_TRAINER;
        gTrainerBattleOpponent_A = TRAINER_LINK_OPPONENT;
        CleanupOverworldWindowsAndTilemaps();
        gMain.savedCallback = CB2_ReturnToField;
        SetMainCallback2(CB2_InitBattle);
        sTestLinkBattlePending = FALSE;
        sTestBattleState = PC_TEST_BATTLE_REQUESTED;
        __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
        fprintf(stderr, "PC link test: battle requested at frame %u\n", sFrameCounter);
    }

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

    if (sTestFsStoragePending
     && sFrameCounter >= sTestFsStorageFrame
     && gMain.callback2 == CB2_Overworld
     && !gMain.inBattle)
    {
        if (!CheckBoxMonSanityAt(0, 0))
            CreateBoxMonAt(0, 0, SPECIES_TREECKO, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
        ScriptContext_SetupScript(EventScript_PC);
        sTestFsStoragePending = FALSE;
        fprintf(stderr, "PC filesystem storage test: menu requested at frame %u\n", sFrameCounter);
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
        if (sTestMoveAnimDouble)
        {
            CreateMon(&gPlayerParty[1], SPECIES_TORCHIC, 16, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);
            CreateMon(&gEnemyParty[1], SPECIES_LOTAD, 2, 0, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
        }
        SetMonMoveSlot(&gPlayerParty[0], MOVE_POUND, 0);
        CalculatePlayerPartyCount();
        BattleSetup_StartWildBattle();
        if (sTestMoveAnimDouble)
            gBattleTypeFlags |= BATTLE_TYPE_DOUBLE;
        sTestBattleState = PC_TEST_BATTLE_REQUESTED;
        __atomic_store_n(&sShared->testBattleState, sTestBattleState, __ATOMIC_RELEASE);
        fprintf(stderr, "PC battle test: wild battle requested at frame %u\n", sFrameCounter);
    }
    else if (sTestBattleState == PC_TEST_BATTLE_REQUESTED && gMain.inBattle)
    {
        sTestBattleState = PC_TEST_BATTLE_ENTERED;
        if (sTestMoveAnimState == PC_TEST_MOVE_ANIM_WAITING)
            sTestMoveAnimReadyFrame = sFrameCounter + TEST_MOVE_ANIM_READY_DELAY;
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

    RunTestMoveAnimations();

    if (sTestContestResultsState == PC_TEST_CONTEST_RESULTS_WAITING)
    {
        if (sFrameCounter >= 60 && gMain.callback2 == CB2_Overworld && !gMain.inBattle)
        {
            u8 i;

            SetupTestContest();
            for (i = 0; i < CONTESTANT_COUNT; i++)
            {
                gContestMonRound1Points[i] = (CONTESTANT_COUNT - i) * 100;
                gContestMonRound2Points[i] = 0;
                gContestMonTotalPoints[i] = gContestMonRound1Points[i];
                gContestFinalStandings[i] = i;
            }
            ShowContestResults();
            sTestContestResultsState = PC_TEST_CONTEST_RESULTS_REQUESTED;
            fprintf(stderr, "PC contest results test: requested at frame %u\n", sFrameCounter);
        }
    }
    else if (sTestContestResultsState == PC_TEST_CONTEST_RESULTS_REQUESTED)
    {
        if (gMain.callback2 != CB2_Overworld)
            sTestContestResultsState = PC_TEST_CONTEST_RESULTS_ENTERED;
    }
    else if (sTestContestResultsState == PC_TEST_CONTEST_RESULTS_ENTERED
          && gMain.callback2 == CB2_Overworld
          && !gPaletteFade.active)
    {
        if (!CheckHeap())
        {
            fprintf(stderr, "PC contest results test: heap corruption after return\n");
            ExitCore(3);
        }
        fprintf(stderr, "PC contest results test: returned successfully at frame %u\n",
                sFrameCounter);
        ExitCore(0);
    }
}

void PcPlatformSoftReset(void)
{
    ExitCore(PC_CORE_EXIT_SOFT_RESET);
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
