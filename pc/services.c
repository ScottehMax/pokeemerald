#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include "global.h"
#include "agb_flash.h"
#include "gba/flash_internal.h"
#include "libgcnmultiboot.h"
#include "main.h"
#include "multiboot.h"
#include "pc_services.h"
#include "siirtc.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define PC_FLASH_SIZE FLASH_ROM_SIZE_1M
#define PC_FLASH_SECTOR_SIZE 4096
#define PC_FLASH_SECTOR_COUNT (PC_FLASH_SIZE / PC_FLASH_SECTOR_SIZE)

static u8 *sFlash;
#ifdef _WIN32
static HANDLE sFlashFile = INVALID_HANDLE_VALUE;
static HANDLE sFlashMapping;
#endif
static s64 sRtcOffset;
static u8 sRtcStatus = SIIRTCINFO_24HOUR;

static u16 PcProgramFlashByte(u16 sectorNum, u32 offset, u8 data);
static u16 PcProgramFlashSector(u16 sectorNum, u8 *src);
static u16 PcEraseFlashChip(void);
static u16 PcEraseFlashSector(u16 sectorNum);
static u16 PcWaitForFlashWrite(u8 phase, u8 *addr, u8 lastData);
static u8 PcPollFlashStatus(u8 *addr);

static const u16 sFlashMaxTime[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#define PC_FLASH_SETUP_INITIALIZER \
    { \
        PcProgramFlashByte, PcProgramFlashSector, PcEraseFlashChip, \
        PcEraseFlashSector, PcWaitForFlashWrite, sFlashMaxTime, \
        {PC_FLASH_SIZE, {PC_FLASH_SECTOR_SIZE, 12, PC_FLASH_SECTOR_COUNT, 0}, \
         {0, 0}, {{0xC2, 0x09}}} \
    }

const struct FlashSetupInfo MX29L010 = PC_FLASH_SETUP_INITIALIZER;
const struct FlashSetupInfo LE26FV10N1TS = PC_FLASH_SETUP_INITIALIZER;
const struct FlashSetupInfo DefaultFlash = PC_FLASH_SETUP_INITIALIZER;

u8 gFlashTimeoutFlag;
u8 (*PollFlashStatus)(u8 *) = PcPollFlashStatus;
u16 (*WaitForFlashWrite)(u8, u8 *, u8) = PcWaitForFlashWrite;
u16 (*ProgramFlashSector)(u16, u8 *) = PcProgramFlashSector;
const struct FlashType *gFlash = &DefaultFlash.type;
u16 (*ProgramFlashByte)(u16, u32, u8) = PcProgramFlashByte;
u16 gFlashNumRemainingBytes;
u16 (*EraseFlashChip)(void) = PcEraseFlashChip;
u16 (*EraseFlashSector)(u16) = PcEraseFlashSector;
const u16 *gFlashMaxTime = sFlashMaxTime;

const u8 RomHeaderGameCode[GAME_CODE_LENGTH] = {'B', 'P', 'E', 'E'};
const u8 RomHeaderSoftwareVersion = 0;

static void SyncFlash(const void *address, size_t size, bool32 wait)
{
#ifdef _WIN32
    (void)wait;
    if (address != NULL)
        FlushViewOfFile(address, size);
#else
    msync((void *)address, size, wait ? MS_SYNC : MS_ASYNC);
#endif
}

bool32 PcServicesInit(const char *savePath)
{
#ifdef _WIN32
    LARGE_INTEGER info;
    LARGE_INTEGER requestedSize;
    s64 oldSize;
#else
    struct stat info;
    off_t oldSize;
    int fd;
#endif

    if (savePath == NULL || savePath[0] == '\0')
    {
        fprintf(stderr, "no save path was provided\n");
        return FALSE;
    }

#ifdef _WIN32
    sFlashFile = CreateFileA(savePath,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ,
                             NULL,
                             OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
    if (sFlashFile == INVALID_HANDLE_VALUE || !GetFileSizeEx(sFlashFile, &info))
    {
        fprintf(stderr, "could not open save file %s: Windows error %lu\n", savePath, GetLastError());
        if (sFlashFile != INVALID_HANDLE_VALUE)
            CloseHandle(sFlashFile);
        sFlashFile = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    oldSize = info.QuadPart;
    requestedSize.QuadPart = PC_FLASH_SIZE;
    if (!SetFilePointerEx(sFlashFile, requestedSize, NULL, FILE_BEGIN) || !SetEndOfFile(sFlashFile))
    {
        fprintf(stderr, "could not size save file %s: Windows error %lu\n", savePath, GetLastError());
        CloseHandle(sFlashFile);
        sFlashFile = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    sFlashMapping = CreateFileMappingA(sFlashFile, NULL, PAGE_READWRITE, 0, PC_FLASH_SIZE, NULL);
    if (sFlashMapping != NULL)
        sFlash = MapViewOfFile(sFlashMapping, FILE_MAP_ALL_ACCESS, 0, 0, PC_FLASH_SIZE);
    if (sFlashMapping == NULL || sFlash == NULL)
    {
        fprintf(stderr, "could not map save file %s: Windows error %lu\n", savePath, GetLastError());
        if (sFlashMapping != NULL)
            CloseHandle(sFlashMapping);
        CloseHandle(sFlashFile);
        sFlashMapping = NULL;
        sFlashFile = INVALID_HANDLE_VALUE;
        sFlash = NULL;
        return FALSE;
    }
#else
    fd = open(savePath, O_RDWR | O_CREAT, 0600);
    if (fd < 0 || fstat(fd, &info) != 0)
    {
        if (fd >= 0)
            close(fd);
        fprintf(stderr, "could not open save file %s: %s\n", savePath, strerror(errno));
        return FALSE;
    }

    oldSize = info.st_size;
    if (ftruncate(fd, PC_FLASH_SIZE) != 0)
    {
        fprintf(stderr, "could not size save file %s: %s\n", savePath, strerror(errno));
        close(fd);
        return FALSE;
    }

    sFlash = mmap(NULL, PC_FLASH_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (sFlash == MAP_FAILED)
    {
        sFlash = NULL;
        fprintf(stderr, "could not map save file %s: %s\n", savePath, strerror(errno));
        return FALSE;
    }
#endif

    if (oldSize < PC_FLASH_SIZE)
    {
        if (oldSize < 0)
            oldSize = 0;
        memset(sFlash + oldSize, 0xFF, PC_FLASH_SIZE - oldSize);
        SyncFlash(sFlash, PC_FLASH_SIZE, TRUE);
    }
    return TRUE;
}

void PcServicesShutdown(void)
{
    if (sFlash != NULL)
    {
        SyncFlash(sFlash, PC_FLASH_SIZE, TRUE);
#ifdef _WIN32
        FlushFileBuffers(sFlashFile);
        UnmapViewOfFile(sFlash);
#else
        munmap(sFlash, PC_FLASH_SIZE);
#endif
        sFlash = NULL;
    }
#ifdef _WIN32
    if (sFlashMapping != NULL)
    {
        CloseHandle(sFlashMapping);
        sFlashMapping = NULL;
    }
    if (sFlashFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(sFlashFile);
        sFlashFile = INVALID_HANDLE_VALUE;
    }
#endif
}

static bool32 IsValidFlashRange(u16 sectorNum, u32 offset, u32 size)
{
    return sectorNum < PC_FLASH_SECTOR_COUNT
        && offset <= PC_FLASH_SECTOR_SIZE
        && size <= PC_FLASH_SECTOR_SIZE - offset;
}

static u16 PcProgramFlashByte(u16 sectorNum, u32 offset, u8 data)
{
    if (sFlash == NULL || !IsValidFlashRange(sectorNum, offset, 1))
        return 0x8000;
    sFlash[sectorNum * PC_FLASH_SECTOR_SIZE + offset] = data;
    SyncFlash(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, PC_FLASH_SECTOR_SIZE, FALSE);
    return 0;
}

static u16 PcProgramFlashSector(u16 sectorNum, u8 *src)
{
    if (sFlash == NULL || src == NULL || !IsValidFlashRange(sectorNum, 0, PC_FLASH_SECTOR_SIZE))
        return 0x80FF;
    memcpy(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, src, PC_FLASH_SECTOR_SIZE);
    SyncFlash(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, PC_FLASH_SECTOR_SIZE, TRUE);
    return 0;
}

static u16 PcEraseFlashChip(void)
{
    if (sFlash == NULL)
        return 0x80FF;
    memset(sFlash, 0xFF, PC_FLASH_SIZE);
    SyncFlash(sFlash, PC_FLASH_SIZE, TRUE);
    return 0;
}

static u16 PcEraseFlashSector(u16 sectorNum)
{
    if (sFlash == NULL || !IsValidFlashRange(sectorNum, 0, PC_FLASH_SECTOR_SIZE))
        return 0x80FF;
    memset(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, 0xFF, PC_FLASH_SECTOR_SIZE);
    SyncFlash(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, PC_FLASH_SECTOR_SIZE, TRUE);
    return 0;
}

static u16 PcWaitForFlashWrite(u8 phase, u8 *addr, u8 lastData)
{
    (void)phase;
    (void)addr;
    (void)lastData;
    return 0;
}

static u8 PcPollFlashStatus(u8 *addr)
{
    return addr == NULL ? 0xFF : *addr;
}

u16 SetFlashTimerIntr(u8 timerNum, void (**intrFunc)(void))
{
    if (timerNum >= 4 || intrFunc == NULL)
        return 1;
    *intrFunc = NULL;
    return 0;
}

u16 IdentifyFlash(void)
{
    ProgramFlashByte = PcProgramFlashByte;
    ProgramFlashSector = PcProgramFlashSector;
    EraseFlashChip = PcEraseFlashChip;
    EraseFlashSector = PcEraseFlashSector;
    WaitForFlashWrite = PcWaitForFlashWrite;
    PollFlashStatus = PcPollFlashStatus;
    gFlashMaxTime = sFlashMaxTime;
    gFlash = &DefaultFlash.type;
    return sFlash == NULL;
}

void ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size)
{
    if (dest == NULL)
        return;
    if (sFlash == NULL || !IsValidFlashRange(sectorNum, offset, size))
        memset(dest, 0xFF, size);
    else
        memcpy(dest, sFlash + sectorNum * PC_FLASH_SECTOR_SIZE + offset, size);
}

u32 ProgramFlashSectorAndVerify(u16 sectorNum, u8 *src)
{
    if (PcProgramFlashSector(sectorNum, src) != 0)
        return 1;
    return memcmp(sFlash + sectorNum * PC_FLASH_SECTOR_SIZE, src, PC_FLASH_SECTOR_SIZE) != 0;
}

void SwitchFlashBank(u8 bankNum) { (void)bankNum; }
u16 ReadFlashId(void) { return 0x09C2; }
void StartFlashTimer(u8 phase) { (void)phase; }
void StopFlashTimer(void) {}
void SetReadFlash1(u16 *dest) { (void)dest; }
u16 WaitForFlashWrite_Common(u8 phase, u8 *addr, u8 lastData) { return PcWaitForFlashWrite(phase, addr, lastData); }
u16 EraseFlashChip_MX(void) { return PcEraseFlashChip(); }
u16 EraseFlashSector_MX(u16 sectorNum) { return PcEraseFlashSector(sectorNum); }
u16 ProgramFlashByte_MX(u16 sectorNum, u32 offset, u8 data) { return PcProgramFlashByte(sectorNum, offset, data); }
u16 ProgramFlashSector_MX(u16 sectorNum, u8 *src) { return PcProgramFlashSector(sectorNum, src); }

static u8 ToBcd(int value)
{
    return (u8)(((value / 10) << 4) | (value % 10));
}

static int FromBcd(u8 value)
{
    return (value >> 4) * 10 + (value & 0xF);
}

static void ReadHostClock(struct SiiRtcInfo *rtc)
{
    time_t now = time(NULL) + sRtcOffset;
    struct tm local;

#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    rtc->year = ToBcd((local.tm_year + 1900) % 100);
    rtc->month = ToBcd(local.tm_mon + 1);
    rtc->day = ToBcd(local.tm_mday);
    rtc->dayOfWeek = ToBcd(local.tm_wday);
    rtc->hour = ToBcd(local.tm_hour);
    rtc->minute = ToBcd(local.tm_min);
    rtc->second = ToBcd(local.tm_sec);
}

void SiiRtcUnprotect(void) {}
void SiiRtcProtect(void) {}
u8 SiiRtcProbe(void) { return 1; }

bool8 SiiRtcReset(void)
{
    sRtcOffset = 0;
    sRtcStatus = SIIRTCINFO_24HOUR;
    return TRUE;
}

bool8 SiiRtcGetStatus(struct SiiRtcInfo *rtc)
{
    if (rtc == NULL)
        return FALSE;
    rtc->status = sRtcStatus | SIIRTCINFO_24HOUR;
    return TRUE;
}

bool8 SiiRtcSetStatus(struct SiiRtcInfo *rtc)
{
    if (rtc == NULL)
        return FALSE;
    sRtcStatus = (rtc->status & (SIIRTCINFO_INTFE | SIIRTCINFO_INTME | SIIRTCINFO_INTAE)) | SIIRTCINFO_24HOUR;
    return TRUE;
}

bool8 SiiRtcGetDateTime(struct SiiRtcInfo *rtc)
{
    if (rtc == NULL)
        return FALSE;
    ReadHostClock(rtc);
    return TRUE;
}

bool8 SiiRtcSetDateTime(struct SiiRtcInfo *rtc)
{
    struct tm desired = {0};
    time_t desiredTime;

    if (rtc == NULL)
        return FALSE;
    desired.tm_year = 100 + FromBcd(rtc->year);
    desired.tm_mon = FromBcd(rtc->month) - 1;
    desired.tm_mday = FromBcd(rtc->day);
    desired.tm_hour = FromBcd(rtc->hour);
    desired.tm_min = FromBcd(rtc->minute);
    desired.tm_sec = FromBcd(rtc->second);
    desired.tm_isdst = -1;
    desiredTime = mktime(&desired);
    if (desiredTime == (time_t)-1)
        return FALSE;
    sRtcOffset = (s64)desiredTime - (s64)time(NULL);
    return TRUE;
}

bool8 SiiRtcGetTime(struct SiiRtcInfo *rtc)
{
    return SiiRtcGetDateTime(rtc);
}

bool8 SiiRtcSetTime(struct SiiRtcInfo *rtc)
{
    struct SiiRtcInfo current;

    if (rtc == NULL)
        return FALSE;
    ReadHostClock(&current);
    current.hour = rtc->hour;
    current.minute = rtc->minute;
    current.second = rtc->second;
    return SiiRtcSetDateTime(&current);
}

void GameCubeMultiBoot_Init(struct GcmbStruct *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

void GameCubeMultiBoot_Main(struct GcmbStruct *state) { (void)state; }
void GameCubeMultiBoot_ExecuteProgram(struct GcmbStruct *state) { (void)state; }
void GameCubeMultiBoot_HandleSerialInterrupt(struct GcmbStruct *state) { (void)state; }
void GameCubeMultiBoot_Quit(void) {}

void MultiBootInit(struct MultiBootParam *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

int MultiBootMain(struct MultiBootParam *state)
{
    if (state != NULL)
    {
        state->probe_count = 0;
        state->response_bit = 0;
        state->client_bit = 0;
    }
    return 0;
}

void MultiBootStartProbe(struct MultiBootParam *state) { (void)state; }

void MultiBootStartMaster(struct MultiBootParam *state,
                          const u8 *src,
                          int length,
                          u8 paletteColor,
                          s8 paletteSpeed)
{
    (void)state;
    (void)src;
    (void)length;
    (void)paletteColor;
    (void)paletteSpeed;
}

int MultiBootCheckComplete(struct MultiBootParam *state)
{
    (void)state;
    return 0;
}

void IntrSIO32(void) {}
