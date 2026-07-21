#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#endif

#include "global.h"
#include "battle.h"
#include "link.h"
#include "load_save.h"
#include "main.h"
#include "pc_diagnostics.h"
#include "pc_shared.h"

static struct PcSharedState *sShared;
static uintptr_t sStackLow;
static uintptr_t sStackHigh;
static uintptr_t sTextLow;
static uintptr_t sTextHigh;

static void AddBreadcrumb(u32 kind, u32 id, const void *address)
{
    struct PcDiagnosticState *state;
    u32 index;

    if (sShared == NULL)
        return;
    state = &sShared->diagnostics;
    index = state->breadcrumbWrite++ % PC_DIAGNOSTIC_BREADCRUMBS;
    state->breadcrumbs[index].frame = state->frame;
    state->breadcrumbs[index].kind = kind;
    state->breadcrumbs[index].id = id;
    state->breadcrumbs[index].address = (uintptr_t)address;
}

static bool32 IsSaveBlock1Range(const void *address, size_t size)
{
    uintptr_t start = (uintptr_t)address;
    uintptr_t low = (uintptr_t)&gSaveblock1;
    uintptr_t high = low + sizeof(gSaveblock1);

    return start >= low && start <= high && size <= high - start;
}

static void FindMemoryBounds(void)
{
    uintptr_t marker = (uintptr_t)&marker;

#ifdef _WIN32
    HMODULE module = GetModuleHandleA(NULL);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)module;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((u8 *)module + dos->e_lfanew);
    IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    WORD i;
    typedef VOID (WINAPI *GetCurrentThreadStackLimitsFunc)(PULONG_PTR, PULONG_PTR);
    GetCurrentThreadStackLimitsFunc getStackLimits;

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        if ((section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0)
        {
            uintptr_t low = (uintptr_t)module + section[i].VirtualAddress;
            uintptr_t high = low + section[i].Misc.VirtualSize;

            if (sTextLow == 0 || low < sTextLow)
                sTextLow = low;
            if (high > sTextHigh)
                sTextHigh = high;
        }
    }

    getStackLimits = (GetCurrentThreadStackLimitsFunc)GetProcAddress(
        GetModuleHandleA("kernel32.dll"), "GetCurrentThreadStackLimits");
    if (getStackLimits != NULL)
    {
        ULONG_PTR low;
        ULONG_PTR high;

        getStackLimits(&low, &high);
        sStackLow = low;
        sStackHigh = high;
    }
#else
    FILE *maps = fopen("/proc/self/maps", "r");
    char line[256];
    extern char _start;
    extern char etext;

    sTextLow = (uintptr_t)&_start;
    sTextHigh = (uintptr_t)&etext;
    if (maps != NULL)
    {
        while (fgets(line, sizeof(line), maps) != NULL)
        {
            unsigned long low;
            unsigned long high;

            if (sscanf(line, "%lx-%lx", &low, &high) == 2
             && marker >= low && marker < high)
            {
                sStackLow = low;
                sStackHigh = high;
                break;
            }
        }
        fclose(maps);
    }
#endif

    if (sStackLow == 0 || sStackHigh == 0)
    {
        sStackLow = marker > 1024 * 1024 ? marker - 1024 * 1024 : 0;
        sStackHigh = marker + 1024 * 1024;
    }
}

void PcDiagnosticsInit(struct PcSharedState *shared)
{
    sShared = shared;
    memset(&sShared->diagnostics, 0, sizeof(sShared->diagnostics));
    memset(&sShared->crash, 0, sizeof(sShared->crash));
    sShared->diagnostics.currentTaskId = UINT32_MAX;
    sShared->diagnostics.currentSpriteId = UINT32_MAX;
    FindMemoryBounds();
}

void PcDiagnosticsFrame(void)
{
    struct PcDiagnosticState *state;

    if (sShared == NULL)
        return;
    state = &sShared->diagnostics;
    state->frame = sShared->frameSequence;
    state->mainCallback1 = (uintptr_t)gMain.callback1;
    state->mainCallback2 = (uintptr_t)gMain.callback2;
    state->inBattle = gMain.inBattle;
    state->battleTypeFlags = gBattleTypeFlags;
    state->battleMainFunc = (uintptr_t)gBattleMainFunc;
    state->battleControllerFlags = gBattleControllerExecFlags;
    state->activeBattler = gActiveBattler;
    state->battlersCount = gBattlersCount;
    state->battleOutcome = gBattleOutcome;
    state->linkType = gLinkType;
    state->linkPlayersReceived = gReceivedRemoteLinkPlayers;
    state->wirelessCommType = gWirelessCommType;

    if (IsSaveBlock1Range(gSaveBlock1Ptr, sizeof(*gSaveBlock1Ptr)))
    {
        state->mapGroup = gSaveBlock1Ptr->location.mapGroup;
        state->mapNum = gSaveBlock1Ptr->location.mapNum;
        state->mapX = gSaveBlock1Ptr->pos.x;
        state->mapY = gSaveBlock1Ptr->pos.y;
    }
}

