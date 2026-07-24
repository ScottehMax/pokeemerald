#include <SDL.h>
#include <SDL_system.h>
#include <GLES3/gl3.h>

#include "pc_crash_report.h"
#include "pc_profiles.h"
#include "pc_shared.h"

#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <linux/futex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#define A_BUTTON 0x0001u
#define B_BUTTON 0x0002u
#define SELECT_BUTTON 0x0004u
#define START_BUTTON 0x0008u
#define DPAD_RIGHT 0x0010u
#define DPAD_LEFT 0x0020u
#define DPAD_UP 0x0040u
#define DPAD_DOWN 0x0080u
#define R_BUTTON 0x0100u
#define L_BUTTON 0x0200u
#define CORE_START_TIMEOUT_MS 10000u
#define CORE_START_RETRY_MS 100u
#define AUDIO_CALLBACK_FRAMES 512u
#define AUDIO_TARGET_FRAMES 1024u
#define AUDIO_PREBUFFER_TIMEOUT_MS 250u
#define PERFORMANCE_REPORT_HOLD_MS 1500u
#define MAX_EVENTS_PER_ITERATION 64
#define AUDIO_GAP_EVENT_THRESHOLD_US 20000u
#define AUDIO_UNDERRUN_EVENT_INTERVAL_NS 100000000ull
#define FRONTEND_LOOP_EVENT_THRESHOLD_US 25000u
#define FRONTEND_PHASE_EVENT_THRESHOLD_US 5000u
#define FRONTEND_PRESENT_EVENT_THRESHOLD_US 20000u
#define FRONTEND_TEXTURE_COUNT 6
#define FRONTEND_UPLOAD_BUFFER_COUNT 6
#define ANDROID_OPENSL_BUFFER_COUNT 3
#define ANDROID_FRAME_WAIT "futex"
#define ANDROID_BUILD_REVISION "0.2"

static uint64_t sLastAudioCallbackNs;
static uint64_t sLastAudioUnderrunEventNs;
static const char *sTextureUploadBackend = "sdl-subimage";
static struct PcSharedState *sFrontendSharedState;
static uint32_t sLifecyclePaused;
static uint32_t sLifecycleCallbackActive;

JNIEXPORT void JNICALL
Java_org_pokeemerald_pc_PokemonEmeraldActivity_nativeNotifyPaused(
    JNIEnv *env,
    jclass activityClass)
{
    struct PcSharedState *shared;

    (void)env;
    (void)activityClass;
    __atomic_store_n(&sLifecyclePaused, 1, __ATOMIC_RELEASE);
    __atomic_add_fetch(&sLifecycleCallbackActive, 1, __ATOMIC_ACQUIRE);
    shared = __atomic_load_n(&sFrontendSharedState, __ATOMIC_ACQUIRE);
    if (shared != NULL)
        __atomic_store_n(&shared->paused, 1, __ATOMIC_RELEASE);
    __atomic_sub_fetch(&sLifecycleCallbackActive, 1, __ATOMIC_RELEASE);
}

struct TextureUploader
{
    GLuint buffers[FRONTEND_UPLOAD_BUFFER_COUNT];
    uint32_t nextBuffer;
    int usePixelBuffers;
};

static void ClearGlErrors(void)
{
    while (glGetError() != GL_NO_ERROR)
        ;
}

static void DestroyTextureUploader(struct TextureUploader *uploader)
{
    if (uploader->buffers[0] != 0)
        glDeleteBuffers(FRONTEND_UPLOAD_BUFFER_COUNT, uploader->buffers);
    memset(uploader, 0, sizeof(*uploader));
}

static void InitTextureUploader(struct TextureUploader *uploader)
{
    int contextMajor = 0;
    int buffer;

    memset(uploader, 0, sizeof(*uploader));
    if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &contextMajor) != 0
     || contextMajor < 3)
        return;

    ClearGlErrors();
    glGenBuffers(FRONTEND_UPLOAD_BUFFER_COUNT, uploader->buffers);
    for (buffer = 0; buffer < FRONTEND_UPLOAD_BUFFER_COUNT; buffer++)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, uploader->buffers[buffer]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER,
                     PC_FRAME_WIDTH * PC_FRAME_HEIGHT * (int)sizeof(uint32_t),
                     NULL,
                     GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    if (glGetError() != GL_NO_ERROR)
    {
        DestroyTextureUploader(uploader);
        return;
    }

    uploader->usePixelBuffers = 1;
    sTextureUploadBackend = "pbo-subimage";
}

static int UploadTextureFrame(struct TextureUploader *uploader,
                              SDL_Texture *texture,
                              const uint32_t *pixels)
{
    int result;

    if (!uploader->usePixelBuffers)
        return SDL_UpdateTexture(texture,
                                 NULL,
                                 pixels,
                                 PC_FRAME_WIDTH * (int)sizeof(uint32_t));

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER,
                 uploader->buffers[uploader->nextBuffer]);
    uploader->nextBuffer =
        (uploader->nextBuffer + 1) % FRONTEND_UPLOAD_BUFFER_COUNT;
    glBufferData(GL_PIXEL_UNPACK_BUFFER,
                 PC_FRAME_WIDTH * PC_FRAME_HEIGHT * (int)sizeof(uint32_t),
                 pixels,
                 GL_STREAM_DRAW);
    if (SDL_GL_BindTexture(texture, NULL, NULL) != 0)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        return -1;
    }
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    PC_FRAME_WIDTH,
                    PC_FRAME_HEIGHT,
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    NULL);
    result = SDL_GL_UnbindTexture(texture);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    return result;
}

static uint64_t GetMonotonicNs(void)
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000000000ull + (uint64_t)time.tv_nsec;
}

static uint64_t GetThreadCpuNs(void)
{
    struct timespec time;

    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time);
    return (uint64_t)time.tv_sec * 1000000000ull + (uint64_t)time.tv_nsec;
}

static void WaitForFrameSignal(struct PcSharedState *shared, uint32_t expectedFrame)
{
    const struct timespec timeout = {0, 25000000};

    syscall(SYS_futex,
            &shared->frameSequence,
            FUTEX_WAIT,
            expectedFrame,
            &timeout,
            NULL,
            0);
}

static void StoreMaximum(uint32_t *target, uint32_t value)
{
    uint32_t previous = __atomic_load_n(target, __ATOMIC_RELAXED);

    while (value > previous
        && !__atomic_compare_exchange_n(target,
                                        &previous,
                                        value,
                                        0,
                                        __ATOMIC_RELAXED,
                                        __ATOMIC_RELAXED))
    {
    }
}

