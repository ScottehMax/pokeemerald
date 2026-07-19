#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include "pc_sdl.h"
#include "pc_shared.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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

#ifdef _WIN32
typedef HANDLE PcProcess;
#define PC_PROCESS_INVALID NULL
#else
typedef pid_t PcProcess;
#define PC_PROCESS_INVALID ((pid_t)-1)
#endif

static void AudioCallback(void *userdata, Uint8 *stream, int len)
{
    struct PcSharedState *shared = userdata;
    int16_t *output = (int16_t *)stream;
    uint32_t requested = (uint32_t)len / (sizeof(int16_t) * 2);
    uint32_t read = __atomic_load_n(&shared->audioRead, __ATOMIC_RELAXED);
    uint32_t write = __atomic_load_n(&shared->audioWrite, __ATOMIC_ACQUIRE);
    uint32_t available = write - read;
    uint32_t count = requested < available ? requested : available;
    uint32_t i;

    memset(stream, 0, (size_t)len);
    for (i = 0; i < count; i++)
    {
        uint32_t index = (read + i) & (PC_AUDIO_BUFFER_FRAMES - 1);
        output[i * 2] = shared->audio[index * 2];
        output[i * 2 + 1] = shared->audio[index * 2 + 1];
    }
    __atomic_store_n(&shared->audioRead, read + count, __ATOMIC_RELEASE);
}

static uint32_t ReadKeyboard(void)
{
    const Uint8 *keyboard;
    int count;
    uint32_t keys = 0;

    keyboard = SDL_GetKeyboardState(&count);
#define PRESSED(scancode) ((scancode) < count && keyboard[(scancode)])
    if (PRESSED(SDL_SCANCODE_X))
        keys |= A_BUTTON;
    if (PRESSED(SDL_SCANCODE_Z))
        keys |= B_BUTTON;
    if (PRESSED(SDL_SCANCODE_BACKSPACE))
        keys |= SELECT_BUTTON;
    if (PRESSED(SDL_SCANCODE_RETURN))
        keys |= START_BUTTON;
    if (PRESSED(SDL_SCANCODE_RIGHT))
        keys |= DPAD_RIGHT;
    if (PRESSED(SDL_SCANCODE_LEFT))
        keys |= DPAD_LEFT;
    if (PRESSED(SDL_SCANCODE_UP))
        keys |= DPAD_UP;
    if (PRESSED(SDL_SCANCODE_DOWN))
        keys |= DPAD_DOWN;
    if (PRESSED(SDL_SCANCODE_S))
        keys |= R_BUTTON;
    if (PRESSED(SDL_SCANCODE_A))
        keys |= L_BUTTON;
#undef PRESSED
    return keys;
}

static int GetDefaultPaths(const char *launcherPath,
                           char *corePath,
                           size_t coreSize,
                           char *savePath,
                           size_t saveSize)
{
    char resolved[PC_PATH_MAX];
    char *separator;

#ifdef _WIN32
    DWORD length;

    (void)launcherPath;
    length = GetModuleFileNameA(NULL, resolved, sizeof(resolved));
    if (length == 0 || length >= sizeof(resolved))
        return -1;
    separator = strrchr(resolved, '\\');
    if (separator == NULL)
        separator = strrchr(resolved, '/');
#else
    if (realpath(launcherPath, resolved) == NULL)
        return -1;
    separator = strrchr(resolved, '/');
#endif
    if (separator == NULL)
        return -1;
    separator[1] = '\0';
#ifdef _WIN32
    if (snprintf(corePath, coreSize, "%spokeemerald-core.exe", resolved) >= (int)coreSize)
        return -1;
#else
    if (snprintf(corePath, coreSize, "%spokeemerald-core", resolved) >= (int)coreSize)
        return -1;
#endif
    if (snprintf(savePath, saveSize, "%spokeemerald.sav", resolved) >= (int)saveSize)
        return -1;
    return 0;
}

