#ifndef GUARD_TEXT_H
#define GUARD_TEXT_H

#include "gba/types.h"
#include "constants/characters.h"

#define TEXT_SKIP_DRAW 0xFF

struct TextPrinterTemplate {
    const u8 *currentChar;
    u8 windowId;
    u8 fontId;
    u8 x;
    u8 y;
    u8 currentX;
    u8 currentY;
    u8 letterSpacing;
    u8 lineSpacing;
    u8 unk:4;
    u8 fgColor:4;
    u8 bgColor:4;
    u8 shadowColor:4;
};

/* Text system - on desktop, text is written to stdout via the console controller.
   These stubs satisfy compile requirements; actual printing is handled by the
   console controller's PRINTSTRING handler using the GF string decoder. */

#define FONT_SMALL        0
#define FONT_NORMAL       1
#define FONT_SHORT        2
#define FONT_NARROW       3
#define FONT_SMALL_NARROW 4
#define FONT_BOLD         5
#define FONT_BRAILLE      6

static inline void InitTextPrinters(void) {}
static inline void RunTextPrinters(void) {}
static inline bool8 IsTextPrinterActive(u8 textPrinterId) { return FALSE; }
static inline u8 AddTextPrinterParameterized(u8 windowId, u8 fontId, const u8 *str, u8 x, u8 y, u8 speed, void *callback) { return 0; }
static inline u8 AddTextPrinterParameterized2(u8 windowId, u8 fontId, u8 x, u8 y, s8 letterSpacing, s8 lineSpacing, const struct FontInfo *font, u8 speed, const u8 *str) { return 0; }
static inline u8 AddTextPrinterParameterized3(u8 windowId, u8 fontId, u8 x, u8 y, const u8 *str, s8 letterSpacing, s8 lineSpacing) { return 0; }
static inline u8 AddTextPrinterParameterized4(u8 windowId, u8 fontId, u8 x, u8 y, s8 letterSpacing, s8 lineSpacing, const u8 *str, u8 speed, void *callback) { return 0; }
static inline u8 AddTextPrinterParameterized5(u8 windowId, u8 fontId, const u8 *str, u8 x, u8 y, u8 speed, void *callback, s8 letterSpacing, s8 lineSpacing) { return 0; }
static inline bool8 AddTextPrinter(struct TextPrinterTemplate *printerTemplate, u8 speed, void (*callback)(struct TextPrinterTemplate *, u16)) { return FALSE; }
static inline void GlyphWidth(u16 glyph, u8 fontId) {}
static inline u8 GetStringWidth(u8 fontId, const u8 *str, s16 letterSpacing) { return 0; }
static inline void DrawDownArrow(u8 windowId, u16 x, u16 y, u8 bgColor, bool8 drawArrow, u8 *counter, u8 *threshold) {}
static inline u8 RenderText(void) { return 0; }
static inline void SetPpuReg(u8 offset, u16 value) {}
static inline void DeactivateAllTextPrinters(void) {}
static inline bool8 RunTextPrintersAndIsPrinter0Active(void) { return FALSE; }

struct FontInfo;

typedef struct {
    bool8 canABSpeedUpPrint:1;
    bool8 useAlternateDownArrow:1;
    bool8 autoScroll:1;
    bool8 forceMidTextSpeed:1;
} TextFlags;

extern TextFlags gTextFlags;

#endif // GUARD_TEXT_H
