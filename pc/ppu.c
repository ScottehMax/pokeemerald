#include "gba/gba.h"
#include "pc_platform.h"
#include "pc_ppu.h"

#include <stddef.h>

struct LayerPixel
{
    u16 color;
    u8 layer;
    bool8 semiTransparent;
};

struct PixelStack
{
    struct LayerPixel top;
    struct LayerPixel second;
};

static u16 ReadIo16(u32 offset)
{
    return *(vu16 *)(REG_BASE + offset);
}

static s32 SignExtend28(u32 value)
{
    return (s32)(value << 4) >> 4;
}

static u32 ColorToArgb(u16 color)
{
    u32 red = color & 0x1F;
    u32 green = (color >> 5) & 0x1F;
    u32 blue = (color >> 10) & 0x1F;

    red = (red << 3) | (red >> 2);
    green = (green << 3) | (green >> 2);
    blue = (blue << 3) | (blue >> 2);
    return 0xFF000000u | (red << 16) | (green << 8) | blue;
}

static void PushPixel(struct PixelStack *stack, u16 color, u8 layer, bool8 semiTransparent)
{
    stack->second = stack->top;
    stack->top.color = color;
    stack->top.layer = layer;
    stack->top.semiTransparent = semiTransparent;
}

static void PushObjectPixel(struct PixelStack *stack, u16 color, bool8 semiTransparent)
{
    // OAM evaluation resolves overlapping sprites into a single OBJ layer
    // before that layer enters the color-effects compositor.
    if (stack->top.layer != (1 << 4))
        stack->second = stack->top;
    stack->top.color = color;
    stack->top.layer = 1 << 4;
    stack->top.semiTransparent = semiTransparent;
}

static bool8 ReadTextBgPixel(u8 bg, s32 screenX, s32 screenY, u16 *color)
{
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)PLTT;
    u16 control = ReadIo16(REG_OFFSET_BG0CNT + bg * 2);
    u16 hofs = ReadIo16(REG_OFFSET_BG0HOFS + bg * 4);
    u16 vofs = ReadIo16(REG_OFFSET_BG0VOFS + bg * 4);
    u32 size = control >> 14;
    u32 width = (size & 1) ? 512 : 256;
    u32 height = (size & 2) ? 512 : 256;
    u32 x = (screenX + hofs) & (width - 1);
    u32 y = (screenY + vofs) & (height - 1);
    u32 tileX = x >> 3;
    u32 tileY = y >> 3;
    u32 block = (tileX >> 5) + (tileY >> 5) * (width >> 8);
    u32 mapBase = ((control >> 8) & 0x1F) * BG_SCREEN_SIZE;
    u32 mapOffset = block * BG_SCREEN_SIZE + ((tileY & 31) * 32 + (tileX & 31)) * 2;
    u16 entry = *(const u16 *)(vram + mapBase + mapOffset);
    u32 tile = entry & 0x3FF;
    u32 pixelX = x & 7;
    u32 pixelY = y & 7;
    u32 charBase = ((control >> 2) & 3) * BG_CHAR_SIZE;
    u8 paletteIndex;

    if (entry & 0x400)
        pixelX = 7 - pixelX;
    if (entry & 0x800)
        pixelY = 7 - pixelY;

    if (control & 0x80)
    {
        paletteIndex = vram[charBase + tile * TILE_SIZE_8BPP + pixelY * 8 + pixelX];
        if (paletteIndex == 0)
            return FALSE;
        *color = palette[paletteIndex];
    }
    else
    {
        u8 packed = vram[charBase + tile * TILE_SIZE_4BPP + pixelY * 4 + pixelX / 2];
        paletteIndex = (pixelX & 1) ? packed >> 4 : packed & 0xF;
        if (paletteIndex == 0)
            return FALSE;
        *color = palette[((entry >> 12) & 0xF) * 16 + paletteIndex];
    }

    return TRUE;
}

