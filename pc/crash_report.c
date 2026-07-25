#define _POSIX_C_SOURCE 200809L

#include "pc_crash_report.h"
#include "pc_shared.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PC_DIAGNOSTIC_REPORTED_BREADCRUMBS 24

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#else
#include <elf.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

struct PcSymbolizer
{
#ifdef _WIN32
    HANDLE process;
    DWORD64 moduleBase;
    DWORD error;
    const char *errorStage;
    SYM_TYPE symbolType;
    int initialized;
#else
#if PLATFORM_ANDROID
    Elf64_Sym *symbols;
#else
    Elf32_Sym *symbols;
#endif
    size_t symbolCount;
    char *strings;
    size_t stringsSize;
    uint32_t moduleBase;
#endif
};

static const char *GetAccessName(uint32_t access)
{
    switch (access)
    {
    case PC_CRASH_ACCESS_READ:
        return "read";
    case PC_CRASH_ACCESS_WRITE:
        return "write";
    case PC_CRASH_ACCESS_EXECUTE:
        return "execute";
    default:
        return "unknown";
    }
}

static const char *GetDispatchName(uint32_t kind)
{
    switch (kind)
    {
    case PC_DIAGNOSTIC_DISPATCH_MAIN_1:
        return "main callback 1";
    case PC_DIAGNOSTIC_DISPATCH_MAIN_2:
        return "main callback 2";
    case PC_DIAGNOSTIC_DISPATCH_TASK:
        return "task";
    case PC_DIAGNOSTIC_DISPATCH_SPRITE:
        return "sprite";
    case PC_DIAGNOSTIC_DISPATCH_SCRIPT_NATIVE:
        return "native script";
    case PC_DIAGNOSTIC_DISPATCH_SCRIPT_COMMAND:
        return "script command";
    default:
        return "none";
    }
}

static const char *GetPhaseName(uint32_t phase)
{
    switch (phase)
    {
    case PC_DIAGNOSTIC_PHASE_CALLBACKS:
        return "callbacks";
    case PC_DIAGNOSTIC_PHASE_FRAME_HOUSEKEEPING:
        return "frame housekeeping";
    case PC_DIAGNOSTIC_PHASE_MAP_MUSIC:
        return "map music";
    case PC_DIAGNOSTIC_PHASE_WAIT_FRAME:
        return "frame wait";
    case PC_DIAGNOSTIC_PHASE_VCOUNT:
        return "VCount";
    case PC_DIAGNOSTIC_PHASE_VBLANK:
        return "VBlank";
    case PC_DIAGNOSTIC_PHASE_AUDIO:
        return "audio";
    case PC_DIAGNOSTIC_PHASE_RENDER:
        return "render";
    default:
        return "none";
    }
}

static const char *GetRenderStageName(uint32_t stage)
{
    switch (stage)
    {
    case PC_DIAGNOSTIC_RENDER_CLEAR:
        return "clear";
    case PC_DIAGNOSTIC_RENDER_OBJECT_WINDOW:
        return "object window";
    case PC_DIAGNOSTIC_RENDER_WINDOWS:
        return "windows";
    case PC_DIAGNOSTIC_RENDER_BACKGROUNDS:
        return "backgrounds";
    case PC_DIAGNOSTIC_RENDER_SPRITES:
        return "sprites";
    case PC_DIAGNOSTIC_RENDER_OUTPUT:
        return "output";
    case PC_DIAGNOSTIC_RENDER_HBLANK_DMA:
        return "HBlank DMA";
    case PC_DIAGNOSTIC_RENDER_HBLANK_CALLBACK:
        return "HBlank callback";
    default:
        return "none";
    }
}

static uint64_t HashFile(const char *path)
{
    unsigned char buffer[64 * 1024];
    uint64_t hash = UINT64_C(14695981039346656037);
    FILE *file = fopen(path, "rb");
    size_t count;

    if (file == NULL)
        return 0;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0)
    {
        size_t i;

        for (i = 0; i < count; i++)
        {
            hash ^= buffer[i];
            hash *= UINT64_C(1099511628211);
        }
    }
    fclose(file);
    return hash;
}

static int EnsureDirectory(const char *path)
{
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
#else
    if (mkdir(path, 0700) == 0 || errno == EEXIST)
        return 0;
#endif
    return -1;
}