static void RecordPerformanceStall(struct PcSharedState *shared,
                                   uint32_t kind,
                                   uint32_t durationUs,
                                   uint32_t detail)
{
    struct PcPerformanceState *performance = &shared->performance;
    uint32_t sequence = __atomic_fetch_add(&performance->stallWrite, 1, __ATOMIC_RELAXED) + 1;
    struct PcPerformanceStall *stall =
        &performance->stalls[(sequence - 1) % PC_PERFORMANCE_STALLS];

    __atomic_store_n(&stall->sequence, 0, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->frame,
                     __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE),
                     __ATOMIC_RELAXED);
    __atomic_store_n(&stall->kind, kind, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->durationUs, durationUs, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->detail, detail, __ATOMIC_RELAXED);
    __atomic_store_n(&stall->sequence, sequence, __ATOMIC_RELEASE);
}

static void AudioCallback(void *userdata, Uint8 *stream, int length)
{
    struct PcSharedState *shared = userdata;
    struct PcPerformanceState *performance = &shared->performance;
    uint64_t callbackStartNs = GetMonotonicNs();
    uint32_t requested = (uint32_t)length / (sizeof(int16_t) * 2);
    uint32_t read = __atomic_load_n(&shared->audioRead, __ATOMIC_RELAXED);
    uint32_t write = __atomic_load_n(&shared->audioWrite, __ATOMIC_ACQUIRE);
    uint32_t available = write - read;
    uint32_t count;
    uint32_t offset;
    uint32_t first;

    __atomic_add_fetch(&performance->audioCallbackCount, 1, __ATOMIC_RELAXED);
    StoreMaximum(&performance->audioMaxQueueFrames, available);
    if (sLastAudioCallbackNs != 0)
    {
        uint32_t gapUs = (uint32_t)((callbackStartNs - sLastAudioCallbackNs) / 1000);

        __atomic_store_n(&performance->audioCallbackGapUs, gapUs, __ATOMIC_RELAXED);
        StoreMaximum(&performance->audioMaxCallbackGapUs, gapUs);
        if (gapUs >= AUDIO_GAP_EVENT_THRESHOLD_US)
            RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_AUDIO_CALLBACK_GAP,
                                   gapUs, available);
    }
    sLastAudioCallbackNs = callbackStartNs;

    count = requested < available ? requested : available;
    offset = read & (PC_AUDIO_BUFFER_FRAMES - 1);
    first = count < PC_AUDIO_BUFFER_FRAMES - offset
          ? count
          : PC_AUDIO_BUFFER_FRAMES - offset;

    memcpy(stream, &shared->audio[offset * 2], first * 2 * sizeof(int16_t));
    if (count > first)
        memcpy(stream + first * 2 * sizeof(int16_t),
               shared->audio,
               (count - first) * 2 * sizeof(int16_t));
    if (count < requested)
    {
        uint32_t missing = requested - count;

        memset(stream + count * 2 * sizeof(int16_t),
               0,
               missing * 2 * sizeof(int16_t));
        __atomic_add_fetch(&performance->audioUnderrunCallbacks, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&performance->audioUnderrunFrames, missing, __ATOMIC_RELAXED);
        if (callbackStartNs - sLastAudioUnderrunEventNs >= AUDIO_UNDERRUN_EVENT_INTERVAL_NS)
        {
            RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_AUDIO_UNDERRUN,
                                   0, missing);
            sLastAudioUnderrunEventNs = callbackStartNs;
        }
    }
    __atomic_store_n(&shared->audioRead, read + count, __ATOMIC_RELEASE);
    __atomic_store_n(&performance->audioQueueFrames, available - count,
                     __ATOMIC_RELAXED);
    {
        uint32_t workUs = (uint32_t)((GetMonotonicNs() - callbackStartNs) / 1000);

        __atomic_store_n(&performance->audioCallbackWorkUs, workUs, __ATOMIC_RELAXED);
        StoreMaximum(&performance->audioMaxCallbackWorkUs, workUs);
    }
}

static void DiscardQueuedAudio(struct PcSharedState *shared)
{
    uint32_t write = __atomic_load_n(&shared->audioWrite, __ATOMIC_ACQUIRE);

    __atomic_store_n(&shared->audioRead, write, __ATOMIC_RELEASE);
    sLastAudioCallbackNs = 0;
    sLastAudioUnderrunEventNs = 0;
}

static void ResumeAudioWhenBuffered(struct PcSharedState *shared, SDL_AudioDeviceID audioDevice)
{
    Uint32 start;

    if (audioDevice == 0)
        return;
    start = SDL_GetTicks();
    while (SDL_GetTicks() - start < AUDIO_PREBUFFER_TIMEOUT_MS
        && !__atomic_load_n(&shared->coreExited, __ATOMIC_ACQUIRE))
    {
        uint32_t read = __atomic_load_n(&shared->audioRead, __ATOMIC_ACQUIRE);
        uint32_t write = __atomic_load_n(&shared->audioWrite, __ATOMIC_ACQUIRE);

        if (write - read >= AUDIO_TARGET_FRAMES)
            break;
        SDL_Delay(2);
    }
    SDL_PauseAudioDevice(audioDevice, 0);
}

static int CallActivityMethod(const char *name, const char *argument)
{
    JNIEnv *env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    jclass activityClass;
    jmethodID method;
    jstring javaArgument = NULL;

    if (env == NULL || activity == NULL)
        return -1;
    activityClass = (*env)->GetObjectClass(env, activity);
    method = (*env)->GetMethodID(env,
                                 activityClass,
                                 name,
                                 argument == NULL ? "()V" : "(Ljava/lang/String;)V");
    if (method == NULL)
        return -1;
    if (argument != NULL)
    {
        javaArgument = (*env)->NewStringUTF(env, argument);
        if (javaArgument == NULL)
            return -1;
        (*env)->CallVoidMethod(env, activity, method, javaArgument);
        (*env)->DeleteLocalRef(env, javaArgument);
    }
    else
    {
        (*env)->CallVoidMethod(env, activity, method);
    }
    (*env)->DeleteLocalRef(env, activityClass);
    (*env)->DeleteLocalRef(env, activity);
    return (*env)->ExceptionCheck(env) ? -1 : 0;
}

