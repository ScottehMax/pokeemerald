#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include <limits.h>
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
#include <sys/syscall.h>
#include <ucontext.h>
#include <unistd.h>
#endif

#include "global.h"
#include "main.h"
#include "pc_diagnostics.h"
#include "pc_platform.h"

void AgbMain(void);

#ifdef _WIN32
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS *exception)
{
    uintptr_t instruction = (uintptr_t)exception->ExceptionRecord->ExceptionAddress;
    uintptr_t address = 0;

    PcDiagnosticsCaptureCrash(exception->ExceptionRecord->ExceptionCode, exception, NULL);
    PcPlatformRecordExit(128);
    if (exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
     && exception->ExceptionRecord->NumberParameters >= 2)
        address = exception->ExceptionRecord->ExceptionInformation[1];
    fprintf(stderr,
            "pokeemerald-core exception 0x%08lx at address 0x%08lx (instruction 0x%08lx)\n",
            (unsigned long)exception->ExceptionRecord->ExceptionCode,
            (unsigned long)address,
            (unsigned long)instruction);
    ExitProcess(128);
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD WINAPI WatchdogThread(void *rawSeconds)
{
    DWORD seconds = (DWORD)(uintptr_t)rawSeconds;

    Sleep(seconds * 1000);
    fprintf(stderr, "pokeemerald-core watchdog expired after %lu seconds\n", (unsigned long)seconds);
    TerminateProcess(GetCurrentProcess(), 124);
    return 0;
}
#else
static const int sCrashSignals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGALRM};
static struct sigaction sPreviousSignalActions[sizeof(sCrashSignals) / sizeof(sCrashSignals[0])];
static pid_t sGameThreadId;

static void ChainPreviousSignalHandler(int signalNumber, siginfo_t *info, void *rawContext)
{
    size_t i;

    for (i = 0; i < sizeof(sCrashSignals) / sizeof(sCrashSignals[0]); i++)
    {
        const struct sigaction *previous;

        if (sCrashSignals[i] != signalNumber)
            continue;
        previous = &sPreviousSignalActions[i];
        if (previous->sa_handler == SIG_IGN)
            return;
        if (previous->sa_handler == SIG_DFL)
        {
            sigaction(signalNumber, previous, NULL);
            raise(signalNumber);
            return;
        }
        if (previous->sa_flags & SA_SIGINFO)
            previous->sa_sigaction(signalNumber, info, rawContext);
        else
            previous->sa_handler(signalNumber);
        return;
    }
}

static void CrashHandler(int signalNumber, siginfo_t *info, void *rawContext)
{
    ucontext_t *context = rawContext;
    uintptr_t instruction = 0;

#if PLATFORM_ANDROID
    if ((pid_t)syscall(SYS_gettid) != sGameThreadId)
    {
        ChainPreviousSignalHandler(signalNumber, info, rawContext);
        return;
    }
#endif

#if defined(__i386__) && defined(REG_EIP)
    instruction = context->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__) && defined(REG_RIP)
    instruction = context->uc_mcontext.gregs[REG_RIP];
#elif defined(__aarch64__)
    instruction = context->uc_mcontext.pc;
#endif
    PcDiagnosticsCaptureCrash(signalNumber, info, rawContext);
    PcPlatformRecordExit(128 + signalNumber);
    fprintf(stderr,
            "pokeemerald-core signal %d at address %p (instruction 0x%08lx)\n",
            signalNumber,
            info->si_addr,
            (unsigned long)instruction);
    _Exit(128 + signalNumber);
}
#endif

static void InstallCrashHandlers(void)
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(CrashHandler);
#else
    static unsigned char signalStack[64 * 1024];
    struct sigaction action;
    stack_t stack;
    size_t i;

    memset(&stack, 0, sizeof(stack));
    stack.ss_sp = signalStack;
    stack.ss_size = sizeof(signalStack);
    sigaltstack(&stack, NULL);
    sGameThreadId = (pid_t)syscall(SYS_gettid);
    action.sa_sigaction = CrashHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    for (i = 0; i < sizeof(sCrashSignals) / sizeof(sCrashSignals[0]); i++)
        sigaction(sCrashSignals[i], &action, &sPreviousSignalActions[i]);
#endif
}

#if PLATFORM_ANDROID
__attribute__((visibility("default")))
#endif
int PcCoreMain(const char *sharedPath)
{
    const char *watchdog;

    InstallCrashHandlers();
    if (!PcPlatformInit(sharedPath))
        return 1;

    watchdog = getenv("POKEEMERALD_PC_WATCHDOG");
    if (watchdog != NULL)
    {
        char *end;
        unsigned long seconds = strtoul(watchdog, &end, 10);

        if (*watchdog != '\0' && *end == '\0' && seconds > 0 && seconds <= UINT_MAX / 1000)
#ifdef _WIN32
        {
            HANDLE thread = CreateThread(NULL, 0, WatchdogThread, (void *)(uintptr_t)seconds, 0, NULL);

            if (thread != NULL)
                CloseHandle(thread);
        }
#else
            alarm((unsigned int)seconds);
#endif
    }

    AgbMain();
    PcPlatformShutdown();
    return 0;
}

#if !PLATFORM_ANDROID
int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s SHARED_MEMORY\n", argv[0]);
        return 2;
    }
    return PcCoreMain(argv[1]);
}
#endif
