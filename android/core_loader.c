#define _GNU_SOURCE

#include <android/dlext.h>
#include <android/log.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pc_shared.h"

#define LOG_TAG "pokeemerald-core"
#define CORE_STACK_SIZE (2u * 1024u * 1024u)
#define LOW_ADDRESS_START UINT32_C(0x20000000)
#define LOW_ADDRESS_LIMIT (UINT64_C(1) << 32)
#define LOW_ADDRESS_STEP (16u * 1024u * 1024u)
#define ANDROID_PAGE_SIZE 16384u

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

typedef int (*CoreMain)(const char *sharedPath);

#if UINTPTR_MAX > UINT32_MAX
typedef Elf64_Ehdr CoreElfHeader;
typedef Elf64_Phdr CoreProgramHeader;
#define CORE_ELF_CLASS ELFCLASS64
#else
typedef Elf32_Ehdr CoreElfHeader;
typedef Elf32_Phdr CoreProgramHeader;
#define CORE_ELF_CLASS ELFCLASS32
#endif

struct CoreThreadArgs
{
    CoreMain main;
    const char *sharedPath;
    int result;
};

static void *RunCoreThread(void *rawArgs)
{
    struct CoreThreadArgs *args = rawArgs;

    args->result = args->main(args->sharedPath);
    return NULL;
}

static struct PcSharedState *MapSharedState(const char *sharedPath)
{
    int fd = open(sharedPath, O_RDWR);
    struct stat info;
    struct PcSharedState *shared;

    if (fd < 0 || fstat(fd, &info) != 0 || info.st_size < (off_t)sizeof(*shared))
    {
        if (fd >= 0)
            close(fd);
        return MAP_FAILED;
    }
    shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    return shared;
}

static void ReportExit(const char *sharedPath, int status, const char *message)
{
    struct PcSharedState *shared = MapSharedState(sharedPath);

    if (shared == MAP_FAILED)
        return;
    if (!__atomic_load_n(&shared->coreExited, __ATOMIC_ACQUIRE))
    {
        if (message != NULL)
            snprintf(shared->coreErrorMessage, sizeof(shared->coreErrorMessage), "%s", message);
        __atomic_store_n(&shared->coreExitStatus, status, __ATOMIC_RELAXED);
        __atomic_store_n(&shared->coreExited, 1, __ATOMIC_RELEASE);
    }
    munmap(shared, sizeof(*shared));
}

static void RecordCoreLoadBase(const char *sharedPath, uintptr_t base)
{
    struct PcSharedState *shared = MapSharedState(sharedPath);

    if (shared == MAP_FAILED)
        return;
    __atomic_store_n(&shared->coreLoadBase, (uint32_t)base, __ATOMIC_RELEASE);
    munmap(shared, sizeof(*shared));
}

static void *ReserveExact(void *address, size_t size, int protection)
{
    void *mapping = mmap(address,
                         size,
                         protection,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1,
                         0);

    if (mapping == MAP_FAILED && errno == EINVAL)
    {
        mapping = mmap(address, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping != MAP_FAILED && mapping != address)
        {
            munmap(mapping, size);
            errno = EEXIST;
            mapping = MAP_FAILED;
        }
    }
    return mapping;
}

static void *ReserveLow(size_t size, int protection)
{
    uint64_t address;

    if (size == 0 || size > LOW_ADDRESS_LIMIT - LOW_ADDRESS_START)
    {
        errno = EINVAL;
        return MAP_FAILED;
    }
    for (address = LOW_ADDRESS_START;
         (uint64_t)address + size <= LOW_ADDRESS_LIMIT;
         address += LOW_ADDRESS_STEP)
    {
        void *requested = (void *)(uintptr_t)address;
        void *mapping = ReserveExact(requested, size, protection);

        if (mapping == requested)
            return mapping;
    }
    errno = ENOMEM;
    return MAP_FAILED;
}

static size_t GetCoreLoadSize(const char *corePath)
{
    CoreElfHeader header;
    CoreProgramHeader program;
    uint64_t high = 0;
    int fd = open(corePath, O_RDONLY);
    uint16_t i;

    if (fd < 0
     || pread(fd, &header, sizeof(header), 0) != sizeof(header)
     || memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
     || header.e_ident[EI_CLASS] != CORE_ELF_CLASS
     || header.e_phentsize != sizeof(program))
    {
        if (fd >= 0)
            close(fd);
        return 0;
    }
    for (i = 0; i < header.e_phnum; i++)
    {
        uint64_t end;

        if (pread(fd,
                  &program,
                  sizeof(program),
                  (off_t)header.e_phoff + (off_t)i * sizeof(program)) != sizeof(program))
        {
            close(fd);
            return 0;
        }
        if (program.p_type != PT_LOAD || program.p_vaddr > UINT64_MAX - program.p_memsz)
            continue;
        end = program.p_vaddr + program.p_memsz;
        if (end > high)
            high = end;
    }
    close(fd);
    if (high == 0 || high > SIZE_MAX - (ANDROID_PAGE_SIZE - 1))
        return 0;
    return (size_t)((high + ANDROID_PAGE_SIZE - 1) & ~(uint64_t)(ANDROID_PAGE_SIZE - 1));
}