static int BuildReportPaths(const char *savePath,
                            char *reportPath,
                            size_t reportPathSize,
                            char *imagePath,
                            size_t imagePathSize)
{
    char directory[PC_PATH_MAX];
    char stamp[32];
    const char *separator;
    struct tm local;
    time_t now = time(NULL);
    unsigned long processId;
    size_t prefixLength;

    separator = strrchr(savePath, '/');
#ifdef _WIN32
    {
        const char *backslash = strrchr(savePath, '\\');

        if (backslash != NULL && (separator == NULL || backslash > separator))
            separator = backslash;
    }
#endif
    prefixLength = separator == NULL ? 0 : (size_t)(separator - savePath + 1);
    if (prefixLength + sizeof("crash-reports") > sizeof(directory))
        return -1;
    if (prefixLength != 0)
        memcpy(directory, savePath, prefixLength);
    memcpy(directory + prefixLength, "crash-reports", sizeof("crash-reports"));
    if (EnsureDirectory(directory) != 0)
        return -1;

#ifdef _WIN32
    localtime_s(&local, &now);
    processId = GetCurrentProcessId();
#else
    localtime_r(&now, &local);
    processId = (unsigned long)getpid();
#endif
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    if (snprintf(reportPath,
                 reportPathSize,
                 "%s%cpokeemerald-crash-%s-%lu.txt",
                 directory,
#ifdef _WIN32
                 '\\',
#else
                 '/',
#endif
                 stamp,
                 processId) >= (int)reportPathSize)
        return -1;
    if (snprintf(imagePath,
                 imagePathSize,
                 "%s%cpokeemerald-crash-%s-%lu.ppm",
                 directory,
#ifdef _WIN32
                 '\\',
#else
                 '/',
#endif
                 stamp,
                 processId) >= (int)imagePathSize)
        return -1;
    return 0;
}

static int WriteFrame(const char *path,
                      const uint32_t *pixels,
                      uint32_t width,
                      uint32_t height)
{
    FILE *file = fopen(path, "wb");
    int x;
    int y;

    if (file == NULL)
        return -1;
    if (width < PC_FRAME_WIDTH
     || width > PC_FRAME_MAX_WIDTH
     || height < PC_FRAME_HEIGHT
     || height > PC_FRAME_MAX_HEIGHT)
    {
        width = PC_FRAME_WIDTH;
        height = PC_FRAME_HEIGHT;
    }
    fprintf(file, "P6\n%u %u\n255\n", width, height);
    for (y = 0; y < (int)height; y++)
    {
        for (x = 0; x < (int)width; x++)
        {
            uint32_t pixel = pixels[y * width + x];
            unsigned char rgb[3] = {
                (unsigned char)(pixel >> 16),
                (unsigned char)(pixel >> 8),
                (unsigned char)pixel,
            };

            fwrite(rgb, sizeof(rgb), 1, file);
        }
    }
    return fclose(file);
}

#ifdef _WIN32
static int GetCoreDirectory(const char *corePath, char *directory, size_t directorySize)
{
    char *separator;
    char *forwardSlash;
    int length = snprintf(directory, directorySize, "%s", corePath);

    if (length < 0 || (size_t)length >= directorySize)
        return -1;
    separator = strrchr(directory, '\\');
    forwardSlash = strrchr(directory, '/');
    if (forwardSlash != NULL && (separator == NULL || forwardSlash > separator))
        separator = forwardSlash;
    if (separator == NULL)
        return snprintf(directory, directorySize, ".") < (int)directorySize ? 0 : -1;
    if (separator == directory)
        separator[1] = '\0';
    else
        *separator = '\0';
    return 0;
}