static bool8 ReadAffineBgPixel(u8 bg, s32 screenX, s32 screenY, u16 *color)
{
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)PLTT;
    u32 registerBase = bg == 2 ? REG_OFFSET_BG2PA : REG_OFFSET_BG3PA;
    u16 control = ReadIo16(REG_OFFSET_BG0CNT + bg * 2);
    s16 pa = (s16)ReadIo16(registerBase + 0);
    s16 pb = (s16)ReadIo16(registerBase + 2);
    s16 pc = (s16)ReadIo16(registerBase + 4);
    s16 pd = (s16)ReadIo16(registerBase + 6);
    u32 rawX = *(vu32 *)(REG_BASE + registerBase + 8);
    u32 rawY = *(vu32 *)(REG_BASE + registerBase + 12);
    s32 referenceX = SignExtend28(rawX);
    s32 referenceY = SignExtend28(rawY);
    s32 x = (referenceX + pa * screenX + pb * screenY) >> 8;
    s32 y = (referenceY + pc * screenX + pd * screenY) >> 8;
    u32 size = 128u << (control >> 14);
    u32 tileMapWidth = size >> 3;
    u32 mapBase = ((control >> 8) & 0x1F) * BG_SCREEN_SIZE;
    u32 charBase = ((control >> 2) & 3) * BG_CHAR_SIZE;
    u8 tile;
    u8 paletteIndex;

    if (control & (1 << 13))
    {
        x &= size - 1;
        y &= size - 1;
    }
    else if (x < 0 || y < 0 || x >= (s32)size || y >= (s32)size)
    {
        return FALSE;
    }

    tile = vram[mapBase + (y >> 3) * tileMapWidth + (x >> 3)];
    paletteIndex = vram[charBase + tile * TILE_SIZE_8BPP + (y & 7) * 8 + (x & 7)];
    if (paletteIndex == 0)
        return FALSE;

    *color = palette[paletteIndex];
    return TRUE;
}

static bool8 ReadBitmapPixel(u8 mode, s32 x, s32 y, u16 *color)
{
    const u8 *vram = (const u8 *)VRAM;

    if (mode == 3)
    {
        *color = *(const u16 *)(vram + (y * DISPLAY_WIDTH + x) * 2);
        return TRUE;
    }

    if (mode == 4)
    {
        u32 page = (REG_DISPCNT & (1 << 4)) ? 0xA000 : 0;
        u8 paletteIndex = vram[page + y * DISPLAY_WIDTH + x];
        *color = ((const u16 *)PLTT)[paletteIndex];
        return TRUE;
    }

    if (mode == 5 && x < 160 && y < 128)
    {
        u32 page = (REG_DISPCNT & (1 << 4)) ? 0xA000 : 0;
        *color = *(const u16 *)(vram + page + (y * 160 + x) * 2);
        return TRUE;
    }

    return FALSE;
}

static void GetSpriteDimensions(u8 shape, u8 size, s32 *width, s32 *height)
{
    static const u8 dimensions[3][4][2] =
    {
        {{8, 8}, {16, 16}, {32, 32}, {64, 64}},
        {{16, 8}, {32, 8}, {32, 16}, {64, 32}},
        {{8, 16}, {8, 32}, {16, 32}, {32, 64}},
    };

    *width = dimensions[shape][size][0];
    *height = dimensions[shape][size][1];
}

static bool8 CoordinateInWindow(s32 coordinate, u8 start, u8 end)
{
    if (start == end)
        return FALSE;
    if (start < end)
        return coordinate >= start && coordinate < end;
    return coordinate >= start || coordinate < end;
}

