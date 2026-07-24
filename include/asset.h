#ifndef GUARD_ASSET_H
#define GUARD_ASSET_H

#include <stddef.h>
#include <stdint.h>

#include "gba/types.h"

#if PLATFORM_RELATIVE_POINTERS
typedef s32 AssetPtr;

static inline const void *ResolveAssetPointer(const AssetPtr *pointer)
{
    return *pointer == 0 ? NULL : (const u8 *)pointer + *pointer;
}
#else
typedef u32 AssetPtr;

static inline const void *ResolveAssetPointer(const AssetPtr *pointer)
{
    return (const void *)(uintptr_t)*pointer;
}
#endif

#define ASSET_POINTER(type, pointer) ((type)ResolveAssetPointer(pointer))
#define ASSET_TABLE_ENTRY(type, table, index) ASSET_POINTER(type, &(table)[index])

#endif // GUARD_ASSET_H
