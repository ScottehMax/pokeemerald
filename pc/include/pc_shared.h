#ifndef GUARD_PC_SHARED_H
#define GUARD_PC_SHARED_H

#include <stdint.h>

#define PC_SHARED_MAGIC 0x50454D45u
#define PC_SHARED_VERSION 9u
#define PC_FRAME_WIDTH 240
#define PC_FRAME_MAX_WIDTH 400
#define PC_FRAME_HEIGHT 160
#define PC_FRAME_BUFFER_COUNT 3
#define PC_PATH_MAX 1024
#define PC_CORE_ERROR_MAX 512
#define PC_AUDIO_RATE 48000
#define PC_AUDIO_BUFFER_FRAMES 32768
#define PC_CORE_EXIT_SOFT_RESET 100
#define PC_CORE_EXIT_PROFILE_SWITCH 101
#define PC_DIAGNOSTIC_STACK_FRAMES 32
#define PC_DIAGNOSTIC_MODULE_NAME 128
#define PC_DIAGNOSTIC_BREADCRUMBS 64
#define PC_DIAGNOSTIC_TASK_DATA 16
#define PC_DIAGNOSTIC_SPRITE_DATA 8
#define PC_PERFORMANCE_STALLS 16

#define PC_CRASH_MAGIC 0x43525348u
#define PC_CRASH_VERSION 2

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

enum PcDiagnosticPhase
{
    PC_DIAGNOSTIC_PHASE_NONE,
    PC_DIAGNOSTIC_PHASE_CALLBACKS,
    PC_DIAGNOSTIC_PHASE_FRAME_HOUSEKEEPING,
    PC_DIAGNOSTIC_PHASE_MAP_MUSIC,
    PC_DIAGNOSTIC_PHASE_WAIT_FRAME,
    PC_DIAGNOSTIC_PHASE_VCOUNT,
    PC_DIAGNOSTIC_PHASE_VBLANK,
    PC_DIAGNOSTIC_PHASE_AUDIO,
    PC_DIAGNOSTIC_PHASE_RENDER,
};

