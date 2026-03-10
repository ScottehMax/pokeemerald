#ifndef GUARD_GBA_IO_REG_H
#define GUARD_GBA_IO_REG_H

/* GBA hardware I/O register offsets - kept as constants for SetGpuReg etc. */
#define REG_OFFSET_DISPCNT    0x000
#define REG_OFFSET_DISPSTAT   0x004
#define REG_OFFSET_VCOUNT     0x006
#define REG_OFFSET_BG0CNT     0x008
#define REG_OFFSET_BG1CNT     0x00A
#define REG_OFFSET_BG2CNT     0x00C
#define REG_OFFSET_BG3CNT     0x00E
#define REG_OFFSET_BG0HOFS    0x010
#define REG_OFFSET_BG0VOFS    0x012
#define REG_OFFSET_BG1HOFS    0x014
#define REG_OFFSET_BG1VOFS    0x016
#define REG_OFFSET_BG2HOFS    0x018
#define REG_OFFSET_BG2VOFS    0x01A
#define REG_OFFSET_BG3HOFS    0x01C
#define REG_OFFSET_BG3VOFS    0x01E
#define REG_OFFSET_WIN0H      0x040
#define REG_OFFSET_WIN1H      0x042
#define REG_OFFSET_WIN0V      0x044
#define REG_OFFSET_WIN1V      0x046
#define REG_OFFSET_WININ      0x048
#define REG_OFFSET_WINOUT     0x04A
#define REG_OFFSET_MOSAIC     0x04C
#define REG_OFFSET_BLDCNT     0x050
#define REG_OFFSET_BLDALPHA   0x052
#define REG_OFFSET_BLDY       0x054

/* Button constants */
#define DPAD_UP     0x0040
#define DPAD_DOWN   0x0080
#define DPAD_LEFT   0x0020
#define DPAD_RIGHT  0x0010
#define A_BUTTON    0x0001
#define B_BUTTON    0x0002
#define SELECT_BUTTON 0x0004
#define START_BUTTON  0x0008
#define L_BUTTON    0x0200
#define R_BUTTON    0x0100

#define JOY_NEW(keys) (0)
#define JOY_HELD(keys) (0)
#define JOY_REPEAT(keys) (0)

/* Desktop stubs for GBA hardware I/O registers (variables, not memory-mapped) */
#include <stdint.h>
extern volatile uint16_t REG_BG0HOFS, REG_BG0VOFS;
extern volatile uint16_t REG_BG1HOFS, REG_BG1VOFS;
extern volatile uint16_t REG_BG2HOFS, REG_BG2VOFS;
extern volatile uint16_t REG_BG3HOFS, REG_BG3VOFS;
extern volatile uint16_t REG_VCOUNT;

/* DISPCNT bits */
#define DISPCNT_OBJ_1D_MAP   0x0040
#define DISPCNT_OBJ_ON       0x1000

/* BGCNT bits */
#define BGCNT_TXT256x512     0x8000
#define BGCNT_SCREENBASE(n)  ((n) << 8)
#define BGCNT_CHARBASE(n)    ((n) << 2)

/* WININ/WINOUT bits */
#define WINOUT_WIN01_BG0     (1 << 0)
#define WINOUT_WIN01_BG1     (1 << 1)
#define WINOUT_WIN01_BG2     (1 << 2)
#define WINOUT_WIN01_BG3     (1 << 3)
#define WINOUT_WIN01_OBJ     (1 << 4)
#define WINOUT_WIN01_CLR     (1 << 5)
#define WINOUT_WIN01_BG_ALL  (WINOUT_WIN01_BG0 | WINOUT_WIN01_BG1 | WINOUT_WIN01_BG2 | WINOUT_WIN01_BG3)
#define WINOUT_WIN01_ALL     (WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR)

/* Palette size helpers */
#define PLTT_SIZEOF(n)       ((n) * 2 * 16)
#define PLTT_SIZE_4BPP       PLTT_SIZEOF(16)

#endif // GUARD_GBA_IO_REG_H