static int GetActivityString(const char *name, char *output, size_t outputSize)
{
    JNIEnv *env = SDL_AndroidGetJNIEnv();
    jobject activity = SDL_AndroidGetActivity();
    jclass activityClass = NULL;
    jmethodID method;
    jstring value = NULL;
    const char *textValue = NULL;
    int result = -1;

    if (env == NULL || activity == NULL)
        goto cleanup;
    activityClass = (*env)->GetObjectClass(env, activity);
    if (activityClass == NULL)
        goto cleanup;
    method = (*env)->GetMethodID(env, activityClass, name, "()Ljava/lang/String;");
    if (method == NULL)
        goto cleanup;
    value = (jstring)(*env)->CallObjectMethod(env, activity, method);
    if ((*env)->ExceptionCheck(env) || value == NULL)
        goto cleanup;
    textValue = (*env)->GetStringUTFChars(env, value, NULL);
    if (textValue == NULL)
        goto cleanup;
    if (snprintf(output, outputSize, "%s", textValue) < (int)outputSize)
        result = 0;

cleanup:
    if (textValue != NULL)
        (*env)->ReleaseStringUTFChars(env, value, textValue);
    if (value != NULL)
        (*env)->DeleteLocalRef(env, value);
    if (activityClass != NULL)
        (*env)->DeleteLocalRef(env, activityClass);
    if (activity != NULL)
        (*env)->DeleteLocalRef(env, activity);
    if (env != NULL && (*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    return result;
}

static int WritePerformanceReport(const char *path,
                                  const struct PcSharedState *shared,
                                  SDL_Renderer *renderer)
{
    const struct PcPerformanceState *performance = &shared->performance;
    const char *audioDriver = SDL_GetCurrentAudioDriver();
    const char *videoDriver = SDL_GetCurrentVideoDriver();
    SDL_RendererInfo rendererInfo;
    const char *rendererDriver = "unknown";
    int rendererVsync = 0;
    int contextMajor = 0;
    int contextMinor = 0;
    uint32_t reportFrame = __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);
    uint32_t stallWrite;
    uint32_t firstStall;
    uint32_t sequence;
    FILE *file = fopen(path, "w");

    if (renderer != NULL && SDL_GetRendererInfo(renderer, &rendererInfo) == 0)
    {
        rendererDriver = rendererInfo.name != NULL ? rendererInfo.name : "unknown";
        rendererVsync = (rendererInfo.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    }
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &contextMajor);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &contextMinor);
    if (file == NULL)
        return -1;
    fprintf(file,
            "pokeemerald-pc performance report\n"
            "format: 18\n"
            "build: " ANDROID_BUILD_REVISION "\n"
            "shared_abi: %u\n"
            "audio_driver: %s\n"
            "audio_backend_buffers: %u\n"
            "video_driver: %s\n"
            "render_driver: %s\n"
            "renderer_vsync: %s\n"
            "gles_version: %d.%d\n"
            "texture_upload: %s\n"
            "frame_wait: " ANDROID_FRAME_WAIT "\n"
            "frontend_texture_count: %u\n"
            "frame: %u\n"
            "core_callback_us: %u (max %u)\n"
            "core_vblank_us: %u (max %u)\n"
            "core_ppu_us: %u (max %u)\n"
            "core_ppu_cpu_us: %u (max %u)\n"
            "core_slow_ppu_frames: %u\n"
            "core_max_ppu_context: frame=%u map=%d.%d main_callback_2=0x%08x\n"
            "core_sleep_overshoot_us: %u (max %u, over_1ms %u)\n"
            "core_frame_gap_us: %u (max %u)\n"
            "core_missed_deadlines: %u\n"
            "frontend_frame_gap_us: %u (max %u)\n"
            "frontend_present_us: %u (max %u)\n"
            "frontend_skipped_frames: %u\n"
            "frontend_max_skip: frame=%u count=%u gap_us=%u event_us=%u wait_us=%u\n"
            "frontend_last_skip: frame=%u count=%u gap_us=%u event_us=%u wait_us=%u\n"
            "frontend_loop_gap_us: %u (max %u)\n"
            "frontend_event_us: %u (max %u)\n"
            "frontend_touch_us: %u (max %u)\n"
            "frontend_copy_us: %u (max %u)\n"
            "frontend_upload_us: %u (max %u)\n"
            "frontend_upload_cpu_us: %u (max %u)\n"
            "frontend_present_cpu_us: %u (max %u)\n"
            "audio_callback_gap_us: %u (max %u)\n"
            "audio_callback_work_us: %u (max %u)\n"
            "audio_callbacks: %u\n"
            "audio_queue_frames: %u (max %u)\n"
            "audio_underruns: %u callbacks, %u frames\n"
            "audio_backlog_drops: %u callbacks, %u frames\n"
            "map: %d.%d position=%d,%d\n"
            "main_callback_2: 0x%08x\n"
            "frame_phase: %u render_scanline=%u render_stage=%u\n",
            shared->version,
            audioDriver != NULL ? audioDriver : "unknown",
            ANDROID_OPENSL_BUFFER_COUNT,
            videoDriver != NULL ? videoDriver : "unknown",
            rendererDriver,
            rendererVsync ? "yes" : "no",
            contextMajor,
            contextMinor,
            sTextureUploadBackend,
            FRONTEND_TEXTURE_COUNT,
            reportFrame,
            __atomic_load_n(&performance->coreCallbackUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxCallbackUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreVblankUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxVblankUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->corePpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->corePpuCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreSlowPpuFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuFrame, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuMapGroup, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuMapNum, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxPpuMainCallback, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreSleepOvershootUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxSleepOvershootUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreSleepOvershoots, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreFrameGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreMaxFrameGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->coreLateFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendFrameGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxFrameGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendPresentUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxPresentUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendSkippedFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxSkipFrame, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxSkipCount, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxSkipGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxSkipEventUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxSkipWaitUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLastSkipFrame, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLastSkipCount, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLastSkipGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLastSkipEventUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLastSkipWaitUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendLoopGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxLoopGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendEventUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxEventUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendTouchUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxTouchUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendCopyUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxCopyUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendUploadUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxUploadUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendUploadCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxUploadCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendPresentCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->frontendMaxPresentCpuUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioCallbackGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioMaxCallbackGapUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioCallbackWorkUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioMaxCallbackWorkUs, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioCallbackCount, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioQueueFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioMaxQueueFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioUnderrunCallbacks, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioUnderrunFrames, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioBacklogDrops, __ATOMIC_RELAXED),
            __atomic_load_n(&performance->audioBacklogFramesDropped, __ATOMIC_RELAXED),
            shared->diagnostics.mapGroup,
            shared->diagnostics.mapNum,
            shared->diagnostics.mapX,
            shared->diagnostics.mapY,
            shared->diagnostics.mainCallback2,
            shared->diagnostics.phase,
            shared->diagnostics.renderScanline,
            shared->diagnostics.renderStage);

    stallWrite = __atomic_load_n(&performance->stallWrite, __ATOMIC_ACQUIRE);
    firstStall = stallWrite > 12 ? stallWrite - 11 : 1;
    fprintf(file, "\nRecent Stalls (up to 12)\n");
    for (sequence = firstStall; sequence <= stallWrite; sequence++)
    {
        const struct PcPerformanceStall *stall =
            &performance->stalls[(sequence - 1) % PC_PERFORMANCE_STALLS];
        uint32_t storedSequence = __atomic_load_n(&stall->sequence, __ATOMIC_ACQUIRE);
        uint32_t frame;
        uint32_t kind;
        uint32_t durationUs;
        uint32_t detail;
        const char *name;

        if (storedSequence != sequence)
            continue;
        frame = __atomic_load_n(&stall->frame, __ATOMIC_RELAXED);
        kind = __atomic_load_n(&stall->kind, __ATOMIC_RELAXED);
        durationUs = __atomic_load_n(&stall->durationUs, __ATOMIC_RELAXED);
        detail = __atomic_load_n(&stall->detail, __ATOMIC_RELAXED);
        if (__atomic_load_n(&stall->sequence, __ATOMIC_ACQUIRE) != sequence)
            continue;

        switch (kind)
        {
        case PC_PERFORMANCE_STALL_AUDIO_CALLBACK_GAP:
            name = "audio_callback_gap";
            break;
        case PC_PERFORMANCE_STALL_AUDIO_UNDERRUN:
            name = "audio_underrun";
            break;
        case PC_PERFORMANCE_STALL_AUDIO_BACKLOG_DROP:
            name = "audio_backlog_drop";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_LOOP_GAP:
            name = "frontend_loop_gap";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_EVENTS:
            name = "frontend_events";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_TOUCH:
            name = "frontend_touch";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_COPY:
            name = "frontend_copy";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_PRESENT:
            name = "frontend_present";
            break;
        case PC_PERFORMANCE_STALL_CORE_FRAME_GAP:
            name = "core_frame_gap";
            break;
        case PC_PERFORMANCE_STALL_CORE_PPU:
            name = "core_ppu";
            break;
        case PC_PERFORMANCE_STALL_FRONTEND_UPLOAD:
            name = "frontend_upload";
            break;
        default:
            name = "unknown";
            break;
        }
        fprintf(file,
                "frame=%u age=%u kind=%s duration_us=%u detail=%u\n",
                frame,
                reportFrame - frame,
                name,
                durationUs,
                detail);
    }
    return fclose(file);
}

