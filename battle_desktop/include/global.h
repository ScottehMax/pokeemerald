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

#endif /* DESKTOP_GLOBAL_PRELUDE_H */
