#ifndef GUARD_WINDOW_H
#define GUARD_WINDOW_H

#include "gba/types.h"

#define WINDOWS_MAX 32
#define PIXEL_FILL(num) ((num) | ((num) << 4))

enum {
    WINDOW_BG,
    WINDOW_TILEMAP_LEFT,
    WINDOW_TILEMAP_TOP,
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    WINDOW_PALETTE_NUM,
    WINDOW_BASE_BLOCK,
    WINDOW_TILE_DATA
};

enum {
    COPYWIN_NONE,
    COPYWIN_MAP,
    COPYWIN_GFX,
    COPYWIN_FULL,
};

struct WindowTemplate {
    u8 bg;
    u8 tilemapLeft;
    u8 tilemapTop;
    u8 width;
    u8 height;
    u8 paletteNum;
    u16 baseBlock;
};

#define DUMMY_WIN_TEMPLATE { .bg = 0xFF }
#define WINDOW_NONE 0xFF

struct Window {
    struct WindowTemplate window;
    u8 *tileData;
};

static inline bool16 InitWindows(const struct WindowTemplate *templates) { return TRUE; }
static inline u16 AddWindow(const struct WindowTemplate *template) { return 0; }
static inline int AddWindowWithoutTileMap(const struct WindowTemplate *template) { return 0; }
static inline void RemoveWindow(u8 windowId) {}
static inline void FreeAllWindowBuffers(void) {}
static inline void CopyWindowToVram(u8 windowId, u8 mode) {}
static inline void CopyWindowRectToVram(u32 windowId, u32 mode, u32 x, u32 y, u32 w, u32 h) {}
static inline void PutWindowTilemap(u8 windowId) {}
static inline void PutWindowRectTilemapOverridePalette(u8 windowId, u8 x, u8 y, u8 width, u8 height, u8 palette) {}
static inline void ClearWindowTilemap(u8 windowId) {}
static inline void ClearStdWindowAndFrame(u8 windowId, bool8 copyToVram) {}
static inline void DrawStdWindowFrame(u8 windowId, bool8 copyToVram) {}
static inline void FillWindowPixelRect(u8 windowId, u8 fillValue, u16 x, u16 y, u16 width, u16 height) {}
static inline void FillWindowPixelBuffer(u8 windowId, u8 fillValue) {}
static inline void BlitBitmapRectToWindow(u8 windowId, const u8 *pixels, u16 srcX, u16 srcY, u16 srcWidth, u16 srcHeight, u16 destX, u16 destY, u16 width, u16 height) {}
static inline void BlitBitmapToWindow(u8 windowId, const u8 *pixels, u16 x, u16 y, u16 width, u16 height) {}
static inline u8 WindowGetAttribute(u8 windowId, u8 attributeId) { return 0; }
static inline void SetWindowAttribute(u8 windowId, u8 attributeId, u32 value) {}
static inline u8 *GetWindowAttribute(u8 windowId, u8 attributeId) { return NULL; }

#endif // GUARD_WINDOW_H