static void BuildWindowMasks(u8 *masks, const bool8 *objectWindow, s32 y)
{
    u16 displayControl = REG_DISPCNT;
    u16 windowIn = REG_WININ;
    u16 windowOut = REG_WINOUT;
    u16 win0Horizontal = REG_WIN0H;
    u16 win1Horizontal = REG_WIN1H;
    u16 win0Vertical = REG_WIN0V;
    u16 win1Vertical = REG_WIN1V;
    bool8 inWin0Y = CoordinateInWindow(y, win0Vertical >> 8, win0Vertical & 0xFF);
    bool8 inWin1Y = CoordinateInWindow(y, win1Vertical >> 8, win1Vertical & 0xFF);
    s32 x;

    if (!(displayControl & (DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJWIN_ON)))
    {
        for (x = 0; x < DISPLAY_WIDTH; x++)
            masks[x] = 0x3F;
        return;
    }

    for (x = 0; x < DISPLAY_WIDTH; x++)
    {
        if ((displayControl & DISPCNT_WIN0_ON)
         && inWin0Y
         && CoordinateInWindow(x, win0Horizontal >> 8, win0Horizontal & 0xFF))
        {
            masks[x] = windowIn & 0x3F;
        }
        else if ((displayControl & DISPCNT_WIN1_ON)
              && inWin1Y
              && CoordinateInWindow(x, win1Horizontal >> 8, win1Horizontal & 0xFF))
        {
            masks[x] = (windowIn >> 8) & 0x3F;
        }
        else if ((displayControl & DISPCNT_OBJWIN_ON) && objectWindow[x])
        {
            masks[x] = (windowOut >> 8) & 0x3F;
        }
        else
        {
            masks[x] = windowOut & 0x3F;
        }
    }
}

static void DrawSpritesForPriority(struct PixelStack *line,
                                   const u8 *windowMasks,
                                   bool8 *objectWindow,
                                   s32 y,
                                   u8 priority,
                                   u8 mode,
                                   bool8 objectWindowPass)
{
    const u16 *oam = (const u16 *)OAM;
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)OBJ_PLTT;
    s32 sprite;

    for (sprite = 127; sprite >= 0; sprite--)
    {
        const u16 *entry = &oam[sprite * 4];
        u16 attr0 = entry[0];
        u16 attr1 = entry[1];
        u16 attr2 = entry[2];
        bool8 affine = (attr0 & (1 << 8)) != 0;
        bool8 doubleSize = affine && (attr0 & (1 << 9));
        u8 objectMode = (attr0 >> 10) & 3;
        bool8 color256 = (attr0 & (1 << 13)) != 0;
        u8 shape = attr0 >> 14;
        u8 size = attr1 >> 14;
        s32 width;
        s32 height;
        s32 drawWidth;
        s32 drawHeight;
        s32 objectX = attr1 & 0x1FF;
        s32 objectY = attr0 & 0xFF;
        s32 screenX;

        if (shape >= 3 || objectMode == 3)
            continue;
        if (objectWindowPass)
        {
            if (objectMode != 2)
                continue;
        }
        else if (((attr2 >> 10) & 3) != priority || objectMode >= 2)
        {
            continue;
        }
        if (!affine && (attr0 & (1 << 9)))
            continue;

        GetSpriteDimensions(shape, size, &width, &height);
        drawWidth = doubleSize ? width * 2 : width;
        drawHeight = doubleSize ? height * 2 : height;
        if (objectX >= 256)
            objectX -= 512;
        if (objectY >= 160)
            objectY -= 256;
        if (y < objectY || y >= objectY + drawHeight)
            continue;

        for (screenX = objectX; screenX < objectX + drawWidth; screenX++)
        {
            s32 sourceX;
            s32 sourceY;
            u32 tileNumber;
            u32 tileOffset;
            u8 paletteIndex;
            u16 color;

            if (screenX < 0 || screenX >= DISPLAY_WIDTH)
                continue;

            if (affine)
            {
                u8 matrix = (attr1 >> 9) & 0x1F;
                const s16 *matrixBase = (const s16 *)(OAM + matrix * 32 + 6);
                s16 pa = matrixBase[0];
                s16 pb = matrixBase[4];
                s16 pc = matrixBase[8];
                s16 pd = matrixBase[12];
                s32 relativeX = screenX - objectX - drawWidth / 2;
                s32 relativeY = y - objectY - drawHeight / 2;

                sourceX = ((pa * relativeX + pb * relativeY) >> 8) + width / 2;
                sourceY = ((pc * relativeX + pd * relativeY) >> 8) + height / 2;
            }
            else
            {
                sourceX = screenX - objectX;
                sourceY = y - objectY;
                if (attr1 & (1 << 12))
                    sourceX = width - 1 - sourceX;
                if (attr1 & (1 << 13))
                    sourceY = height - 1 - sourceY;
            }

            if (sourceX < 0 || sourceY < 0 || sourceX >= width || sourceY >= height)
                continue;

            tileNumber = attr2 & 0x3FF;
            if (color256)
                tileNumber &= ~1u;
            if (REG_DISPCNT & (1 << 6))
                tileNumber += (sourceY >> 3) * (width >> 3) * (color256 ? 2 : 1);
            else
                tileNumber += (sourceY >> 3) * 32;
            tileNumber += (sourceX >> 3) * (color256 ? 2 : 1);
            tileOffset = (mode >= 3 ? 0x14000 : 0x10000) + tileNumber * 32;

            if (color256)
            {
                paletteIndex = vram[tileOffset + (sourceY & 7) * 8 + (sourceX & 7)];
                if (paletteIndex == 0)
                    continue;
                color = palette[paletteIndex];
            }
            else
            {
                u8 packed = vram[tileOffset + (sourceY & 7) * 4 + ((sourceX & 7) >> 1)];
                paletteIndex = (sourceX & 1) ? packed >> 4 : packed & 0xF;
                if (paletteIndex == 0)
                    continue;
                color = palette[((attr2 >> 12) & 0xF) * 16 + paletteIndex];
            }

            if (objectMode == 2)
                objectWindow[screenX] = TRUE;
            else if (windowMasks[screenX] & (1 << 4))
                PushObjectPixel(&line[screenX], color, objectMode == 1);
        }
    }
}

