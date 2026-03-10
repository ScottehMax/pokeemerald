#ifndef GUARD_ALLOC_H
#define GUARD_ALLOC_H

#include "gba/types.h"
#include <stdlib.h>

#define FREE_AND_SET_NULL(ptr)   { free(ptr); ptr = NULL; }
#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

/* On desktop, use the standard C heap instead of the GBA fixed heap */
#define HEAP_SIZE 0x1C000
extern u8 gHeap[HEAP_SIZE];

static inline void *Alloc(u32 size) { return calloc(1, size); }
static inline void *AllocZeroed(u32 size) { return calloc(1, size); }
static inline void Free(void *pointer) { free(pointer); }
static inline void InitHeap(void *heapStart, u32 heapSize) {}

#endif // GUARD_ALLOC_H
