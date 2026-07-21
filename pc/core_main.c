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
static void CrashHandler(int signalNumber, siginfo_t *info, void *rawContext)
{
    ucontext_t *context = rawContext;
    uintptr_t instruction = 0;

#if defined(__i386__) && defined(REG_EIP)
    instruction = context->uc_mcontext.gregs[REG_EIP];
#endif
    PcDiagnosticsCaptureCrash(signalNumber, info, rawContext);
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
    static const int signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGALRM};
    static unsigned char signalStack[64 * 1024];
    struct sigaction action;
    stack_t stack;
    size_t i;

    memset(&stack, 0, sizeof(stack));
    stack.ss_sp = signalStack;
    stack.ss_size = sizeof(signalStack);
    sigaltstack(&stack, NULL);
    action.sa_sigaction = CrashHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    for (i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
        sigaction(signals[i], &action, NULL);
#endif
}

int main(int argc, char **argv)
{
    const char *watchdog;

    InstallCrashHandlers();
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s SHARED_MEMORY\n", argv[0]);
        return 2;
    }

    if (!PcPlatformInit(argv[1]))
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
