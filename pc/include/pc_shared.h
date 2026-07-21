#ifndef GUARD_PC_SHARED_H
#define GUARD_PC_SHARED_H

#include <stdint.h>

#define PC_SHARED_MAGIC 0x50454D45u
#define PC_FRAME_WIDTH 240
#define PC_FRAME_HEIGHT 160
#define PC_FRAME_BUFFER_COUNT 3
#define PC_PATH_MAX 1024
#define PC_AUDIO_RATE 48000
#define PC_AUDIO_BUFFER_FRAMES 32768
#define PC_CORE_EXIT_SOFT_RESET 100
#define PC_DIAGNOSTIC_STACK_FRAMES 32
#define PC_DIAGNOSTIC_BREADCRUMBS 64
#define PC_DIAGNOSTIC_TASK_DATA 16
#define PC_DIAGNOSTIC_SPRITE_DATA 8

#define PC_CRASH_MAGIC 0x43525348u
#define PC_CRASH_VERSION 1

enum PcCrashAccess
{
    PC_CRASH_ACCESS_UNKNOWN,
    PC_CRASH_ACCESS_READ,
    PC_CRASH_ACCESS_WRITE,
    PC_CRASH_ACCESS_EXECUTE,
};

enum PcDiagnosticDispatch
{
    PC_DIAGNOSTIC_DISPATCH_NONE,
    PC_DIAGNOSTIC_DISPATCH_MAIN_1,
    PC_DIAGNOSTIC_DISPATCH_MAIN_2,
    PC_DIAGNOSTIC_DISPATCH_TASK,
    PC_DIAGNOSTIC_DISPATCH_SPRITE,
    PC_DIAGNOSTIC_DISPATCH_SCRIPT_NATIVE,
    PC_DIAGNOSTIC_DISPATCH_SCRIPT_COMMAND,
};

struct PcDiagnosticBreadcrumb
{
    uint32_t frame;
    uint32_t kind;
    uint32_t id;
    uint32_t address;
};

struct PcDiagnosticState
{
    uint32_t frame;
    uint32_t mainCallback1;
    uint32_t mainCallback2;
    uint32_t currentMainKind;
    uint32_t currentMainCallback;
    uint32_t currentTaskId;
    uint32_t currentTaskFunc;
    int16_t currentTaskData[PC_DIAGNOSTIC_TASK_DATA];
    uint32_t currentSpriteId;
    uint32_t currentSpriteCallback;
    int16_t currentSpriteData[PC_DIAGNOSTIC_SPRITE_DATA];
    uint32_t currentScriptPtr;
    uint32_t currentScriptCommand;
    uint32_t currentScriptFunc;
    uint32_t invalidKind;
    uint32_t invalidId;
    uint32_t invalidAddress;
    int32_t mapGroup;
    int32_t mapNum;
    int32_t mapX;
    int32_t mapY;
    uint32_t inBattle;
    uint32_t battleTypeFlags;
    uint32_t battleMainFunc;
    uint32_t battleControllerFlags;
    uint32_t activeBattler;
    uint32_t battlersCount;
    uint32_t battleOutcome;
    uint32_t linkType;
    uint32_t linkPlayersReceived;
    uint32_t wirelessCommType;
    uint32_t breadcrumbWrite;
    struct PcDiagnosticBreadcrumb breadcrumbs[PC_DIAGNOSTIC_BREADCRUMBS];
};

struct PcCrashRecord
{
    uint32_t magic;
    uint32_t version;
    uint32_t complete;
    uint32_t code;
    uint32_t access;
    uint32_t faultAddress;
    uint32_t instruction;
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t eflags;
    uint32_t stackFrameCount;
    uint32_t stackFrames[PC_DIAGNOSTIC_STACK_FRAMES];
};

struct PcSharedState
{
    uint32_t magic;
    uint32_t quit;
    uint32_t keys;
    uint32_t frameSequence;
    uint32_t frameBufferIndex;
    uint32_t coreReady;
    uint32_t coreError;
    char savePath[PC_PATH_MAX];
    uint32_t audioRead;
    uint32_t audioWrite;
    uint32_t audioFramesGenerated;
    uint32_t audioPeak;
    uint32_t audioSamplesNonzero;
    uint32_t audioSamplesClipped;
    uint32_t testBattleState;
    uint32_t testBattleOutcome;
    struct PcDiagnosticState diagnostics;
    struct PcCrashRecord crash;
    int16_t audio[PC_AUDIO_BUFFER_FRAMES * 2];
    uint32_t pixels[PC_FRAME_BUFFER_COUNT][PC_FRAME_WIDTH * PC_FRAME_HEIGHT];
};

#endif