void PcDiagnosticsEnterMain(u32 kind, const void *callback)
{
    if (sShared == NULL)
        return;
    sShared->diagnostics.currentMainKind = kind;
    sShared->diagnostics.currentMainCallback = (uintptr_t)callback;
    AddBreadcrumb(kind, 0, callback);
}

void PcDiagnosticsLeaveMain(void)
{
    if (sShared != NULL)
    {
        sShared->diagnostics.currentMainKind = PC_DIAGNOSTIC_DISPATCH_NONE;
        sShared->diagnostics.currentMainCallback = 0;
    }
}

void PcDiagnosticsEnterTask(u8 taskId, const void *callback, const s16 *data)
{
    if (sShared == NULL)
        return;
    sShared->diagnostics.currentTaskId = taskId;
    sShared->diagnostics.currentTaskFunc = (uintptr_t)callback;
    memcpy(sShared->diagnostics.currentTaskData, data, sizeof(sShared->diagnostics.currentTaskData));
    AddBreadcrumb(PC_DIAGNOSTIC_DISPATCH_TASK, taskId, callback);
}

void PcDiagnosticsLeaveTask(void)
{
    if (sShared != NULL)
    {
        sShared->diagnostics.currentTaskId = UINT32_MAX;
        sShared->diagnostics.currentTaskFunc = 0;
        memset(sShared->diagnostics.currentTaskData, 0, sizeof(sShared->diagnostics.currentTaskData));
    }
}

void PcDiagnosticsEnterSprite(u8 spriteId, const void *callback, const s16 *data)
{
    if (sShared == NULL)
        return;
    sShared->diagnostics.currentSpriteId = spriteId;
    sShared->diagnostics.currentSpriteCallback = (uintptr_t)callback;
    memcpy(sShared->diagnostics.currentSpriteData, data, sizeof(sShared->diagnostics.currentSpriteData));
    AddBreadcrumb(PC_DIAGNOSTIC_DISPATCH_SPRITE, spriteId, callback);
}

void PcDiagnosticsLeaveSprite(void)
{
    if (sShared != NULL)
    {
        sShared->diagnostics.currentSpriteId = UINT32_MAX;
        sShared->diagnostics.currentSpriteCallback = 0;
        memset(sShared->diagnostics.currentSpriteData, 0, sizeof(sShared->diagnostics.currentSpriteData));
    }
}

void PcDiagnosticsEnterScript(const void *script, u32 command, const void *callback)
{
    if (sShared == NULL)
        return;
    sShared->diagnostics.currentScriptPtr = (uintptr_t)script;
    sShared->diagnostics.currentScriptCommand = command;
    sShared->diagnostics.currentScriptFunc = (uintptr_t)callback;
    AddBreadcrumb(command == UINT32_MAX ? PC_DIAGNOSTIC_DISPATCH_SCRIPT_NATIVE
                                       : PC_DIAGNOSTIC_DISPATCH_SCRIPT_COMMAND,
                  command,
                  callback);
}

void PcDiagnosticsLeaveScript(void)
{
    if (sShared != NULL)
    {
        sShared->diagnostics.currentScriptPtr = 0;
        sShared->diagnostics.currentScriptCommand = 0;
        sShared->diagnostics.currentScriptFunc = 0;
    }
}

bool32 PcDiagnosticsIsExecutable(const void *callback)
{
    uintptr_t address = (uintptr_t)callback;

    return address >= sTextLow && address < sTextHigh;
}

void PcDiagnosticsInvalidCallback(u32 kind, u32 id, const void *callback)
{
    if (sShared != NULL)
    {
        sShared->diagnostics.invalidKind = kind;
        sShared->diagnostics.invalidId = id;
        sShared->diagnostics.invalidAddress = (uintptr_t)callback;
    }
#ifdef _WIN32
    RaiseException(0xE0005043u, EXCEPTION_NONCONTINUABLE, 0, NULL);
    ExitProcess(128);
#else
    raise(SIGABRT);
    _Exit(128 + SIGABRT);
#endif
}