static int InitSymbolizer(struct PcSymbolizer *symbolizer,
                          const char *corePath,
                          uint32_t moduleBase)
{
    IMAGEHLP_MODULE64 module = {0};
    char searchPath[PC_PATH_MAX];

    memset(symbolizer, 0, sizeof(*symbolizer));
    (void)moduleBase;
    symbolizer->process = GetCurrentProcess();
    if (GetCoreDirectory(corePath, searchPath, sizeof(searchPath)) != 0)
    {
        symbolizer->errorStage = "core directory";
        symbolizer->error = ERROR_INSUFFICIENT_BUFFER;
        return -1;
    }
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    if (!SymInitialize(symbolizer->process, searchPath, FALSE))
    {
        symbolizer->errorStage = "SymInitialize";
        symbolizer->error = GetLastError();
        return -1;
    }
    symbolizer->initialized = 1;
    symbolizer->moduleBase = SymLoadModuleEx(symbolizer->process,
                                             NULL,
                                             corePath,
                                             NULL,
                                             UINT64_C(0x10000000),
                                             0,
                                             NULL,
                                             0);
    if (symbolizer->moduleBase == 0)
    {
        symbolizer->errorStage = "SymLoadModuleEx";
        symbolizer->error = GetLastError();
        return -1;
    }
    module.SizeOfStruct = sizeof(module);
    if (!SymGetModuleInfo64(symbolizer->process, symbolizer->moduleBase, &module))
    {
        symbolizer->errorStage = "SymGetModuleInfo64";
        symbolizer->error = GetLastError();
        return -1;
    }
    symbolizer->symbolType = module.SymType;
    return 0;
}

static void CloseSymbolizer(struct PcSymbolizer *symbolizer)
{
    if (symbolizer->initialized)
        SymCleanup(symbolizer->process);
}

static void FormatSymbol(struct PcSymbolizer *symbolizer,
                         uint64_t address,
                         char *buffer,
                         size_t bufferSize)
{
    unsigned char rawSymbol[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)rawSymbol;
    IMAGEHLP_LINE64 line;
    DWORD64 displacement = 0;
    DWORD lineDisplacement = 0;

    memset(rawSymbol, 0, sizeof(rawSymbol));
    symbol->SizeOfStruct = sizeof(*symbol);
    symbol->MaxNameLen = MAX_SYM_NAME;
    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(line);
    if (symbolizer->moduleBase != 0
     && SymFromAddr(symbolizer->process, address, &displacement, symbol))
    {
        if (SymGetLineFromAddr64(symbolizer->process, address, &lineDisplacement, &line))
            snprintf(buffer, bufferSize, "%s+0x%llx (%s:%lu)",
                     symbol->Name,
                     (unsigned long long)displacement,
                     line.FileName,
                     (unsigned long)line.LineNumber);
        else
            snprintf(buffer, bufferSize, "%s+0x%llx",
                     symbol->Name,
                     (unsigned long long)displacement);
    }
    else
    {
        snprintf(buffer, bufferSize, "unknown");
    }
}

static void PrintSymbolizerStatus(FILE *file, const struct PcSymbolizer *symbolizer)
{
    if (symbolizer->errorStage != NULL)
        fprintf(file, "symbols: unavailable (%s, Windows error %lu)\n",
                symbolizer->errorStage,
                (unsigned long)symbolizer->error);
    else
        fprintf(file, "symbols: loaded (DbgHelp type %u)\n", (unsigned)symbolizer->symbolType);
}
#else
static int ReadAt(FILE *file, long offset, void *data, size_t size)
{
    return fseek(file, offset, SEEK_SET) == 0 && fread(data, 1, size, file) == size ? 0 : -1;
}

static int InitSymbolizer(struct PcSymbolizer *symbolizer,
                          const char *corePath,
                          uint32_t moduleBase)
{
#if PLATFORM_ANDROID
    Elf64_Ehdr header;
    Elf64_Shdr *sections = NULL;
#else
    Elf32_Ehdr header;
    Elf32_Shdr *sections = NULL;
#endif
    FILE *file = NULL;
    size_t i;
    int result = -1;

    memset(symbolizer, 0, sizeof(*symbolizer));
    symbolizer->moduleBase = moduleBase;
    file = fopen(corePath, "rb");
    if (file == NULL || fread(&header, 1, sizeof(header), file) != sizeof(header))
        goto cleanup;
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
#if PLATFORM_ANDROID
     || header.e_ident[EI_CLASS] != ELFCLASS64
     || header.e_shentsize != sizeof(Elf64_Shdr)
#else
     || header.e_ident[EI_CLASS] != ELFCLASS32
     || header.e_shentsize != sizeof(Elf32_Shdr)
#endif
     || header.e_shnum == 0)
        goto cleanup;
    sections = malloc((size_t)header.e_shnum * sizeof(*sections));
    if (sections == NULL
     || ReadAt(file, header.e_shoff, sections, (size_t)header.e_shnum * sizeof(*sections)) != 0)
        goto cleanup;
    for (i = 0; i < header.e_shnum; i++)
    {
#if PLATFORM_ANDROID
        Elf64_Shdr *symbols = &sections[i];
        Elf64_Shdr *strings;
#else
        Elf32_Shdr *symbols = &sections[i];
        Elf32_Shdr *strings;
#endif

        if (symbols->sh_type != SHT_SYMTAB
#if PLATFORM_ANDROID
         || symbols->sh_entsize != sizeof(Elf64_Sym)
#else
         || symbols->sh_entsize != sizeof(Elf32_Sym)
#endif
         || symbols->sh_link >= header.e_shnum)
            continue;
        strings = &sections[symbols->sh_link];
        symbolizer->symbolCount = symbols->sh_size / sizeof(*symbolizer->symbols);
        symbolizer->symbols = malloc(symbols->sh_size);
        symbolizer->strings = malloc(strings->sh_size);
        symbolizer->stringsSize = strings->sh_size;
        if (symbolizer->symbols == NULL || symbolizer->strings == NULL
         || ReadAt(file, symbols->sh_offset, symbolizer->symbols, symbols->sh_size) != 0
         || ReadAt(file, strings->sh_offset, symbolizer->strings, strings->sh_size) != 0)
            goto cleanup;
        result = 0;
        break;
    }

