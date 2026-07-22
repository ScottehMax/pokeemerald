#define _POSIX_C_SOURCE 200809L

#include "pc_profiles.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LAST_PROFILE_FILE ".last-profile"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

struct PcProfileList
{
    struct PcProfileInfo *items;
    size_t count;
    size_t capacity;
    char profilesDirectory[PC_PATH_MAX];
    char defaultSavePath[PC_PATH_MAX];
};

static struct PcProfileList sProfiles;

static int CaseCompare(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0')
    {
        int difference = toupper((unsigned char)*a) - toupper((unsigned char)*b);

        if (difference != 0)
            return difference;
        a++;
        b++;
    }
    return toupper((unsigned char)*a) - toupper((unsigned char)*b);
}

static int IsProfileName(const char *name)
{
    size_t i;
    size_t length = strlen(name);

    if (length == 0 || length > PC_PROFILE_NAME_MAX || name[0] == '.'
     || name[length - 1] == ' ' || name[length - 1] == '.')
        return 0;
    for (i = 0; i < length; i++)
    {
        unsigned char character = (unsigned char)name[i];

        if (!isalnum(character) && character != ' ' && character != '-' && character != '_')
            return 0;
    }
    return CaseCompare(name, "Default") != 0;
}

static int BuildPath(char *path, size_t pathSize, const char *directory, const char *name)
{
    int length = snprintf(path, pathSize, "%s/%s", directory, name);

    return length >= 0 && (size_t)length < pathSize ? 0 : -1;
}

static int GetProfilesDirectory(const char *defaultSavePath, char *path, size_t pathSize)
{
    const char *separator = strrchr(defaultSavePath, '/');
    size_t prefixLength;

#ifdef _WIN32
    {
        const char *backslash = strrchr(defaultSavePath, '\\');

        if (backslash != NULL && (separator == NULL || backslash > separator))
            separator = backslash;
    }
#endif
    prefixLength = separator == NULL ? 0 : (size_t)(separator - defaultSavePath + 1);
    if (prefixLength + sizeof("profiles") > pathSize)
        return -1;
    if (prefixLength != 0)
        memcpy(path, defaultSavePath, prefixLength);
    memcpy(path + prefixLength, "profiles", sizeof("profiles"));
    return 0;
}

static int DirectoryExists(const char *path)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);

    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;

    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

static int SaveExists(const char *path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA info;

    return GetFileAttributesExA(path, GetFileExInfoStandard, &info)
        && !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        && info.nFileSizeHigh == 0
        && info.nFileSizeLow == 128 * 1024;
#else
    struct stat info;

    return stat(path, &info) == 0 && S_ISREG(info.st_mode) && info.st_size == 128 * 1024;
#endif
}

static int EnsureDirectory(const char *path)
{
#ifdef _WIN32
    if (CreateDirectoryA(path, NULL))
        return 0;
    return GetLastError() == ERROR_ALREADY_EXISTS && DirectoryExists(path) ? 0 : -1;
#else
    if (mkdir(path, 0700) == 0)
        return 0;
    return errno == EEXIST && DirectoryExists(path) ? 0 : -1;
#endif
}

void PcProfilesFree(void)
{
    free(sProfiles.items);
    memset(&sProfiles, 0, sizeof(sProfiles));
}

static int AddProfile(const char *name, const char *savePath, int isDefault)
{
    struct PcProfileInfo *profile;
    const char *separator;
    size_t directoryLength;

    if (sProfiles.count == sProfiles.capacity)
    {
        size_t capacity = sProfiles.capacity == 0 ? 8 : sProfiles.capacity * 2;
        struct PcProfileInfo *items;

        if (capacity < sProfiles.capacity || capacity > SIZE_MAX / sizeof(*items))
            return -1;
        items = realloc(sProfiles.items, capacity * sizeof(*items));
        if (items == NULL)
            return -1;
        sProfiles.items = items;
        sProfiles.capacity = capacity;
    }

    profile = &sProfiles.items[sProfiles.count++];
    memset(profile, 0, sizeof(*profile));
    if (snprintf(profile->name, sizeof(profile->name), "%s", name) >= (int)sizeof(profile->name)
     || snprintf(profile->savePath, sizeof(profile->savePath), "%s", savePath) >= (int)sizeof(profile->savePath))
        goto fail;
    profile->isDefault = isDefault;
    profile->hasSave = SaveExists(savePath);
    if (isDefault)
    {
        if (snprintf(profile->storagePath, sizeof(profile->storagePath), "storage")
            >= (int)sizeof(profile->storagePath))
            goto fail;
    }
    else
    {
        separator = strrchr(savePath, '/');
#ifdef _WIN32
        {
            const char *backslash = strrchr(savePath, '\\');

            if (backslash != NULL && (separator == NULL || backslash > separator))
                separator = backslash;
        }
#endif
        if (separator == NULL)
            goto fail;
        directoryLength = (size_t)(separator - savePath + 1);
        if (directoryLength + sizeof("storage") > sizeof(profile->storagePath))
            goto fail;
        memcpy(profile->storagePath, savePath, directoryLength);
        memcpy(profile->storagePath + directoryLength, "storage", sizeof("storage"));
    }
    return 0;

fail:
    sProfiles.count--;
    return -1;
}