static u16 BlendColors(u16 first, u16 second, u8 eva, u8 evb)
{
    u32 red = ((first & 0x1F) * eva + (second & 0x1F) * evb) >> 4;
    u32 green = (((first >> 5) & 0x1F) * eva + ((second >> 5) & 0x1F) * evb) >> 4;
    u32 blue = (((first >> 10) & 0x1F) * eva + ((second >> 10) & 0x1F) * evb) >> 4;

    if (red > 31)
        red = 31;
    if (green > 31)
        green = 31;
    if (blue > 31)
        blue = 31;
    return (u16)(red | (green << 5) | (blue << 10));
}

static u16 BrightenColor(u16 color, u8 amount)
{
    u32 red = color & 0x1F;
    u32 green = (color >> 5) & 0x1F;
    u32 blue = (color >> 10) & 0x1F;

    red += ((31 - red) * amount) >> 4;
    green += ((31 - green) * amount) >> 4;
    blue += ((31 - blue) * amount) >> 4;
    return (u16)(red | (green << 5) | (blue << 10));
}

static u16 DarkenColor(u16 color, u8 amount)
{
    u32 red = (color & 0x1F) * (16 - amount) >> 4;
    u32 green = ((color >> 5) & 0x1F) * (16 - amount) >> 4;
    u32 blue = ((color >> 10) & 0x1F) * (16 - amount) >> 4;

    return (u16)(red | (green << 5) | (blue << 10));
}

static u16 ApplyColorEffects(const struct PixelStack *stack, bool8 effectsEnabled)
{
    u16 blendControl = REG_BLDCNT;
    u8 effect = (blendControl >> 6) & 3;
    bool8 firstTarget = (blendControl & stack->top.layer) != 0;
    bool8 secondTarget = (blendControl & (stack->second.layer << 8)) != 0;
    u8 eva = REG_BLDALPHA & 0x1F;
    u8 evb = (REG_BLDALPHA >> 8) & 0x1F;
    u8 evy = REG_BLDY & 0x1F;

    if (!effectsEnabled)
        return stack->top.color;
    if (eva > 16)
        eva = 16;
    if (evb > 16)
        evb = 16;
    if (evy > 16)
        evy = 16;

    if ((stack->top.semiTransparent || (effect == 1 && firstTarget)) && secondTarget)
        return BlendColors(stack->top.color, stack->second.color, eva, evb);
    if (effect == 2 && firstTarget)
        return BrightenColor(stack->top.color, evy);
    if (effect == 3 && firstTarget)
        return DarkenColor(stack->top.color, evy);
    return stack->top.color;
}