static void CaptureStack(struct PcCrashRecord *crash, uintptr_t instruction, uintptr_t frame)
{
    crash->stackFrames[crash->stackFrameCount++] = instruction;
    while (crash->stackFrameCount < PC_DIAGNOSTIC_STACK_FRAMES
        && frame >= sStackLow
        && frame <= sStackHigh - 2 * sizeof(uintptr_t)
        && (frame & (sizeof(uintptr_t) - 1)) == 0)
    {
        const uintptr_t *words = (const uintptr_t *)frame;
        uintptr_t next = words[0];
        uintptr_t address = words[1];

        if (address < sTextLow || address >= sTextHigh)
            break;
        crash->stackFrames[crash->stackFrameCount++] = address;
        if (next <= frame || next - frame > 1024 * 1024)
            break;
        frame = next;
    }
}

bool32 PcDiagnosticsCaptureCrash(u32 code, const void *nativeInfo, const void *nativeContext)
{
    struct PcCrashRecord *crash;
    uintptr_t instruction;
    uintptr_t frame;

    if (sShared == NULL)
        return FALSE;
    crash = &sShared->crash;
    memset(crash, 0, sizeof(*crash));
    crash->magic = PC_CRASH_MAGIC;
    crash->version = PC_CRASH_VERSION;
    crash->code = code;

#ifdef _WIN32
    {
        const EXCEPTION_POINTERS *exception = nativeInfo;
        const CONTEXT *context = exception->ContextRecord;

        (void)nativeContext;

        instruction = context->Eip;
        frame = context->Ebp;
        crash->faultAddress = 0;
        if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
         && exception->ExceptionRecord->NumberParameters >= 2)
        {
            ULONG_PTR operation = exception->ExceptionRecord->ExceptionInformation[0];

            crash->faultAddress = exception->ExceptionRecord->ExceptionInformation[1];
            crash->access = operation == 0 ? PC_CRASH_ACCESS_READ
                          : operation == 1 ? PC_CRASH_ACCESS_WRITE
                          : operation == 8 ? PC_CRASH_ACCESS_EXECUTE
                                           : PC_CRASH_ACCESS_UNKNOWN;
        }
        crash->eax = context->Eax;
        crash->ebx = context->Ebx;
        crash->ecx = context->Ecx;
        crash->edx = context->Edx;
        crash->esi = context->Esi;
        crash->edi = context->Edi;
        crash->ebp = context->Ebp;
        crash->esp = context->Esp;
        crash->eflags = context->EFlags;
    }
#else
    {
        const siginfo_t *info = nativeInfo;
        const ucontext_t *context = nativeContext;

        instruction = 0;
        frame = 0;
        crash->faultAddress = 0;
        if (code == SIGSEGV || code == SIGBUS)
            crash->faultAddress = (uintptr_t)info->si_addr;
#if defined(__i386__) && defined(REG_EIP)
        instruction = context->uc_mcontext.gregs[REG_EIP];
        frame = context->uc_mcontext.gregs[REG_EBP];
        crash->eax = context->uc_mcontext.gregs[REG_EAX];
        crash->ebx = context->uc_mcontext.gregs[REG_EBX];
        crash->ecx = context->uc_mcontext.gregs[REG_ECX];
        crash->edx = context->uc_mcontext.gregs[REG_EDX];
        crash->esi = context->uc_mcontext.gregs[REG_ESI];
        crash->edi = context->uc_mcontext.gregs[REG_EDI];
        crash->ebp = context->uc_mcontext.gregs[REG_EBP];
        crash->esp = context->uc_mcontext.gregs[REG_ESP];
        crash->eflags = context->uc_mcontext.gregs[REG_EFL];
        if (code == SIGSEGV || code == SIGBUS)
        {
            u32 error = context->uc_mcontext.gregs[REG_ERR];

            crash->access = (error & (1 << 4)) ? PC_CRASH_ACCESS_EXECUTE
                          : (error & (1 << 1)) ? PC_CRASH_ACCESS_WRITE
                                               : PC_CRASH_ACCESS_READ;
        }
#endif
    }
#endif

    crash->instruction = instruction;
    CaptureStack(crash, instruction, frame);
    __atomic_store_n(&crash->complete, 1, __ATOMIC_RELEASE);
    return TRUE;
}

void __attribute__((noinline)) PcDiagnosticsTriggerTestCrash(const char *kind)
{
    static volatile uintptr_t address;
    static volatile u32 value;
    static volatile u32 numerator = 1;
    static volatile u32 divisor;

    if (strcmp(kind, "read") == 0)
        value = *(volatile u32 *)address;
    else if (strcmp(kind, "write") == 0)
        *(volatile u32 *)address = 1;
    else if (strcmp(kind, "divide") == 0)
        value = numerator / divisor;
    else if (strcmp(kind, "invalid") == 0)
        PcDiagnosticsInvalidCallback(PC_DIAGNOSTIC_DISPATCH_TASK, 3, (const void *)4);
    else
        abort();
    if (value == UINT32_MAX)
        abort();
    abort();
}
