#ifndef GUARD_MENU_H
#define GUARD_MENU_H
#include "gba/types.h"

struct MenuAction {
    const u8 *text;
    union {
        void (*void_u8)(u8);
        u8 (*u8_void)(void);
    } func;
};

#define MENU_NOTHING_CHOSEN -2
#define MENU_B_PRESSED -1
#define MENU_CURSOR_DELTA_NONE   0
#define MENU_CURSOR_DELTA_UP    -1
#define MENU_CURSOR_DELTA_DOWN   1
#define MENU_CURSOR_DELTA_LEFT  -1
#define MENU_CURSOR_DELTA_RIGHT  1

static inline s8 Menu_ProcessInputNoWrap(void) { return MENU_B_PRESSED; }
static inline s8 Menu_ProcessInput(void) { return MENU_B_PRESSED; }
static inline void DrawStdWindowAndBufferTiles(u8 windowId) {}
static inline void ClearStdWindowAndFrameToTransparent(u8 windowId, bool8 copyToVram) {}
static inline void DrawDialogueFrame(u8 windowId, bool8 copyToVram) {}

#endif /* GUARD_MENU_H */