static PcProcess LaunchCore(const char *corePath, const char *sharedPath)
{
#ifdef _WIN32
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION process = {0};
    char command[PC_PATH_MAX + 256];

    startup.cb = sizeof(startup);
    if (snprintf(command, sizeof(command), "\"%s\" \"%s\"", corePath, sharedPath) >= (int)sizeof(command))
        return PC_PROCESS_INVALID;
    if (!CreateProcessA(corePath, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
        return PC_PROCESS_INVALID;
    CloseHandle(process.hThread);
    return process.hProcess;
#else
    pid_t child = fork();

    if (child == 0)
    {
        execl(corePath, corePath, sharedPath, (char *)NULL);
        fprintf(stderr, "could not launch %s: %s\n", corePath, strerror(errno));
        _exit(127);
    }
    return child;
#endif
}

static void ResetSharedState(struct PcSharedState *shared,
                             const char *savePath,
                             SDL_AudioDeviceID audioDevice)
{
    if (audioDevice != 0)
        SDL_PauseAudioDevice(audioDevice, 1);
    memset(shared, 0, sizeof(*shared));
    shared->magic = PC_SHARED_MAGIC;
    memcpy(shared->savePath, savePath, strlen(savePath) + 1);
    if (audioDevice != 0)
        SDL_PauseAudioDevice(audioDevice, 0);
}

static int PollCore(PcProcess *core, int *exitStatus)
{
    if (*core == PC_PROCESS_INVALID)
        return 0;

#ifdef _WIN32
    if (WaitForSingleObject(*core, 0) != WAIT_OBJECT_0)
        return 0;

    {
        DWORD status = 1;

        GetExitCodeProcess(*core, &status);
        CloseHandle(*core);
        *core = PC_PROCESS_INVALID;
        *exitStatus = (int)status;
    }
#else
    {
        int status;
        pid_t childStatus = waitpid(*core, &status, WNOHANG);

        if (childStatus != *core)
            return 0;
        *core = PC_PROCESS_INVALID;
        if (WIFSIGNALED(status))
            *exitStatus = -WTERMSIG(status);
        else if (WIFEXITED(status))
            *exitStatus = WEXITSTATUS(status);
        else
            *exitStatus = 1;
    }
#endif
    return 1;
}

static int WriteFrame(const char *path, const uint32_t *pixels)
{
    FILE *file = fopen(path, "wb");
    int x;
    int y;

    if (file == NULL)
        return -1;
    fprintf(file, "P6\n%d %d\n255\n", PC_FRAME_WIDTH, PC_FRAME_HEIGHT);
    for (y = 0; y < PC_FRAME_HEIGHT; y++)
    {
        for (x = 0; x < PC_FRAME_WIDTH; x++)
        {
            uint32_t pixel = pixels[y * PC_FRAME_WIDTH + x];
            Uint8 rgb[3] = {(Uint8)(pixel >> 16), (Uint8)(pixel >> 8), (Uint8)pixel};

            fwrite(rgb, sizeof(rgb), 1, file);
        }
    }
    return fclose(file);
}

static int CopyStableFrame(struct PcSharedState *shared, uint32_t *pixels, uint32_t *frame)
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
#ifdef _WIN32
    char sharedPath[128];
    HANDLE sharedMapping = NULL;
#else
    char sharedPath[] = "/tmp/pokeemerald-pc-XXXXXX";
    int sharedFd = -1;
#endif
    char corePath[PC_PATH_MAX];
    char savePath[PC_PATH_MAX];
    struct PcSharedState *shared = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_AudioDeviceID audioDevice = 0;
    uint32_t lastFrame = UINT32_MAX;
    PcProcess core = PC_PROCESS_INVALID;
    int result = 1;
    int running = 1;
    uint32_t frameLimit = 0;
    const char *dumpPath = NULL;
    uint32_t framePixels[PC_FRAME_WIDTH * PC_FRAME_HEIGHT];
    int printStats = 0;
    int suppressKeysUntilRelease = 0;
    uint32_t coreRestarts = 0;
    int argIndex;

    if (GetDefaultPaths(argv[0], corePath, sizeof(corePath), savePath, sizeof(savePath)) != 0)
    {
        fprintf(stderr, "could not resolve the game paths\n");
        return 1;
    }
    for (argIndex = 1; argIndex < argc; argIndex++)
    {
        if (strcmp(argv[argIndex], "--save") == 0 && argIndex + 1 < argc)
        {
            if (snprintf(savePath, sizeof(savePath), "%s", argv[++argIndex]) >= (int)sizeof(savePath))
            {
                fprintf(stderr, "save path is too long\n");
                return 2;
            }
        }
        else if (strcmp(argv[argIndex], "--frames") == 0 && argIndex + 1 < argc)
        {
            char *end;
            unsigned long value = strtoul(argv[++argIndex], &end, 10);

            if (*end != '\0' || value == 0 || value > UINT32_MAX)
            {
                fprintf(stderr, "invalid frame count\n");
                return 2;
            }
            frameLimit = (uint32_t)value;
        }
        else if (strcmp(argv[argIndex], "--dump") == 0 && argIndex + 1 < argc)
        {
            dumpPath = argv[++argIndex];
        }
        else if (strcmp(argv[argIndex], "--stats") == 0)
        {
            printStats = 1;
        }
        else
        {
            fprintf(stderr, "usage: %s [--save PATH] [--frames COUNT] [--dump PATH] [--stats]\n", argv[0]);
            return 2;
        }
    }
    if (dumpPath != NULL && frameLimit == 0)
    {
        fprintf(stderr, "--dump requires --frames\n");
        return 2;
    }

#ifdef _WIN32
    if (snprintf(sharedPath,
                 sizeof(sharedPath),
                 "Local\\pokeemerald-pc-%lu-%lu",
                 (unsigned long)GetCurrentProcessId(),
                 (unsigned long)GetTickCount()) >= (int)sizeof(sharedPath))
    {
        fprintf(stderr, "could not create shared memory name\n");
        goto cleanup;
    }
    sharedMapping = CreateFileMappingA(INVALID_HANDLE_VALUE,
                                       NULL,
                                       PAGE_READWRITE,
                                       0,
                                       (DWORD)sizeof(*shared),
                                       sharedPath);
    if (sharedMapping == NULL)
    {
        fprintf(stderr, "could not create shared memory: Windows error %lu\n", GetLastError());
        goto cleanup;
    }
    shared = MapViewOfFile(sharedMapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*shared));
    if (shared == NULL)
    {
        fprintf(stderr, "could not map shared memory: Windows error %lu\n", GetLastError());
        goto cleanup;
    }
#else
    sharedFd = mkstemp(sharedPath);
    if (sharedFd < 0 || ftruncate(sharedFd, sizeof(*shared)) != 0)
    {
        fprintf(stderr, "could not create shared memory: %s\n", strerror(errno));
        goto cleanup;
    }

    shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE, MAP_SHARED, sharedFd, 0);
    if (shared == MAP_FAILED)
    {
        fprintf(stderr, "could not map shared memory: %s\n", strerror(errno));
        goto cleanup;
    }