static int CompareProfiles(const void *rawA, const void *rawB)
{
    const struct PcProfileInfo *a = rawA;
    const struct PcProfileInfo *b = rawB;

    return CaseCompare(a->name, b->name);
}

int PcProfilesScan(const char *defaultSavePath)
{
    char path[PC_PATH_MAX];

    PcProfilesFree();
    if (snprintf(sProfiles.defaultSavePath, sizeof(sProfiles.defaultSavePath), "%s", defaultSavePath)
        >= (int)sizeof(sProfiles.defaultSavePath)
     || GetProfilesDirectory(defaultSavePath, sProfiles.profilesDirectory,
                             sizeof(sProfiles.profilesDirectory)) != 0
     || AddProfile("Default", defaultSavePath, 1) != 0)
        goto fail;
    if (!DirectoryExists(sProfiles.profilesDirectory))
        return 0;

#ifdef _WIN32
    {
        WIN32_FIND_DATAA entry;
        HANDLE search;

        if (BuildPath(path, sizeof(path), sProfiles.profilesDirectory, "*") != 0)
            goto fail;
        search = FindFirstFileA(path, &entry);
        if (search == INVALID_HANDLE_VALUE)
            return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
        do
        {
            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsProfileName(entry.cFileName))
            {
                char directory[PC_PATH_MAX];

                if (BuildPath(directory, sizeof(directory), sProfiles.profilesDirectory, entry.cFileName) != 0
                 || BuildPath(path, sizeof(path), directory, "pokeemerald.sav") != 0
                 || AddProfile(entry.cFileName, path, 0) != 0)
                {
                    FindClose(search);
                    goto fail;
                }
            }
        } while (FindNextFileA(search, &entry));
        FindClose(search);
    }
#else
    {
        DIR *directory = opendir(sProfiles.profilesDirectory);
        struct dirent *entry;

        if (directory == NULL)
            goto fail;
        while ((entry = readdir(directory)) != NULL)
        {
            char profileDirectory[PC_PATH_MAX];

            if (!IsProfileName(entry->d_name)
             || BuildPath(profileDirectory, sizeof(profileDirectory), sProfiles.profilesDirectory,
                          entry->d_name) != 0
             || !DirectoryExists(profileDirectory))
                continue;
            if (BuildPath(path, sizeof(path), profileDirectory, "pokeemerald.sav") != 0
             || AddProfile(entry->d_name, path, 0) != 0)
            {
                closedir(directory);
                goto fail;
            }
        }
        closedir(directory);
    }
#endif
    if (sProfiles.count > 2)
        qsort(sProfiles.items + 1, sProfiles.count - 1, sizeof(*sProfiles.items), CompareProfiles);
    return 0;

fail:
    PcProfilesFree();
    return -1;
}

uint32_t PcProfilesCount(void)
{
    return (uint32_t)sProfiles.count;
}

int PcProfilesGet(uint32_t index, struct PcProfileInfo *info)
{
    if (index >= sProfiles.count || info == NULL)
        return -1;
    *info = sProfiles.items[index];
    return 0;
}

int PcProfilesCreate(const char *name, struct PcProfileInfo *info)
{
    char directory[PC_PATH_MAX];
    char savePath[PC_PATH_MAX];
    size_t i;

    if (!IsProfileName(name))
        return -1;
    for (i = 0; i < sProfiles.count; i++)
        if (CaseCompare(sProfiles.items[i].name, name) == 0)
            return -1;
    if (EnsureDirectory(sProfiles.profilesDirectory) != 0
     || BuildPath(directory, sizeof(directory), sProfiles.profilesDirectory, name) != 0
     || EnsureDirectory(directory) != 0
     || BuildPath(savePath, sizeof(savePath), directory, "pokeemerald.sav") != 0
     || AddProfile(name, savePath, 0) != 0)
        return -1;
    if (info != NULL)
        *info = sProfiles.items[sProfiles.count - 1];
    return 0;
}

