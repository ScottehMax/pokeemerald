#ifndef GUARD_PC_PROFILES_H
#define GUARD_PC_PROFILES_H

#include <stddef.h>
#include <stdint.h>

#include "pc_shared.h"

#define PC_PROFILE_NAME_MAX 16

struct PcProfileInfo
{
    char name[PC_PROFILE_NAME_MAX + 1];
    char savePath[PC_PATH_MAX];
    char storagePath[PC_PATH_MAX];
    int hasSave;
    int isDefault;
};

int PcProfilesScan(const char *defaultSavePath);
uint32_t PcProfilesCount(void);
int PcProfilesGet(uint32_t index, struct PcProfileInfo *info);
int PcProfilesCreate(const char *name, struct PcProfileInfo *info);
int PcProfilesArchive(uint32_t index);
void PcProfilesFree(void);

int PcProfileResolve(const char *defaultSavePath,
                     const char *profileName,
                     char *savePath,
                     size_t savePathSize);

#endif