void PcPpuRender(u32 *pixels, PcInterruptCallback hblankCallback)
{
    const u16 *palette = (const u16 *)PLTT;
    u8 mode = REG_DISPCNT & 7;
    s32 y;

    if (REG_DISPCNT & (1 << 7))
    {
        for (y = 0; y < DISPLAY_WIDTH * DISPLAY_HEIGHT; y++)
            pixels[y] = 0xFFFFFFFFu;
        REG_VCOUNT = 161;
        return;
    }

    for (y = 0; y < DISPLAY_HEIGHT; y++)
    {
        struct PixelStack line[DISPLAY_WIDTH];
        bool8 objectWindow[DISPLAY_WIDTH] = {FALSE};
        u8 windowMasks[DISPLAY_WIDTH];
        s32 x;
        s32 priority;

        REG_VCOUNT = (u16)y;
        if (REG_DISPCNT & DISPCNT_OBJWIN_ON)
            DrawSpritesForPriority(NULL, NULL, objectWindow, y, 0, mode, TRUE);
        BuildWindowMasks(windowMasks, objectWindow, y);
        for (x = 0; x < DISPLAY_WIDTH; x++)
        {
            line[x].top.color = palette[0];
            line[x].top.layer = 1 << 5;
            line[x].top.semiTransparent = FALSE;
            line[x].second = line[x].top;
        }

        for (priority = 3; priority >= 0; priority--)
        {
            s32 bg;

            for (bg = 3; bg >= 0; bg--)
            {
                u16 control;

                if (!(REG_DISPCNT & (1 << (8 + bg))))
                    continue;
                control = ReadIo16(REG_OFFSET_BG0CNT + bg * 2);
                if ((control & 3) != priority)
                    continue;

                for (x = 0; x < DISPLAY_WIDTH; x++)
                {
                    u16 color;
                    bool8 opaque = FALSE;

                    if (!(windowMasks[x] & (1 << bg)))
                        continue;
                    if (mode == 0)
                        opaque = ReadTextBgPixel((u8)bg, x, y, &color);
                    else if (mode == 1)
                    {
                        if (bg < 2)
                            opaque = ReadTextBgPixel((u8)bg, x, y, &color);
                        else if (bg == 2)
                            opaque = ReadAffineBgPixel((u8)bg, x, y, &color);
                    }
                    else if (mode == 2 && bg >= 2)
                        opaque = ReadAffineBgPixel((u8)bg, x, y, &color);
                    else if (mode >= 3 && mode <= 5 && bg == 2)
                        opaque = ReadBitmapPixel(mode, x, y, &color);

                    if (opaque)
                        PushPixel(&line[x], color, (u8)(1 << bg), FALSE);
                }
            }

            if (REG_DISPCNT & (1 << 12))
                DrawSpritesForPriority(line,
                                       windowMasks,
                                       objectWindow,
                                       y,
                                       (u8)priority,
                                       mode,
                                       FALSE);
        }

        for (x = 0; x < DISPLAY_WIDTH; x++)
        {
            bool8 effectsEnabled = (windowMasks[x] & (1 << 5)) != 0;

            pixels[y * DISPLAY_WIDTH + x] = ColorToArgb(ApplyColorEffects(&line[x], effectsEnabled));
        }

        PcDmaRunHBlank();
        if ((REG_IE & INTR_FLAG_HBLANK) && hblankCallback != NULL)
            hblankCallback();
    }

    REG_VCOUNT = 161;
}