int PcProfilesArchive(uint32_t index)
{
    char directory[PC_PATH_MAX];
    char stamp[32];
    char archiveName[PC_PROFILE_NAME_MAX + 64];
    char archivePath[PC_PATH_MAX];
    char *separator;
    struct tm local;
    time_t now = time(NULL);
    unsigned int suffix;

    if (index == 0 || index >= sProfiles.count)
        return -1;
    if (snprintf(directory, sizeof(directory), "%s", sProfiles.items[index].savePath)
        >= (int)sizeof(directory))
        return -1;
    separator = strrchr(directory, '/');
#ifdef _WIN32
    {
        char *backslash = strrchr(directory, '\\');
        if (backslash != NULL && (separator == NULL || backslash > separator))
            separator = backslash;
    }
#endif
    if (separator == NULL)
        return -1;
    *separator = '\0';
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    for (suffix = 0; suffix < 1000; suffix++)
    {
        if (suffix == 0)
            snprintf(archiveName, sizeof(archiveName), ".archived-%s-%s", stamp,
                     sProfiles.items[index].name);
        else
            snprintf(archiveName, sizeof(archiveName), ".archived-%s-%s-%u", stamp,
                     sProfiles.items[index].name, suffix);
        if (BuildPath(archivePath, sizeof(archivePath), sProfiles.profilesDirectory, archiveName) != 0)
            return -1;
        if (DirectoryExists(archivePath))
            continue;
#ifdef _WIN32
        return MoveFileA(directory, archivePath) ? 0 : -1;
#else
        return rename(directory, archivePath);
#endif
    }
    return -1;
}

int PcProfileResolve(const char *defaultSavePath,
                     const char *profileName,
                     char *savePath,
                     size_t savePathSize)
{
    size_t i;
    int result = -1;

    if (PcProfilesScan(defaultSavePath) != 0)
        return -1;
    for (i = 0; i < sProfiles.count; i++)
    {
        if (CaseCompare(sProfiles.items[i].name, profileName) == 0)
        {
            int length = snprintf(savePath, savePathSize, "%s", sProfiles.items[i].savePath);

            result = length >= 0 && (size_t)length < savePathSize ? 0 : -1;
            break;
        }
    }
    PcProfilesFree();
    return result;
}

int PcProfileResolveRemembered(const char *defaultSavePath,
                               char *savePath,
                               size_t savePathSize)
{
    char profilesDirectory[PC_PATH_MAX];
    char preferencePath[PC_PATH_MAX];
    char profileName[PC_PROFILE_NAME_MAX + 3];
    FILE *file;
    size_t length;

    if (GetProfilesDirectory(defaultSavePath, profilesDirectory, sizeof(profilesDirectory)) != 0
     || BuildPath(preferencePath, sizeof(preferencePath), profilesDirectory, LAST_PROFILE_FILE) != 0)
        return -1;
    file = fopen(preferencePath, "rb");
    if (file == NULL)
        return -1;
    length = fread(profileName, 1, sizeof(profileName), file);
    if (ferror(file) || length == sizeof(profileName))
    {
        fclose(file);
        return -1;
    }
    fclose(file);
    while (length != 0 && (profileName[length - 1] == '\n' || profileName[length - 1] == '\r'))
        length--;
    profileName[length] = '\0';
    if (CaseCompare(profileName, "Default") != 0 && !IsProfileName(profileName))
        return -1;
    return PcProfileResolve(defaultSavePath, profileName, savePath, savePathSize);
}

int PcProfileRememberBySavePath(const char *defaultSavePath, const char *savePath)
{
    char profilesDirectory[PC_PATH_MAX];
    char preferencePath[PC_PATH_MAX];
    char profileName[PC_PROFILE_NAME_MAX + 1];
    FILE *file;
    size_t i;
    int found = 0;

    if (PcProfilesScan(defaultSavePath) != 0)
        return -1;
    for (i = 0; i < sProfiles.count; i++)
    {
        if (strcmp(sProfiles.items[i].savePath, savePath) == 0)
        {
            memcpy(profileName, sProfiles.items[i].name, strlen(sProfiles.items[i].name) + 1);
            found = 1;
            break;
        }
    }
    PcProfilesFree();
    if (!found
     || GetProfilesDirectory(defaultSavePath, profilesDirectory, sizeof(profilesDirectory)) != 0
     || EnsureDirectory(profilesDirectory) != 0
     || BuildPath(preferencePath, sizeof(preferencePath), profilesDirectory, LAST_PROFILE_FILE) != 0)
        return -1;
    file = fopen(preferencePath, "wb");
    if (file == NULL)
        return -1;
    if (fprintf(file, "%s\n", profileName) < 0)
    {
        fclose(file);
        return -1;
    }
    return fclose(file) == 0 ? 0 : -1;
}
