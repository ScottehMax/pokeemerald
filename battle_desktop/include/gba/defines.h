#ifndef GUARD_GBA_DEFINES_H
#define GUARD_GBA_DEFINES_H

#include <stddef.h>

#define TRUE  1
#define FALSE 0

/* On desktop, memory section attributes are removed */
#define IWRAM_DATA
#define EWRAM_DATA
#define COMMON_DATA
#define UNUSED __attribute__((unused))
#define NOINLINE __attribute__((noinline))
#define ALIGNED(n) __attribute__((aligned(n)))

/* GBA memory map constants - kept for any code that uses them as values,
   but pointers to these addresses are never dereferenced on desktop */
#define EWRAM_START 0x02000000
#define EWRAM_END   (EWRAM_START + 0x40000)
#define IWRAM_START 0x03000000
#define IWRAM_END   (IWRAM_START + 0x8000)

#define PLTT          0x5000000
#define BG_PLTT       PLTT
#define BG_PLTT_SIZE  0x200
#define OBJ_PLTT      (PLTT + BG_PLTT_SIZE)
#define OBJ_PLTT_SIZE 0x200
#define PLTT_SIZE     (BG_PLTT_SIZE + OBJ_PLTT_SIZE)

#define VRAM      0x6000000
#define VRAM_SIZE 0x18000
#define BG_VRAM           VRAM
#define BG_VRAM_SIZE      0x10000
#define BG_CHAR_SIZE      0x4000
#define BG_SCREEN_SIZE    0x800
#define BG_CHAR_ADDR(n)   (BG_VRAM + (BG_CHAR_SIZE * (n)))
#define BG_SCREEN_ADDR(n) (BG_VRAM + (BG_SCREEN_SIZE * (n)))

#define BG_TILE_H_FLIP(n) (0x400 + (n))
#define BG_TILE_V_FLIP(n) (0x800 + (n))
#define NUM_BACKGROUNDS 4

#define OBJ_VRAM0      (VRAM + 0x10000)
#define OBJ_VRAM0_SIZE 0x8000
#define OBJ_VRAM1      (VRAM + 0x14000)
#define OBJ_VRAM1_SIZE 0x4000

#define OAM      0x7000000
#define OAM_SIZE 0x400

#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 160

/* These are used as bit flags, not hardware addresses */
#define SOUND_INFO_PTR  (NULL)
#define INTR_CHECK      0
#define INTR_VECTOR     NULL

/* Desktop: no hardware interrupt table */
#define WIN_RANGE(a, b) (((a) << 8) | (b))

/* Tile size helpers */
#define TILE_WIDTH  8
#define TILE_HEIGHT 8
#define TILE_SIZE(bpp) ((bpp) * TILE_WIDTH * TILE_HEIGHT / 8)
#define TILE_SIZE_4BPP TILE_SIZE(4)
#define TILE_SIZE_8BPP TILE_SIZE(8)
#define TILE_OFFSET_4BPP(n) ((n) * TILE_SIZE_4BPP)
#define TILE_OFFSET_8BPP(n) ((n) * TILE_SIZE_8BPP)

#endif // GUARD_GBA_DEFINES_H