cleanup:
    free(sections);
    if (file != NULL)
        fclose(file);
    if (result != 0)
    {
        free(symbolizer->symbols);
        free(symbolizer->strings);
        memset(symbolizer, 0, sizeof(*symbolizer));
    }
    return result;
}

static void CloseSymbolizer(struct PcSymbolizer *symbolizer)
{
    free(symbolizer->symbols);
    free(symbolizer->strings);
}

static void FormatSymbol(struct PcSymbolizer *symbolizer,
                         uint64_t address,
                         char *buffer,
                         size_t bufferSize)
{
#if PLATFORM_ANDROID
    const Elf64_Sym *best = NULL;
    uint64_t lookupAddress = symbolizer->moduleBase != 0 && address >= symbolizer->moduleBase
                           ? address - symbolizer->moduleBase
                           : address;
#else
    const Elf32_Sym *best = NULL;
    uint32_t lookupAddress = address;
#endif
    size_t i;

    for (i = 0; i < symbolizer->symbolCount; i++)
    {
#if PLATFORM_ANDROID
        const Elf64_Sym *symbol = &symbolizer->symbols[i];
        uint64_t end;
#else
        const Elf32_Sym *symbol = &symbolizer->symbols[i];
        uint32_t end;
#endif

        if (
#if PLATFORM_ANDROID
            ELF64_ST_TYPE(symbol->st_info)
#else
            ELF32_ST_TYPE(symbol->st_info)
#endif
            != STT_FUNC
         || symbol->st_name >= symbolizer->stringsSize
         || symbol->st_value > lookupAddress)
            continue;
        end = symbol->st_size == 0 ? symbol->st_value : symbol->st_value + symbol->st_size;
        if (((symbol->st_size == 0 && lookupAddress - symbol->st_value < 4096) || lookupAddress < end)
         && (best == NULL || symbol->st_value > best->st_value))
            best = symbol;
    }
    if (best != NULL)
        snprintf(buffer,
                 bufferSize,
                 "%s+0x%" PRIx64,
                 symbolizer->strings + best->st_name,
                 (uint64_t)(lookupAddress - best->st_value));
    else
        snprintf(buffer, bufferSize, "unknown");
}
#endif

static void PrintAddress(FILE *file,
                         struct PcSymbolizer *symbolizer,
                         const char *label,
                         uint32_t address)
{
    char symbol[1024];

    if (address == 0)
    {
        fprintf(file, "%s: 0x00000000\n", label);
        return;
    }
    FormatSymbol(symbolizer, address, symbol, sizeof(symbol));
    fprintf(file, "%s: 0x%08" PRIx32 " %s\n", label, address, symbol);
}

static void PrintNativeAddress(FILE *file,
                               struct PcSymbolizer *symbolizer,
                               const char *label,
                               uint64_t address)
{
    char symbol[1024];

    if (address == 0)
    {
        fprintf(file, "%s: 0x0000000000000000\n", label);
        return;
    }
    FormatSymbol(symbolizer, address, symbol, sizeof(symbol));
    fprintf(file, "%s: 0x%016" PRIx64 " %s\n", label, address, symbol);
}

