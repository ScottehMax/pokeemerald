#ifndef GUARD_PC_SERVICES_H
#define GUARD_PC_SERVICES_H

#include "global.h"

struct BoxPokemon;

bool32 PcServicesInit(const char *savePath, const char *storagePath);
void PcServicesShutdown(void);

bool32 PcStorageScan(void);
u32 PcStorageCount(void);
bool32 PcStorageReadMon(u32 index, struct BoxPokemon *mon);
bool32 PcStorageWriteMon(const struct BoxPokemon *mon);
void PcStorageRollbackWrite(void);
bool32 PcStorageDeleteMon(u32 index);

#endif
