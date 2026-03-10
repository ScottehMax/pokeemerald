#ifndef GUARD_BG_H
#define GUARD_BG_H

#include "gba/types.h"

struct BgTemplate {
    u8 bg:2;
    u8 charBaseIndex:2;
    u8 mapBaseIndex:5;
    u8 screenSize:2;
    u8 paletteMode:1;
    u8 priority:2;
    u8 baseTile:8;
};

struct BgConfig {
    u8 screenSize;
    u8 paletteMode;
    u8 priority;
    u8 mosaic;
    u8 charBaseBlock;
    u8 screenBaseBlock;
};

enum {
    BG_ATTR_SCREENSIZE,
    BG_ATTR_AREAOVERFLOW,
    BG_ATTR_SIZE,
    BG_ATTR_CHARBASEINDEX,
    BG_ATTR_MAPBASEINDEX,
    BG_ATTR_PALETTEMODE,
    BG_ATTR_PRIORITY,
    BG_ATTR_MOSAIC,
    BG_ATTR_WRAPAROUND,
    BG_ATTR_TYPE,
    BG_ATTR_VISIBLE,
    BG_ATTR_CHARBASEBLOCK,
    BG_ATTR_SCREENBASEBLOCK,
};
static inline void ResetBgsAndClearDma3BusyFlags(bool8 a) {}
static inline void InitBgsFromTemplates(u8 mode, const struct BgTemplate *templates, u8 count) {}
static inline void SetBgTilemapBuffer(u8 bg, void *buffer) {}
static inline void UnsetBgTilemapBuffer(u8 bg) {}
static inline void TransferPlttBuffer(void);
static inline void ShowBg(u8 bg) {}
static inline void HideBg(u8 bg) {}
static inline void SetBgControlAttributes(u8 bg, u8 charBaseBlock, u8 screenBaseBlock, u8 screenSize, u8 paletteMode, u8 priority, u8 mosaic, u8 wrap) {}
static inline void ChangeBgX(u8 bg, u32 value, u8 op) {}
static inline void ChangeBgY(u8 bg, u32 value, u8 op) {}
static inline u32 GetBgX(u8 bg) { return 0; }
static inline u32 GetBgY(u8 bg) { return 0; }
static inline void SetBgX(u8 bg, u32 value) {}
static inline void SetBgY(u8 bg, u32 value) {}
static inline void CopyToBgTilemapBuffer(u8 bg, const void *src, u16 size, u16 offset) {}
static inline void CopyToBgTilemapBufferRect(u8 bg, const void *src, u8 x, u8 y, u8 width, u8 height) {}
static inline void CopyBgTilemapBufferToVram(u8 bg) {}
static inline void FillBgTilemapBufferRect(u8 bg, u16 tileNum, u8 x, u8 y, u8 width, u8 height, u8 paletteNum) {}
static inline void FillBgTilemapBufferRect_Palette0(u8 bg, u16 tileNum, u8 x, u8 y, u8 width, u8 height) {}
static inline void WriteSequenceToBgTilemapBuffer(u8 bg, u16 startTileNum, u8 x, u8 y, u8 width, u8 height, u8 paletteNum, u16 count) {}
static inline bool8 IsDma3ManagerBusyWithBgCopy(void) { return FALSE; }
static inline void LoadBgTilemap(u8 bg, const void *src, u16 size, u16 offset) {}
static inline void LoadBgTiles(u8 bg, const void *src, u16 size, u16 offset) {}
static inline void SetBgAttribute(u8 bg, u8 attributeId, u32 value) {}
static inline u32 GetBgAttribute(u8 bg, u8 attributeId) { return 0; }
static inline u16 GetBgMetaTileAt(u8 bg, u8 x, u8 y) { return 0; }

#endif // GUARD_BG_H