static int StartGameCore(struct PcSharedState *shared, const char *sharedPath)
{
    Uint32 start = SDL_GetTicks();
    Uint32 nextRequest = start;

    while (SDL_GetTicks() - start < CORE_START_TIMEOUT_MS)
    {
        Uint32 now = SDL_GetTicks();

        if (__atomic_load_n(&shared->coreReady, __ATOMIC_ACQUIRE)
         || __atomic_load_n(&shared->coreExited, __ATOMIC_ACQUIRE))
            return 0;
        if ((Sint32)(now - nextRequest) >= 0)
        {
            if (CallActivityMethod("startGameCore", sharedPath) != 0)
            {
                snprintf(shared->coreErrorMessage,
                         sizeof(shared->coreErrorMessage),
                         "could not request the Android game core service");
                __atomic_store_n(&shared->coreExitStatus, 1, __ATOMIC_RELAXED);
                __atomic_store_n(&shared->coreExited, 1, __ATOMIC_RELEASE);
                return 0;
            }
            nextRequest = now + CORE_START_RETRY_MS;
        }
        SDL_Delay(10);
    }

    snprintf(shared->coreErrorMessage,
             sizeof(shared->coreErrorMessage),
             "timed out waiting for the Android game core service");
    __atomic_store_n(&shared->coreExitStatus, 1, __ATOMIC_RELAXED);
    __atomic_store_n(&shared->coreExited, 1, __ATOMIC_RELEASE);
    return 0;
}

static void ResetSharedState(struct PcSharedState *shared,
                             const char *defaultSavePath,
                             const char *savePath,
                             const char *storagePath,
                             int resumeMainMenu,
                             SDL_AudioDeviceID audioDevice)
{
    if (audioDevice != 0)
        SDL_PauseAudioDevice(audioDevice, 1);
    sLastAudioCallbackNs = 0;
    sLastAudioUnderrunEventNs = 0;
    memset(shared, 0, sizeof(*shared));
    shared->magic = PC_SHARED_MAGIC;
    shared->version = PC_SHARED_VERSION;
    shared->resumeMainMenu = (uint32_t)resumeMainMenu;
    snprintf(shared->defaultSavePath, sizeof(shared->defaultSavePath), "%s", defaultSavePath);
    snprintf(shared->savePath, sizeof(shared->savePath), "%s", savePath);
    snprintf(shared->storagePath, sizeof(shared->storagePath), "%s", storagePath);
}

static int GetStoragePath(const char *savePath, char *storagePath, size_t storagePathSize)
{
    const char *separator = strrchr(savePath, '/');
    size_t prefixLength;

    if (separator == NULL)
        return -1;
    prefixLength = (size_t)(separator - savePath + 1);
    if (prefixLength + sizeof("storage") > storagePathSize)
        return -1;
    memcpy(storagePath, savePath, prefixLength);
    memcpy(storagePath + prefixLength, "storage", sizeof("storage"));
    return 0;
}

static uint32_t ReadKeyboard(void)
{
    const Uint8 *keyboard = SDL_GetKeyboardState(NULL);
    uint32_t keys = 0;

    if (keyboard[SDL_SCANCODE_X]) keys |= A_BUTTON;
    if (keyboard[SDL_SCANCODE_Z] || keyboard[SDL_SCANCODE_ESCAPE]) keys |= B_BUTTON;
    if (keyboard[SDL_SCANCODE_BACKSPACE]) keys |= SELECT_BUTTON;
    if (keyboard[SDL_SCANCODE_RETURN]) keys |= START_BUTTON;
    if (keyboard[SDL_SCANCODE_RIGHT]) keys |= DPAD_RIGHT;
    if (keyboard[SDL_SCANCODE_LEFT]) keys |= DPAD_LEFT;
    if (keyboard[SDL_SCANCODE_UP]) keys |= DPAD_UP;
    if (keyboard[SDL_SCANCODE_DOWN]) keys |= DPAD_DOWN;
    if (keyboard[SDL_SCANCODE_S]) keys |= R_BUTTON;
    if (keyboard[SDL_SCANCODE_A]) keys |= L_BUTTON;
    return keys;
}