enum PcDiagnosticRenderStage
{
    PC_DIAGNOSTIC_RENDER_NONE,
    PC_DIAGNOSTIC_RENDER_CLEAR,
    PC_DIAGNOSTIC_RENDER_OBJECT_WINDOW,
    PC_DIAGNOSTIC_RENDER_WINDOWS,
    PC_DIAGNOSTIC_RENDER_BACKGROUNDS,
    PC_DIAGNOSTIC_RENDER_SPRITES,
    PC_DIAGNOSTIC_RENDER_OUTPUT,
    PC_DIAGNOSTIC_RENDER_HBLANK_DMA,
    PC_DIAGNOSTIC_RENDER_HBLANK_CALLBACK,
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
    uint32_t phase;
    uint32_t gameThreadId;
    uint32_t renderScanline;
    uint32_t renderStage;
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

enum PcPerformanceStallKind
{
    PC_PERFORMANCE_STALL_NONE,
    PC_PERFORMANCE_STALL_AUDIO_CALLBACK_GAP,
    PC_PERFORMANCE_STALL_AUDIO_UNDERRUN,
    PC_PERFORMANCE_STALL_AUDIO_CATCHUP,
    PC_PERFORMANCE_STALL_FRONTEND_LOOP_GAP,
    PC_PERFORMANCE_STALL_FRONTEND_EVENTS,
    PC_PERFORMANCE_STALL_FRONTEND_TOUCH,
    PC_PERFORMANCE_STALL_FRONTEND_COPY,
    PC_PERFORMANCE_STALL_FRONTEND_PRESENT,
    PC_PERFORMANCE_STALL_CORE_FRAME_GAP,
    PC_PERFORMANCE_STALL_CORE_PPU,
    PC_PERFORMANCE_STALL_FRONTEND_UPLOAD,
};

struct PcPerformanceStall
{
    uint32_t sequence;
    uint32_t frame;
    uint32_t kind;
    uint32_t durationUs;
    uint32_t detail;
};

struct PcPerformanceState
{
    uint32_t coreCallbackUs;
    uint32_t coreVblankUs;
    uint32_t corePpuUs;
    uint32_t coreFrameGapUs;
    uint32_t coreMaxCallbackUs;
    uint32_t coreMaxVblankUs;
    uint32_t coreMaxPpuUs;
    uint32_t coreMaxFrameGapUs;
    uint32_t coreLateFrames;
    uint32_t corePpuCpuUs;
    uint32_t coreMaxPpuCpuUs;
    uint32_t coreSleepOvershootUs;
    uint32_t coreMaxSleepOvershootUs;
    uint32_t coreSleepOvershoots;
    uint32_t coreSlowPpuFrames;
    uint32_t coreMaxPpuFrame;
    int32_t coreMaxPpuMapGroup;
    int32_t coreMaxPpuMapNum;
    uint32_t coreMaxPpuMainCallback;
    uint32_t frontendFrameGapUs;
    uint32_t frontendPresentUs;
    uint32_t frontendMaxFrameGapUs;
    uint32_t frontendMaxPresentUs;
    uint32_t frontendSkippedFrames;
    uint32_t audioCallbackCount;
    uint32_t audioCallbackGapUs;
    uint32_t audioMaxCallbackGapUs;
    uint32_t audioCallbackWorkUs;
    uint32_t audioMaxCallbackWorkUs;
    uint32_t audioQueueFrames;
    uint32_t audioMaxQueueFrames;
    uint32_t audioUnderrunCallbacks;
    uint32_t audioUnderrunFrames;
    uint32_t audioCatchupCallbacks;
    uint32_t audioCatchupSourceFrames;
    uint32_t frontendLoopGapUs;
    uint32_t frontendMaxLoopGapUs;
    uint32_t frontendEventUs;
    uint32_t frontendMaxEventUs;
    uint32_t frontendTouchUs;
    uint32_t frontendMaxTouchUs;
    uint32_t frontendCopyUs;
    uint32_t frontendMaxCopyUs;
    uint32_t frontendUploadUs;
    uint32_t frontendMaxUploadUs;
    uint32_t frontendUploadCpuUs;
    uint32_t frontendMaxUploadCpuUs;
    uint32_t frontendPresentCpuUs;
    uint32_t frontendMaxPresentCpuUs;
    uint32_t frontendMaxSkipFrame;
    uint32_t frontendMaxSkipCount;
    uint32_t frontendMaxSkipGapUs;
    uint32_t frontendMaxSkipEventUs;
    uint32_t frontendMaxSkipWaitUs;
    uint32_t frontendLastSkipFrame;
    uint32_t frontendLastSkipCount;
    uint32_t frontendLastSkipGapUs;
    uint32_t frontendLastSkipEventUs;
    uint32_t frontendLastSkipWaitUs;
    uint32_t stallWrite;
    struct PcPerformanceStall stalls[PC_PERFORMANCE_STALLS];
};

struct __attribute__((aligned(8))) PcCrashRecord
{
    uint32_t magic;
    uint32_t version;
    uint32_t complete;
    uint32_t code;
    uint32_t access;
    uint32_t threadId;
    uint64_t faultAddress;
    uint64_t instruction;
    uint64_t eax;
    uint64_t ebx;
    uint64_t ecx;
    uint64_t edx;
    uint64_t esi;
    uint64_t edi;
    uint64_t ebp;
    uint64_t esp;
    uint64_t link;
    uint64_t eflags;
    uint64_t nativeModuleBase;
    char nativeModule[PC_DIAGNOSTIC_MODULE_NAME];
    uint32_t stackFrameCount;
    uint32_t stackReserved;
    uint64_t stackFrames[PC_DIAGNOSTIC_STACK_FRAMES];
};

struct PcSharedState
{
    uint32_t magic;
    uint32_t version;
    uint32_t quit;
    uint32_t paused;
    uint32_t keys;
    uint32_t frameSequence;
    uint32_t frameBufferIndex;
    uint32_t requestedFrameWidth;
    uint32_t frameWidth;
    uint32_t frameHeight;
    uint32_t coreReady;
    uint32_t coreError;
    uint32_t coreExited;
    int32_t coreExitStatus;
    uint32_t coreLoadBase;
    char coreErrorMessage[PC_CORE_ERROR_MAX];
    uint32_t resumeMainMenu;
    char defaultSavePath[PC_PATH_MAX];
    char savePath[PC_PATH_MAX];
    char storagePath[PC_PATH_MAX];
    char requestedSavePath[PC_PATH_MAX];
    char requestedStoragePath[PC_PATH_MAX];
    uint32_t audioRead;
    uint32_t audioWrite;
    uint32_t audioFramesGenerated;
    uint32_t audioPeak;
    uint32_t audioSamplesNonzero;
    uint32_t audioSamplesClipped;
    uint32_t testBattleState;
    uint32_t testBattleOutcome;
    struct PcPerformanceState performance;
    struct PcDiagnosticState diagnostics;
    struct PcCrashRecord crash;
    int16_t audio[PC_AUDIO_BUFFER_FRAMES * 2];
    uint32_t pixels[PC_FRAME_BUFFER_COUNT][PC_FRAME_MAX_WIDTH * PC_FRAME_HEIGHT];
};

#endif