static void PrintData(FILE *file, const int16_t *data, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
        fprintf(file, "%s%d", i == 0 ? "" : ", ", data[i]);
    fputc('\n', file);
}

int PcWriteCrashReport(const struct PcSharedState *shared,
                       const char *corePath,
                       const char *savePath,
                       const uint32_t *pixels,
                       int exitStatus,
                       char *reportPath,
                       size_t reportPathSize)
{
    const struct PcCrashRecord *crash = &shared->crash;
    const struct PcDiagnosticState *state = &shared->diagnostics;
    struct PcSymbolizer symbolizer;
    char imagePath[PC_PATH_MAX];
    char topSymbol[1024] = "unknown";
    uint64_t coreHash;
    FILE *file;
    uint32_t i;
    uint32_t breadcrumbAvailable;
    uint32_t breadcrumbCount;
    uint32_t breadcrumbStart;

    if (BuildReportPaths(savePath,
                         reportPath,
                         reportPathSize,
                         imagePath,
                         sizeof(imagePath)) != 0)
        return -1;
    file = fopen(reportPath, "wb");
    if (file == NULL)
        return -1;
    InitSymbolizer(&symbolizer, corePath, shared->coreLoadBase);
    coreHash = HashFile(corePath);

    fprintf(file, "pokeemerald-pc crash report\n");
    fprintf(file, "format: %u\n", PC_CRASH_VERSION);
    fprintf(file, "core: %s\n", corePath);
    fprintf(file, "core_fnv1a64: %016" PRIx64 "\n", coreHash);
#if PLATFORM_ANDROID
    fprintf(file, "core_base: 0x%08" PRIx32 "\n", shared->coreLoadBase);
#endif
#ifdef _WIN32
    PrintSymbolizerStatus(file, &symbolizer);
#endif
    fprintf(file, "exit_status: %d\n", exitStatus);
    if (shared->coreErrorMessage[0] != '\0')
        fprintf(file, "core_error: %s\n", shared->coreErrorMessage);
    fprintf(file, "frame: %" PRIu32 "\n", state->frame);
    fprintf(file, "frame_size: %" PRIu32 "x%" PRIu32 "\n",
            shared->frameWidth,
            shared->frameHeight);
    if (crash->magic == PC_CRASH_MAGIC
     && crash->version == PC_CRASH_VERSION
     && __atomic_load_n(&crash->complete, __ATOMIC_ACQUIRE) != 0)
    {
        fprintf(file, "crash_code: 0x%08" PRIx32 "\n", crash->code);
        fprintf(file, "access: %s\n", GetAccessName(crash->access));
        fprintf(file, "fault_address: 0x%016" PRIx64 "\n", crash->faultAddress);
        PrintNativeAddress(file, &symbolizer, "instruction", crash->instruction);
        if (crash->nativeModule[0] != '\0' && crash->instruction >= crash->nativeModuleBase)
            fprintf(file, "native_module: %s+0x%" PRIx64 "\n",
                    crash->nativeModule,
                    crash->instruction - crash->nativeModuleBase);
    }
    else
    {
        fprintf(file, "crash_record: unavailable\n");
    }
    fprintf(file, "last_frame: %s\n\n", pixels == NULL ? "unavailable" : imagePath);

    fprintf(file, "Stack\n");
    for (i = 0; i < crash->stackFrameCount && i < PC_DIAGNOSTIC_STACK_FRAMES; i++)
    {
        char label[32];
        char symbol[1024];
        uint64_t lookupAddress = i == 0 || crash->stackFrames[i] == 0
                               ? crash->stackFrames[i]
                               : crash->stackFrames[i] - 1;

        snprintf(label, sizeof(label), "#%02" PRIu32, i);
        FormatSymbol(&symbolizer, lookupAddress, symbol, sizeof(symbol));
        fprintf(file,
                "%s: 0x%016" PRIx64 " %s\n",
                label,
                crash->stackFrames[i],
                symbol);
    }

    fprintf(file, "\nRegisters\n");
    fprintf(file, "eax=%016" PRIx64 " ebx=%016" PRIx64 " ecx=%016" PRIx64 " edx=%016" PRIx64 "\n",
            crash->eax, crash->ebx, crash->ecx, crash->edx);
    fprintf(file, "esi=%016" PRIx64 " edi=%016" PRIx64 " ebp=%016" PRIx64 " esp=%016" PRIx64 "\n",
            crash->esi, crash->edi, crash->ebp, crash->esp);
    fprintf(file, "link=%016" PRIx64 "\n", crash->link);
    fprintf(file, "eflags=%016" PRIx64 "\n", crash->eflags);

    fprintf(file, "\nGame State\n");
    fprintf(file, "threads: crash=%" PRIu32 " game=%" PRIu32 "%s\n",
            crash->threadId,
            state->gameThreadId,
            crash->threadId != 0 && state->gameThreadId != 0 && crash->threadId != state->gameThreadId
                ? " (different thread)"
                : "");
    fprintf(file, "map=%d.%d position=%d,%d in_battle=%" PRIu32 "\n",
            state->mapGroup, state->mapNum, state->mapX, state->mapY, state->inBattle);
    fprintf(file, "battle_type=0x%08" PRIx32 " controller_flags=0x%08" PRIx32
                  " active_battler=%" PRIu32 " battlers=%" PRIu32 " outcome=%" PRIu32 "\n",
            state->battleTypeFlags,
            state->battleControllerFlags,
            state->activeBattler,
            state->battlersCount,
            state->battleOutcome);
    PrintAddress(file, &symbolizer, "battle_main", state->battleMainFunc);
    fprintf(file, "link_type=0x%04" PRIx32 " players_received=%" PRIu32 " wireless=%" PRIu32 "\n",
            state->linkType, state->linkPlayersReceived, state->wirelessCommType);

    fprintf(file, "\nDispatch State\n");
    fprintf(file, "frame_phase: %s\n", GetPhaseName(state->phase));
    if (state->phase == PC_DIAGNOSTIC_PHASE_RENDER)
    {
        if (state->renderScanline == UINT32_MAX)
            fprintf(file, "render_progress: scanline=none stage=%s\n",
                    GetRenderStageName(state->renderStage));
        else
            fprintf(file, "render_progress: scanline=%" PRIu32 " stage=%s\n",
                    state->renderScanline,
                    GetRenderStageName(state->renderStage));
    }
    PrintAddress(file, &symbolizer, "main_callback_1", state->mainCallback1);
    PrintAddress(file, &symbolizer, "main_callback_2", state->mainCallback2);
    fprintf(file, "current_main_kind: %s\n", GetDispatchName(state->currentMainKind));
    PrintAddress(file, &symbolizer, "current_main", state->currentMainCallback);
    if (state->currentTaskId == UINT32_MAX)
        fprintf(file, "current_task_id: none\n");
    else
        fprintf(file, "current_task_id: %" PRIu32 "\n", state->currentTaskId);
    PrintAddress(file, &symbolizer, "current_task", state->currentTaskFunc);
    fprintf(file, "current_task_data: ");
    PrintData(file, state->currentTaskData, PC_DIAGNOSTIC_TASK_DATA);
    if (state->currentSpriteId == UINT32_MAX)
        fprintf(file, "current_sprite_id: none\n");
    else
        fprintf(file, "current_sprite_id: %" PRIu32 "\n", state->currentSpriteId);
    PrintAddress(file, &symbolizer, "current_sprite", state->currentSpriteCallback);
    fprintf(file, "current_sprite_data: ");
    PrintData(file, state->currentSpriteData, PC_DIAGNOSTIC_SPRITE_DATA);
    fprintf(file, "current_script_ptr: 0x%08" PRIx32 " command=%" PRIu32 "\n",
            state->currentScriptPtr, state->currentScriptCommand);
    PrintAddress(file, &symbolizer, "current_script_func", state->currentScriptFunc);
    if (state->invalidAddress != 0 || state->invalidKind != PC_DIAGNOSTIC_DISPATCH_NONE)
    {
        fprintf(file, "invalid_callback_kind: %s\n", GetDispatchName(state->invalidKind));
        fprintf(file, "invalid_callback_id: %" PRIu32 "\n", state->invalidId);
        PrintAddress(file, &symbolizer, "invalid_callback", state->invalidAddress);
    }

    breadcrumbAvailable = state->breadcrumbWrite < PC_DIAGNOSTIC_BREADCRUMBS
                        ? state->breadcrumbWrite
                        : PC_DIAGNOSTIC_BREADCRUMBS;
    breadcrumbCount = breadcrumbAvailable < PC_DIAGNOSTIC_REPORTED_BREADCRUMBS
                    ? breadcrumbAvailable
                    : PC_DIAGNOSTIC_REPORTED_BREADCRUMBS;
    fprintf(file, "\nRecent Dispatches (%" PRIu32 " of %" PRIu32 ")\n",
            breadcrumbCount,
            breadcrumbAvailable);
    breadcrumbStart = state->breadcrumbWrite - breadcrumbCount;
    for (i = 0; i < breadcrumbCount;)
    {
        const struct PcDiagnosticBreadcrumb *breadcrumb =
            &state->breadcrumbs[(breadcrumbStart + i) % PC_DIAGNOSTIC_BREADCRUMBS];
        char symbol[1024];
        uint32_t runLength = 1;

        if (breadcrumb->kind == PC_DIAGNOSTIC_DISPATCH_SPRITE)
        {
            while (i + runLength < breadcrumbCount)
            {
                const struct PcDiagnosticBreadcrumb *next =
                    &state->breadcrumbs[(breadcrumbStart + i + runLength) % PC_DIAGNOSTIC_BREADCRUMBS];

                if (next->frame != breadcrumb->frame
                 || next->kind != breadcrumb->kind
                 || next->address != breadcrumb->address
                 || next->id != breadcrumb->id + runLength)
                    break;
                runLength++;
            }
        }
        else
        {
            while (i + runLength < breadcrumbCount)
            {
                const struct PcDiagnosticBreadcrumb *next =
                    &state->breadcrumbs[(breadcrumbStart + i + runLength) % PC_DIAGNOSTIC_BREADCRUMBS];

                if (next->frame != breadcrumb->frame + runLength
                 || next->kind != breadcrumb->kind
                 || next->id != breadcrumb->id
                 || next->address != breadcrumb->address)
                    break;
                runLength++;
            }
        }

        FormatSymbol(&symbolizer, breadcrumb->address, symbol, sizeof(symbol));
        if (runLength == 1)
        {
            fprintf(file,
                    "frame=%" PRIu32 " kind=%s id=%" PRIu32 " address=0x%08" PRIx32 " %s\n",
                    breadcrumb->frame,
                    GetDispatchName(breadcrumb->kind),
                    breadcrumb->id,
                    breadcrumb->address,
                    symbol);
        }
        else if (breadcrumb->kind == PC_DIAGNOSTIC_DISPATCH_SPRITE)
        {
            fprintf(file,
                    "frame=%" PRIu32 " kind=%s ids=%" PRIu32 "-%" PRIu32
                    " count=%" PRIu32 " address=0x%08" PRIx32 " %s\n",
                    breadcrumb->frame,
                    GetDispatchName(breadcrumb->kind),
                    breadcrumb->id,
                    breadcrumb->id + runLength - 1,
                    runLength,
                    breadcrumb->address,
                    symbol);
        }
        else
        {
            fprintf(file,
                    "frames=%" PRIu32 "-%" PRIu32 " kind=%s id=%" PRIu32
                    " count=%" PRIu32 " address=0x%08" PRIx32 " %s\n",
                    breadcrumb->frame,
                    breadcrumb->frame + runLength - 1,
                    GetDispatchName(breadcrumb->kind),
                    breadcrumb->id,
                    runLength,
                    breadcrumb->address,
                    symbol);
        }
        i += runLength;
    }

    if (crash->magic == PC_CRASH_MAGIC
     && crash->version == PC_CRASH_VERSION
     && crash->complete != 0)
        FormatSymbol(&symbolizer, crash->instruction, topSymbol, sizeof(topSymbol));
    CloseSymbolizer(&symbolizer);
    if (fclose(file) != 0)
        return -1;
    if (pixels != NULL
     && WriteFrame(imagePath,
                   pixels,
                   shared->frameWidth,
                   shared->frameHeight) != 0)
        fprintf(stderr, "could not write crash frame %s: %s\n", imagePath, strerror(errno));
    if (crash->magic == PC_CRASH_MAGIC
     && crash->version == PC_CRASH_VERSION
     && crash->complete != 0)
        fprintf(stderr,
                "crash location: 0x%016" PRIx64 " %s\n",
                crash->instruction,
                topSymbol);
    return 0;
}