static uint32_t ReadController(SDL_GameController *controller)
{
    uint32_t keys = 0;

    if (controller == NULL)
        return 0;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A)) keys |= A_BUTTON;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B)) keys |= B_BUTTON;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK)) keys |= SELECT_BUTTON;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START)) keys |= START_BUTTON;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) keys |= DPAD_RIGHT;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) keys |= DPAD_LEFT;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) keys |= DPAD_UP;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) keys |= DPAD_DOWN;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) keys |= R_BUTTON;
    if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) keys |= L_BUTTON;
    return keys;
}

static uint32_t KeysAtPoint(float x, float y)
{
    uint32_t keys = 0;
    float dx = x - 0.16f;
    float dy = y - 0.70f;

    if (dx * dx + dy * dy < 0.030f)
    {
        if (dx > 0.035f) keys |= DPAD_RIGHT;
        if (dx < -0.035f) keys |= DPAD_LEFT;
        if (dy > 0.035f) keys |= DPAD_DOWN;
        if (dy < -0.035f) keys |= DPAD_UP;
    }
    dx = x - 0.86f;
    dy = y - 0.63f;
    if (dx * dx + dy * dy < 0.010f) keys |= A_BUTTON;
    dx = x - 0.73f;
    dy = y - 0.76f;
    if (dx * dx + dy * dy < 0.010f) keys |= B_BUTTON;
    if (x > 0.45f && x < 0.55f && y > 0.84f) keys |= START_BUTTON;
    if (x > 0.33f && x < 0.43f && y > 0.84f) keys |= SELECT_BUTTON;
    if (x < 0.28f && y < 0.18f) keys |= L_BUTTON;
    if (x > 0.72f && y < 0.18f) keys |= R_BUTTON;
    return keys;
}

static uint32_t ReadTouch(void)
{
    uint32_t keys = 0;
    int deviceCount = SDL_GetNumTouchDevices();
    int deviceIndex;

    for (deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++)
    {
        SDL_TouchID device = SDL_GetTouchDevice(deviceIndex);
        int fingerCount = SDL_GetNumTouchFingers(device);
        int fingerIndex;

        for (fingerIndex = 0; fingerIndex < fingerCount; fingerIndex++)
        {
            const SDL_Finger *finger = SDL_GetTouchFinger(device, fingerIndex);

            if (finger != NULL)
                keys |= KeysAtPoint(finger->x, finger->y);
        }
    }
    return keys;
}

static void DrawRect(SDL_Renderer *renderer,
                     int x,
                     int y,
                     int width,
                     int height,
                     SDL_Color color)
{
    SDL_Rect rect = {x, y, width, height};

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static void DrawControls(SDL_Renderer *renderer, int width, int height, uint32_t keys)
{
    SDL_Color idle = {20, 24, 28, 105};
    SDL_Color pressed = {238, 238, 238, 175};
    int unit = height / 13;
    int cx = width * 16 / 100;
    int cy = height * 70 / 100;
    int aX = width * 86 / 100;
    int aY = height * 63 / 100;
    int bX = width * 73 / 100;
    int bY = height * 76 / 100;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    DrawRect(renderer, cx - unit / 2, cy - unit * 2, unit, unit * 4,
             keys & (DPAD_UP | DPAD_DOWN) ? pressed : idle);
    DrawRect(renderer, cx - unit * 2, cy - unit / 2, unit * 4, unit,
             keys & (DPAD_LEFT | DPAD_RIGHT) ? pressed : idle);
    DrawRect(renderer, aX - unit, aY - unit, unit * 2, unit * 2,
             keys & A_BUTTON ? pressed : idle);
    DrawRect(renderer, bX - unit, bY - unit, unit * 2, unit * 2,
             keys & B_BUTTON ? pressed : idle);
    DrawRect(renderer, width * 45 / 100, height * 88 / 100, width / 10, unit / 2,
             keys & START_BUTTON ? pressed : idle);
    DrawRect(renderer, width * 33 / 100, height * 88 / 100, width / 10, unit / 2,
             keys & SELECT_BUTTON ? pressed : idle);
    DrawRect(renderer, width / 40, height / 30, width / 4, unit,
             keys & L_BUTTON ? pressed : idle);
    DrawRect(renderer, width * 29 / 40, height / 30, width / 4, unit,
             keys & R_BUTTON ? pressed : idle);
}

static int CopyStableFrame(const struct PcSharedState *shared, uint32_t *pixels, uint32_t *frame)
{
    uint32_t before;
    uint32_t after;
    uint32_t buffer;

    do
    {
        before = __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);
        buffer = __atomic_load_n(&shared->frameBufferIndex, __ATOMIC_ACQUIRE);
        if (buffer >= PC_FRAME_BUFFER_COUNT)
            return -1;
        memcpy(pixels, shared->pixels[buffer], sizeof(shared->pixels[buffer]));
        after = __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);
    } while (before != after);
    *frame = after;
    return 0;
}