#endif
    ResetSharedState(shared, savePath, 0);

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        goto unmap;
    }

    window = SDL_CreateWindow("Pokemon Emerald",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              PC_FRAME_WIDTH * 3,
                              PC_FRAME_HEIGHT * 3,
                              SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        goto sdl_cleanup;
    }

    {
        SDL_AudioSpec desired;

        memset(&desired, 0, sizeof(desired));
        desired.freq = PC_AUDIO_RATE;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = 1024;
        desired.callback = AudioCallback;
        desired.userdata = shared;
        audioDevice = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
        if (audioDevice == 0)
            fprintf(stderr, "SDL audio is unavailable: %s\n", SDL_GetError());
        else
            SDL_PauseAudioDevice(audioDevice, 0);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL)
    {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        goto sdl_cleanup;
    }
    SDL_RenderSetLogicalSize(renderer, PC_FRAME_WIDTH, PC_FRAME_HEIGHT);

    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                PC_FRAME_WIDTH,
                                PC_FRAME_HEIGHT);
    if (texture == NULL)
    {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        goto sdl_cleanup;
    }

    core = LaunchCore(corePath, sharedPath);
    if (core == PC_PROCESS_INVALID)
    {
#ifdef _WIN32
        fprintf(stderr, "could not launch the game core: Windows error %lu\n", GetLastError());
#else
        fprintf(stderr, "could not fork the game core: %s\n", strerror(errno));
#endif
        goto sdl_cleanup;
    }

    result = 0;
    while (running)
    {
        SDL_Event event;
        uint32_t frame;
        const Uint8 *keyboard;
        int keyCount;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        keyboard = SDL_GetKeyboardState(&keyCount);
        if (SDL_SCANCODE_ESCAPE < keyCount && keyboard[SDL_SCANCODE_ESCAPE])
            running = 0;
        {
            uint32_t keys = ReadKeyboard();

            if (suppressKeysUntilRelease)
            {
                if (keys == 0)
                    suppressKeysUntilRelease = 0;
                keys = 0;
            }
            __atomic_store_n(&shared->keys, keys, __ATOMIC_RELEASE);
        }

        frame = __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE);
        if (frame != lastFrame && CopyStableFrame(shared, framePixels, &frame) == 0)
        {
            SDL_UpdateTexture(texture, NULL, framePixels, PC_FRAME_WIDTH * sizeof(uint32_t));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            lastFrame = frame;
            if (frameLimit != 0 && frame >= frameLimit)
            {
                if (dumpPath != NULL && WriteFrame(dumpPath, framePixels) != 0)
                {
                    fprintf(stderr, "could not write frame dump %s: %s\n", dumpPath, strerror(errno));
                    result = 1;
                }
                running = 0;
            }
        }
        else
        {
            SDL_Delay(1);
        }

        {
            int exitStatus;

            if (PollCore(&core, &exitStatus))
            {
                if (exitStatus == PC_CORE_EXIT_SOFT_RESET)
                {
                    ResetSharedState(shared, savePath, audioDevice);
                    core = LaunchCore(corePath, sharedPath);
                    if (core == PC_PROCESS_INVALID)
                    {
#ifdef _WIN32
                        fprintf(stderr, "could not restart the game core: Windows error %lu\n", GetLastError());
#else
                        fprintf(stderr, "could not restart the game core: %s\n", strerror(errno));
#endif
                        result = 1;
                        running = 0;
                    }
                    else
                    {
                        coreRestarts++;
                        suppressKeysUntilRelease = 1;
                        lastFrame = UINT32_MAX;
                    }
                }
                else
                {
                    if (exitStatus < 0)
                    {
                        fprintf(stderr, "pokeemerald-core terminated by signal %d\n", -exitStatus);
                        result = 1;
                    }
                    else if (exitStatus != 0)
                    {
                        fprintf(stderr, "pokeemerald-core exited with status %d\n", exitStatus);
                        result = 1;
                    }
                    running = 0;
                }
            }
        }
    }

    if (printStats)
    {
        fprintf(stderr,
                "frames=%u audio_frames=%u audio_peak=%u audio_nonzero=%u audio_clipped=%u "
                "battle_state=%u battle_outcome=%u core_restarts=%u\n",
                __atomic_load_n(&shared->frameSequence, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->audioFramesGenerated, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->audioPeak, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->audioSamplesNonzero, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->audioSamplesClipped, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->testBattleState, __ATOMIC_ACQUIRE),
                __atomic_load_n(&shared->testBattleOutcome, __ATOMIC_ACQUIRE),
                coreRestarts);
    }

    __atomic_store_n(&shared->quit, 1, __ATOMIC_RELEASE);
    if (core != PC_PROCESS_INVALID)
    {
        int attempts;

#ifdef _WIN32
        for (attempts = 0; attempts < 100 && WaitForSingleObject(core, 0) == WAIT_TIMEOUT; attempts++)
            SDL_Delay(5);
        if (attempts == 100)
        {
            TerminateProcess(core, 1);
            WaitForSingleObject(core, INFINITE);
        }
        CloseHandle(core);
#else
        {
            int status;

        for (attempts = 0; attempts < 100 && waitpid(core, &status, WNOHANG) == 0; attempts++)
            SDL_Delay(5);
        if (attempts == 100)
        {
            kill(core, SIGTERM);
            waitpid(core, &status, 0);
        }
        }
#endif
    }

sdl_cleanup:
    if (audioDevice != 0)
        SDL_CloseAudioDevice(audioDevice);
    if (texture != NULL)
        SDL_DestroyTexture(texture);
    if (renderer != NULL)
        SDL_DestroyRenderer(renderer);
    if (window != NULL)
        SDL_DestroyWindow(window);
    SDL_Quit();
unmap:
#ifdef _WIN32
    if (shared != NULL)
        UnmapViewOfFile(shared);
#else
    munmap(shared, sizeof(*shared));
#endif
cleanup:
#ifdef _WIN32
    if (sharedMapping != NULL)
        CloseHandle(sharedMapping);
#else
    if (sharedFd >= 0)
        close(sharedFd);
    unlink(sharedPath);
#endif
    return result;
}
