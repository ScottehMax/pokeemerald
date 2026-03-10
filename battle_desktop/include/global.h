/*
 * global.h - Desktop shadow header
 *
 * Defines macros that are normally handled by the pokeemerald preproc tool
 * or GBA-specific toolchain, then chains to the real global.h.
 */
#ifndef DESKTOP_GLOBAL_PRELUDE_H
#define DESKTOP_GLOBAL_PRELUDE_H

/* Mark this as a desktop/modern build */
#ifndef MODERN
#define MODERN 1
#endif

/* The _() macro normally runs through the pokeemerald preproc tool which
 * applies charmap.txt to encode ASCII strings into GF byte encoding.
 * On desktop we append \xff (GF EOS = 0xFF) so BattleStringExpandPlaceholders
 * and other string functions terminate correctly at the end of the string. */
#ifndef _
#define _(x)  {x "\xff"}
#define __(x) {x "\xff"}
#endif

/* INCBIN macros load binary blobs from files. Stub them to zero initializers. */
#ifndef INCBIN
#define INCBIN(...)     {0}
#define INCBIN_U8       INCBIN
#define INCBIN_U16      INCBIN
#define INCBIN_U32      INCBIN
#define INCBIN_S8       INCBIN
#define INCBIN_S16      INCBIN
#define INCBIN_S32      INCBIN
#endif

/* Include the real global.h from include/ (next in the include search path) */
#include_next "global.h"

/* ---------------------------------------------------------------------------
 * Option A: delta-offset pointer decoding for battle scripts.
 *
 * GBA battle scripts embed 4-byte values that are either:
 *   - A 32-bit offset from gBattleScriptData[0]  (script->script jumps)
 *   - 0x80000000 | (varIndex << 16) | addend      (script->C variable refs)
 *
 * We override T1_READ_PTR / T2_READ_PTR (defined in include/global.h as
 * simple 4-byte reads) with a decoder that handles both cases.
 * --------------------------------------------------------------------------- */
#include <stdint.h>

extern const u8 * const gBattleScriptBase;
extern void *gBattleVarAddresses[];
extern const uint32_t gBattleScriptDataSize;

static inline void *DecodeScriptPtr_fn(const u8 *p)
{
    uint32_t v = (uint32_t)p[0]
               | ((uint32_t)p[1] << 8)
               | ((uint32_t)p[2] << 16)
               | ((uint32_t)p[3] << 24);
    if (v & 0x80000000u) {
        uint32_t idx    = (v >> 16) & 0x7FFFu;
        uint32_t addend = v & 0xFFFFu;
        return (u8 *)gBattleVarAddresses[idx] + addend;
    }
    return (u8 *)gBattleScriptBase + v;
}

#undef  T1_READ_PTR
#undef  T2_READ_PTR
#define T1_READ_PTR(ptr) ((u8 *)DecodeScriptPtr_fn(ptr))
#define T2_READ_PTR(ptr) (DecodeScriptPtr_fn(ptr))

#endif /* DESKTOP_GLOBAL_PRELUDE_H */