int main(int argc, char **argv)
{
    char *preferencePath;
    char defaultSavePath[PC_PATH_MAX];
    char corePath[PC_PATH_MAX];
    char savePath[PC_PATH_MAX];
    char storagePath[PC_PATH_MAX];
    char sharedPath[PC_PATH_MAX];
    char performancePath[PC_PATH_MAX];
    struct PcSharedState *shared = MAP_FAILED;
    uint32_t pixels[PC_FRAME_WIDTH * PC_FRAME_HEIGHT] = {0};
    uint32_t lastFrame = UINT32_MAX;
    uint32_t touchKeys = 0;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *textures[FRONTEND_TEXTURE_COUNT] = {0};
    struct TextureUploader textureUploader = {0};
    uint32_t textureIndex = 0;
    SDL_GameController *controller = NULL;
    SDL_AudioDeviceID audioDevice = 0;
    int sharedFd = -1;
    int running = 1;
    int suppressKeys = 0;
    int redraw = 1;
    int resumeAudio = 0;
    Uint32 performanceHoldStart = 0;
    int performanceReportShared = 0;
    uint64_t lastFrameObservedNs = 0;
    uint64_t lastLoopStartNs = 0;
    uint32_t lastWaitUs = 0;

    (void)argc;
    (void)argv;
    preferencePath = SDL_GetPrefPath("pokeemerald", "pokeemerald-pc");
    if (preferencePath == NULL
     || snprintf(defaultSavePath, sizeof(defaultSavePath), "%spokeemerald.sav", preferencePath) >= (int)sizeof(defaultSavePath)
     || snprintf(sharedPath, sizeof(sharedPath), "%score-shared", preferencePath) >= (int)sizeof(sharedPath)
     || snprintf(performancePath, sizeof(performancePath), "%sperformance-report.txt", preferencePath) >= (int)sizeof(performancePath))
        goto cleanup;
    snprintf(savePath, sizeof(savePath), "%s", defaultSavePath);
    PcProfileResolveRemembered(defaultSavePath, savePath, sizeof(savePath));
    if (GetStoragePath(savePath, storagePath, sizeof(storagePath)) != 0)
        goto cleanup;

    sharedFd = open(sharedPath, O_RDWR | O_CREAT, 0600);
    if (sharedFd < 0 || ftruncate(sharedFd, sizeof(*shared)) != 0)
        goto cleanup;
    shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE, MAP_SHARED, sharedFd, 0);
    if (shared == MAP_FAILED)
        goto cleanup;
    ResetSharedState(shared, defaultSavePath, savePath, storagePath, 0, 0);
    __atomic_store_n(&sFrontendSharedState, shared, __ATOMIC_RELEASE);
    if (__atomic_load_n(&sLifecyclePaused, __ATOMIC_ACQUIRE))
        __atomic_store_n(&shared->paused, 1, __ATOMIC_RELEASE);

    SDL_SetHint(SDL_HINT_AUDIODRIVER, "openslES");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0)
        goto cleanup;
    CallActivityMethod("prioritizeFrontendThread", NULL);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    {
        int requestedMajor;

        for (requestedMajor = 3; requestedMajor >= 2; requestedMajor--)
        {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                SDL_GL_CONTEXT_PROFILE_ES);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, requestedMajor);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            window = SDL_CreateWindow("Pokemon Emerald",
                                      SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
                                      1280,
                                      720,
                                      SDL_WINDOW_FULLSCREEN_DESKTOP
                                    | SDL_WINDOW_ALLOW_HIGHDPI
                                    | SDL_WINDOW_OPENGL);
            if (window == NULL)
                continue;
            renderer = SDL_CreateRenderer(window,
                                          -1,
                                          SDL_RENDERER_ACCELERATED
                                        | SDL_RENDERER_PRESENTVSYNC);
            if (renderer != NULL)
                break;
            SDL_DestroyWindow(window);
            window = NULL;
        }
    }
    if (renderer == NULL)
        goto cleanup;
    CallActivityMethod("configureGameSurface", NULL);
    InitTextureUploader(&textureUploader);
    {
        int texture;

        for (texture = 0; texture < FRONTEND_TEXTURE_COUNT; texture++)
        {
            textures[texture] = SDL_CreateTexture(renderer,
                                                  SDL_PIXELFORMAT_ARGB8888,
                                                  SDL_TEXTUREACCESS_STATIC,
                                                  PC_FRAME_WIDTH,
                                                  PC_FRAME_HEIGHT);
            if (textures[texture] == NULL)
                goto cleanup;
            if (UploadTextureFrame(&textureUploader,
                                   textures[texture],
                                   pixels) != 0)
                goto cleanup;
        }
        if (textureUploader.usePixelBuffers
         && glGetError() != GL_NO_ERROR)
        {
            SDL_SetError("Android PBO upload initialization failed");
            goto cleanup;
        }
    }
    if (GetActivityString("getGameCorePath", corePath, sizeof(corePath)) != 0)
        goto cleanup;

    {
        SDL_AudioSpec desired = {0};
        int joystickIndex;

        desired.freq = PC_AUDIO_RATE;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = AUDIO_CALLBACK_FRAMES;
        desired.callback = AudioCallback;
        desired.userdata = shared;
        audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
        for (joystickIndex = 0; joystickIndex < SDL_NumJoysticks(); joystickIndex++)
        {
            if (SDL_IsGameController(joystickIndex))
            {
                controller = SDL_GameControllerOpen(joystickIndex);
                break;
            }
        }
    }
    if (StartGameCore(shared, sharedPath) != 0)
        goto cleanup;
    ResumeAudioWhenBuffered(shared, audioDevice);

    while (running)
    {
        SDL_Event event;
        uint32_t frame;
        uint32_t keys;
        uint32_t eventUs;
        uint32_t loopGapUs = 0;
        uint64_t loopStartNs = GetMonotonicNs();
        uint64_t eventStartNs;
        uint64_t eventEndNs;
        uint64_t frameObservedNs;
        int newFrame = 0;
        int touchChanged = 0;
        int eventCount = 0;

        if (lastLoopStartNs != 0
         && !__atomic_load_n(&shared->paused, __ATOMIC_ACQUIRE))
        {
            loopGapUs = (uint32_t)((loopStartNs - lastLoopStartNs) / 1000);

            __atomic_store_n(&shared->performance.frontendLoopGapUs, loopGapUs,
                             __ATOMIC_RELAXED);
            StoreMaximum(&shared->performance.frontendMaxLoopGapUs, loopGapUs);
            if (loopGapUs >= FRONTEND_LOOP_EVENT_THRESHOLD_US)
                RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_FRONTEND_LOOP_GAP,
                                       loopGapUs, lastWaitUs);
        }
        lastLoopStartNs = loopStartNs;
        eventStartNs = GetMonotonicNs();
        SDL_PumpEvents();
        while (eventCount < MAX_EVENTS_PER_ITERATION
            && SDL_PeepEvents(&event,
                              1,
                              SDL_GETEVENT,
                              SDL_FIRSTEVENT,
                              SDL_LASTEVENT) > 0)
        {
            eventCount++;
            if (event.type == SDL_QUIT)
                running = 0;
            else if (event.type == SDL_APP_WILLENTERBACKGROUND)
            {
                if (audioDevice != 0)
                    SDL_PauseAudioDevice(audioDevice, 1);
                DiscardQueuedAudio(shared);
                __atomic_store_n(&sLifecyclePaused, 1, __ATOMIC_RELEASE);
                __atomic_store_n(&shared->paused, 1, __ATOMIC_RELEASE);
                touchKeys = 0;
                lastLoopStartNs = 0;
                lastWaitUs = 0;
            }
            else if (event.type == SDL_APP_DIDENTERFOREGROUND)
            {
                DiscardQueuedAudio(shared);
                __atomic_store_n(&sLifecyclePaused, 0, __ATOMIC_RELEASE);
                __atomic_store_n(&shared->paused, 0, __ATOMIC_RELEASE);
                resumeAudio = 1;
                lastFrameObservedNs = 0;
                lastLoopStartNs = 0;
                lastWaitUs = 0;
                CallActivityMethod("configureGameSurface", NULL);
                redraw = 1;
            }
            else if (event.type == SDL_WINDOWEVENT
                  && (event.window.event == SDL_WINDOWEVENT_EXPOSED
                   || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED))
                redraw = 1;
            else if (event.type == SDL_FINGERDOWN
                  || event.type == SDL_FINGERMOTION
                  || event.type == SDL_FINGERUP)
                touchChanged = 1;
        }
        if (touchChanged)
            SDL_FlushEvent(SDL_FINGERMOTION);
        eventEndNs = GetMonotonicNs();
        eventUs = (uint32_t)((eventEndNs - eventStartNs) / 1000);
        __atomic_store_n(&shared->performance.frontendEventUs, eventUs, __ATOMIC_RELAXED);
        StoreMaximum(&shared->performance.frontendMaxEventUs, eventUs);
        if (eventUs >= FRONTEND_PHASE_EVENT_THRESHOLD_US)
            RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_FRONTEND_EVENTS,
                                   eventUs, (uint32_t)eventCount);

        if (__atomic_load_n(&sLifecyclePaused, __ATOMIC_ACQUIRE))
        {
            uint32_t expectedFrame =
                __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);

            touchKeys = 0;
            __atomic_store_n(&shared->keys, 0, __ATOMIC_RELEASE);
            lastFrameObservedNs = 0;
            lastLoopStartNs = 0;
            lastWaitUs = 0;
            WaitForFrameSignal(shared, expectedFrame);
            continue;
        }

        if (resumeAudio)
        {
            ResumeAudioWhenBuffered(shared, audioDevice);
            resumeAudio = 0;
        }

        if (touchChanged)
        {
            uint64_t touchStartNs = GetMonotonicNs();
            uint32_t touchUs;

            touchKeys = ReadTouch();
            touchUs = (uint32_t)((GetMonotonicNs() - touchStartNs) / 1000);
            __atomic_store_n(&shared->performance.frontendTouchUs, touchUs,
                             __ATOMIC_RELAXED);
            StoreMaximum(&shared->performance.frontendMaxTouchUs, touchUs);
            if (touchUs >= FRONTEND_PHASE_EVENT_THRESHOLD_US)
                RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_FRONTEND_TOUCH,
                                       touchUs, 0);
        }
        keys = ReadKeyboard() | ReadController(controller) | touchKeys;
        if (suppressKeys)
        {
            if (keys == 0)
                suppressKeys = 0;
            keys = 0;
        }
        __atomic_store_n(&shared->keys, keys, __ATOMIC_RELEASE);

        frame = __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);
        if (frame != lastFrame)
        {
            uint64_t copyStartNs = GetMonotonicNs();

            if (CopyStableFrame(shared, pixels, &frame) == 0)
            {
                uint32_t copyUs;

                frameObservedNs = GetMonotonicNs();
                if (lastFrameObservedNs != 0)
                {
                    uint32_t gapUs = (uint32_t)((frameObservedNs - lastFrameObservedNs) / 1000);

                    __atomic_store_n(&shared->performance.frontendFrameGapUs, gapUs, __ATOMIC_RELAXED);
                    StoreMaximum(&shared->performance.frontendMaxFrameGapUs, gapUs);
                    if (lastFrame != UINT32_MAX && frame - lastFrame > 1)
                    {
                        uint32_t skipped = frame - lastFrame - 1;
                        uint32_t previousMax =
                            __atomic_load_n(&shared->performance.frontendMaxSkipCount,
                                            __ATOMIC_RELAXED);

                        __atomic_add_fetch(&shared->performance.frontendSkippedFrames,
                                           skipped,
                                           __ATOMIC_RELAXED);
                        __atomic_store_n(&shared->performance.frontendLastSkipFrame,
                                         frame,
                                         __ATOMIC_RELAXED);
                        __atomic_store_n(&shared->performance.frontendLastSkipCount,
                                         skipped,
                                         __ATOMIC_RELAXED);
                        __atomic_store_n(&shared->performance.frontendLastSkipGapUs,
                                         gapUs,
                                         __ATOMIC_RELAXED);
                        __atomic_store_n(&shared->performance.frontendLastSkipEventUs,
                                         eventUs,
                                         __ATOMIC_RELAXED);
                        __atomic_store_n(&shared->performance.frontendLastSkipWaitUs,
                                         lastWaitUs,
                                         __ATOMIC_RELAXED);
                        if (skipped > previousMax
                         || (skipped == previousMax
                          && gapUs > __atomic_load_n(
                                         &shared->performance.frontendMaxSkipGapUs,
                                         __ATOMIC_RELAXED)))
                        {
                            __atomic_store_n(&shared->performance.frontendMaxSkipFrame,
                                             frame,
                                             __ATOMIC_RELAXED);
                            __atomic_store_n(&shared->performance.frontendMaxSkipCount,
                                             skipped,
                                             __ATOMIC_RELAXED);
                            __atomic_store_n(&shared->performance.frontendMaxSkipGapUs,
                                             gapUs,
                                             __ATOMIC_RELAXED);
                            __atomic_store_n(&shared->performance.frontendMaxSkipEventUs,
                                             eventUs,
                                             __ATOMIC_RELAXED);
                            __atomic_store_n(&shared->performance.frontendMaxSkipWaitUs,
                                             lastWaitUs,
                                             __ATOMIC_RELAXED);
                        }
                    }
                }
                lastFrameObservedNs = frameObservedNs;
                copyUs = (uint32_t)((GetMonotonicNs() - copyStartNs) / 1000);
                __atomic_store_n(&shared->performance.frontendCopyUs, copyUs,
                                 __ATOMIC_RELAXED);
                StoreMaximum(&shared->performance.frontendMaxCopyUs, copyUs);
                if (copyUs >= FRONTEND_PHASE_EVENT_THRESHOLD_US)
                    RecordPerformanceStall(shared, PC_PERFORMANCE_STALL_FRONTEND_COPY,
                                           copyUs, 0);
                {
                    uint32_t nextTexture = (textureIndex + 1) % FRONTEND_TEXTURE_COUNT;
                    uint64_t uploadStartNs = GetMonotonicNs();
                    uint64_t uploadCpuStartNs = GetThreadCpuNs();
                    uint32_t uploadUs;
                    uint32_t uploadCpuUs;

                    if (UploadTextureFrame(&textureUploader,
                                           textures[nextTexture],
                                           pixels) != 0)
                    {
                        running = 0;
                        continue;
                    }
                    uploadUs = (uint32_t)((GetMonotonicNs() - uploadStartNs) / 1000);
                    uploadCpuUs = (uint32_t)((GetThreadCpuNs() - uploadCpuStartNs) / 1000);
                    __atomic_store_n(&shared->performance.frontendUploadUs,
                                     uploadUs,
                                     __ATOMIC_RELAXED);
                    StoreMaximum(&shared->performance.frontendMaxUploadUs, uploadUs);
                    __atomic_store_n(&shared->performance.frontendUploadCpuUs,
                                     uploadCpuUs,
                                     __ATOMIC_RELAXED);
                    StoreMaximum(&shared->performance.frontendMaxUploadCpuUs,
                                 uploadCpuUs);
                    if (uploadUs >= FRONTEND_PHASE_EVENT_THRESHOLD_US)
                        RecordPerformanceStall(shared,
                                               PC_PERFORMANCE_STALL_FRONTEND_UPLOAD,
                                               uploadUs,
                                               nextTexture);
                    textureIndex = nextTexture;
                }
                lastFrame = frame;
                newFrame = 1;
                redraw = 1;
            }
        }
        if (redraw)
        {
            int width;
            int height;
            int gameWidth;
            int gameHeight;
            SDL_Rect destination;
            uint64_t presentStartNs = GetMonotonicNs();
            uint64_t presentCpuStartNs = GetThreadCpuNs();
            uint32_t presentUs;
            uint32_t presentCpuUs;

            SDL_GetRendererOutputSize(renderer, &width, &height);
            gameHeight = height;
            gameWidth = gameHeight * PC_FRAME_WIDTH / PC_FRAME_HEIGHT;
            if (gameWidth > width)
            {
                gameWidth = width;
                gameHeight = width * PC_FRAME_HEIGHT / PC_FRAME_WIDTH;
            }
            destination.x = (width - gameWidth) / 2;
            destination.y = (height - gameHeight) / 2;
            destination.w = gameWidth;
            destination.h = gameHeight;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, textures[textureIndex], NULL, &destination);
            DrawControls(renderer, width, height, keys);
            SDL_RenderPresent(renderer);
            presentUs = (uint32_t)((GetMonotonicNs() - presentStartNs) / 1000);
            presentCpuUs = (uint32_t)((GetThreadCpuNs() - presentCpuStartNs) / 1000);
            if (newFrame)
            {
                __atomic_store_n(&shared->performance.frontendPresentUs,
                                 presentUs,
                                 __ATOMIC_RELAXED);
                StoreMaximum(&shared->performance.frontendMaxPresentUs,
                             presentUs);
                __atomic_store_n(&shared->performance.frontendPresentCpuUs,
                                 presentCpuUs,
                                 __ATOMIC_RELAXED);
                StoreMaximum(&shared->performance.frontendMaxPresentCpuUs,
                             presentCpuUs);
            }
            if (presentUs >= FRONTEND_PRESENT_EVENT_THRESHOLD_US)
                RecordPerformanceStall(shared,
                                       PC_PERFORMANCE_STALL_FRONTEND_PRESENT,
                                       presentUs,
                                       0);
            redraw = 0;
        }

        if ((keys & (L_BUTTON | R_BUTTON | SELECT_BUTTON))
         == (L_BUTTON | R_BUTTON | SELECT_BUTTON))
        {
            if (performanceHoldStart == 0)
                performanceHoldStart = SDL_GetTicks();
            else if (!performanceReportShared
                  && SDL_GetTicks() - performanceHoldStart >= PERFORMANCE_REPORT_HOLD_MS)
            {
                if (WritePerformanceReport(performancePath, shared, renderer) == 0)
                    CallActivityMethod("sharePerformanceReport", performancePath);
                performanceReportShared = 1;
            }
        }
        else
        {
            performanceHoldStart = 0;
            performanceReportShared = 0;
        }

        if (__atomic_load_n(&shared->coreExited, __ATOMIC_ACQUIRE))
        {
            int status = __atomic_load_n(&shared->coreExitStatus, __ATOMIC_RELAXED);
            int resumeMainMenu = status == PC_CORE_EXIT_PROFILE_SWITCH;

            if (status != PC_CORE_EXIT_SOFT_RESET && !resumeMainMenu)
            {
                char reportPath[PC_PATH_MAX];

                if (status != 0
                 && PcWriteCrashReport(shared,
                                       corePath,
                                       savePath,
                                       lastFrame == UINT32_MAX ? NULL : pixels,
                                       status,
                                       reportPath,
                                       sizeof(reportPath)) == 0)
                {
                    CallActivityMethod("shareCrashReport", reportPath);
                    SDL_Delay(250);
                }
                running = 0;
                continue;
            }
            if (resumeMainMenu)
            {
                if (shared->requestedSavePath[0] == '\0' || shared->requestedStoragePath[0] == '\0')
                {
                    running = 0;
                    continue;
                }
                snprintf(savePath, sizeof(savePath), "%s", shared->requestedSavePath);
                snprintf(storagePath, sizeof(storagePath), "%s", shared->requestedStoragePath);
                PcProfileRememberBySavePath(defaultSavePath, savePath);
            }
            ResetSharedState(shared, defaultSavePath, savePath, storagePath, resumeMainMenu, audioDevice);
            suppressKeys = 1;
            lastFrame = UINT32_MAX;
            lastFrameObservedNs = 0;
            lastLoopStartNs = 0;
            lastWaitUs = 0;
            performanceHoldStart = 0;
            performanceReportShared = 0;
            redraw = 1;
            if (StartGameCore(shared, sharedPath) != 0)
                running = 0;
            else
                ResumeAudioWhenBuffered(shared, audioDevice);
        }

        if (!redraw)
        {
            uint32_t expectedFrame = lastFrame == UINT32_MAX
                                   ? __atomic_load_n(&shared->frameSequence,
                                                     __ATOMIC_ACQUIRE)
                                   : lastFrame;
            uint64_t waitStartNs = GetMonotonicNs();

            WaitForFrameSignal(shared, expectedFrame);
            lastWaitUs = (uint32_t)((GetMonotonicNs() - waitStartNs) / 1000);
        }
    }

cleanup:
    if (shared != MAP_FAILED)
        __atomic_store_n(&shared->quit, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&sFrontendSharedState, NULL, __ATOMIC_RELEASE);
    while (__atomic_load_n(&sLifecycleCallbackActive, __ATOMIC_ACQUIRE) != 0)
        SDL_Delay(1);
    CallActivityMethod("stopGameCore", NULL);
    if (controller != NULL) SDL_GameControllerClose(controller);
    if (audioDevice != 0) SDL_CloseAudioDevice(audioDevice);
    {
        int texture;

        for (texture = 0; texture < FRONTEND_TEXTURE_COUNT; texture++)
        {
            if (textures[texture] != NULL)
                SDL_DestroyTexture(textures[texture]);
        }
    }
    DestroyTextureUploader(&textureUploader);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
    if (shared != MAP_FAILED) munmap(shared, sizeof(*shared));
    if (sharedFd >= 0) close(sharedFd);
    SDL_free(preferencePath);
    return 0;
}