JNIEXPORT jint JNICALL
Java_org_pokeemerald_pc_CoreService_nativeRunCore(JNIEnv *env,
                                                   jclass classObject,
                                                   jstring rawCorePath,
                                                   jstring rawSharedPath)
{
    const char *corePath;
    const char *sharedPath;
    android_dlextinfo loaderInfo;
    pthread_attr_t threadAttributes;
    struct CoreThreadArgs args;
    Dl_info libraryInfo;
    pthread_t thread;
    void *reservation;
    void *stack;
    void *library;
    uintptr_t loadedBase;
    size_t reservationSize;
    int error;
    char errorMessage[PC_CORE_ERROR_MAX] = "";

    (void)classObject;
    corePath = (*env)->GetStringUTFChars(env, rawCorePath, NULL);
    if (corePath == NULL)
        return 1;
    sharedPath = (*env)->GetStringUTFChars(env, rawSharedPath, NULL);
    if (sharedPath == NULL)
    {
        (*env)->ReleaseStringUTFChars(env, rawCorePath, corePath);
        return 1;
    }
    args.result = 1;

    reservationSize = GetCoreLoadSize(corePath);
    if (reservationSize == 0)
    {
        snprintf(errorMessage, sizeof(errorMessage), "could not determine game core load size");
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", errorMessage);
        goto release_strings;
    }
    reservation = ReserveLow(reservationSize, PROT_NONE);
    if (reservation == MAP_FAILED)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "could not reserve %zu bytes of low core address space: %s",
                 reservationSize,
                 strerror(errno));
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        goto release_strings;
    }
    memset(&loaderInfo, 0, sizeof(loaderInfo));
    loaderInfo.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
    loaderInfo.reserved_addr = reservation;
    loaderInfo.reserved_size = reservationSize;
    library = android_dlopen_ext(corePath, RTLD_NOW | RTLD_LOCAL, &loaderInfo);
    if (library == NULL)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "could not load low-address game core: %s", dlerror());
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        args.result = 1;
        goto release_reservation;
    }

    args.main = (CoreMain)dlsym(library, "PcCoreMain");
    args.sharedPath = sharedPath;
    if (args.main == NULL
     || dladdr((void *)args.main, &libraryInfo) == 0
     || libraryInfo.dli_fbase == NULL)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "game core entry point or load address is unavailable");
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        goto close_library;
    }
    loadedBase = (uintptr_t)libraryInfo.dli_fbase;
    if (loadedBase > UINT32_MAX
     || reservationSize > LOW_ADDRESS_LIMIT - loadedBase
     || (uint64_t)loadedBase + reservationSize > LOW_ADDRESS_LIMIT
     || (uintptr_t)args.main > UINT32_MAX)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "game core was not loaded entirely below 4 GiB");
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        goto close_library;
    }
    RecordCoreLoadBase(sharedPath, loadedBase);

    stack = ReserveLow(CORE_STACK_SIZE, PROT_READ | PROT_WRITE);
    if (stack == MAP_FAILED)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "could not reserve low game stack: %s", strerror(errno));
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        goto close_library;
    }

    pthread_attr_init(&threadAttributes);
    error = pthread_attr_setstack(&threadAttributes, stack, CORE_STACK_SIZE);
    if (error == 0)
        error = pthread_create(&thread, &threadAttributes, RunCoreThread, &args);
    pthread_attr_destroy(&threadAttributes);
    if (error != 0)
    {
        snprintf(errorMessage, sizeof(errorMessage),
                 "could not start game core thread: %s", strerror(error));
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG,
                            "%s", errorMessage);
        munmap(stack, CORE_STACK_SIZE);
        goto close_library;
    }
    pthread_join(thread, NULL);
    munmap(stack, CORE_STACK_SIZE);

close_library:
    dlclose(library);
release_reservation:
    munmap(reservation, reservationSize);
release_strings:
    if (args.result != 0 && errorMessage[0] == '\0')
        snprintf(errorMessage, sizeof(errorMessage), "game core exited with status %d", args.result);
    ReportExit(sharedPath, args.result, errorMessage[0] == '\0' ? NULL : errorMessage);
    (*env)->ReleaseStringUTFChars(env, rawSharedPath, sharedPath);
    (*env)->ReleaseStringUTFChars(env, rawCorePath, corePath);
    return args.result;
}
