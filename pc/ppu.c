#include "gba/gba.h"
#include "pc_diagnostics.h"
#include "field_camera.h"
#include "fieldmap.h"
#include "palette.h"
#include "pc_platform.h"
#include "pc_ppu.h"
#include "pc_shared.h"

#include <stddef.h>
#include <string.h>

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

struct BgScanlineState
{
    u16 control;
    u16 hofs;
    u16 vofs;
    s16 pa;
    s16 pb;
    s16 pc;
    s16 pd;
    s32 referenceX;
    s32 referenceY;
};

struct ColorEffectState
{
    u16 control;
    u8 eva;
    u8 evb;
    u8 evy;
};

#define NATIVE_TILESET_CACHE_COUNT 8
#define NATIVE_TILESET_TILE_BYTES (NUM_TILES_IN_PRIMARY * TILE_SIZE_4BPP)

struct NativeFieldTile
{
    u16 entry;
    const struct Tileset *primaryTileset;
    const struct Tileset *secondaryTileset;
};

struct NativeTilesetCache
{
    const struct Tileset *tileset;
    u8 tiles[NATIVE_TILESET_TILE_BYTES];
};

static struct NativeTilesetCache sNativeTilesetCache[NATIVE_TILESET_CACHE_COUNT];
static u8 sNextNativeTilesetCache;

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

static s32 ApplyMosaic(s32 coordinate, u8 size)
{
    s32 remainder = coordinate % size;

    if (remainder < 0)
        remainder += size;
    return coordinate - remainder;
}

static void ApplyBgMosaic(u16 control, u16 mosaic, s32 *screenX, s32 *screenY)
{
    if (!(control & BGCNT_MOSAIC))
        return;

    *screenX = ApplyMosaic(*screenX, (mosaic & 0xF) + 1);
    *screenY = ApplyMosaic(*screenY, ((mosaic >> 4) & 0xF) + 1);
}

static s32 FloorDivideByPowerOfTwo(s32 value, u8 shift)
{
    if (value >= 0)
        return value >> shift;
    return -(((-value) + (1 << shift) - 1) >> shift);
}

static u8 PositiveModulo16(s32 value)
{
    s32 remainder = value % 16;

    if (remainder < 0)
        remainder += 16;
    return remainder;
}

static s32 GetNativeFieldPhase(u16 scroll, u8 tileOffset)
{
    s32 phase = (scroll - tileOffset * 8) & 0xFF;

    if (phase >= 128)
        phase -= 256;
    return phase;
}

static const u8 *GetNativeTilesetTiles(const struct Tileset *tileset)
{
    struct NativeTilesetCache *cache = NULL;
    u8 i;

    for (i = 0; i < NATIVE_TILESET_CACHE_COUNT; i++)
    {
        if (sNativeTilesetCache[i].tileset == tileset)
            return sNativeTilesetCache[i].tiles;
        if (cache == NULL && sNativeTilesetCache[i].tileset == NULL)
            cache = &sNativeTilesetCache[i];
    }
    if (cache == NULL)
    {
        cache = &sNativeTilesetCache[sNextNativeTilesetCache++];
        if (sNextNativeTilesetCache == NATIVE_TILESET_CACHE_COUNT)
            sNextNativeTilesetCache = 0;
    }

    memset(cache->tiles, 0, sizeof(cache->tiles));
    if (tileset->isCompressed)
        LZ77UnCompWram(tileset->tiles, cache->tiles);
    else
        memcpy(cache->tiles, tileset->tiles, sizeof(cache->tiles));
    cache->tileset = tileset;
    return cache->tiles;
}

static const u8 *GetNativeFieldTileRow(u16 entry,
                                       u8 pixelY,
                                       const struct NativeFieldTile *fieldTile,
                                       const u8 *vram,
                                       u32 charBase)
{
    const struct Tileset *tileset;
    const struct Tileset *activeTileset;
    u16 tile = entry & 0x3FF;

    if (tile < NUM_TILES_IN_PRIMARY)
    {
        tileset = fieldTile->primaryTileset;
        activeTileset = gMapHeader.mapLayout->primaryTileset;
    }
    else
    {
        tileset = fieldTile->secondaryTileset;
        activeTileset = gMapHeader.mapLayout->secondaryTileset;
        tile -= NUM_TILES_IN_PRIMARY;
    }

    if (tileset == NULL || tileset == activeTileset)
        return vram + charBase + (entry & 0x3FF) * TILE_SIZE_4BPP + pixelY * 4;
    return GetNativeTilesetTiles(tileset) + tile * TILE_SIZE_4BPP + pixelY * 4;
}

static u8 TransformNativeColorComponent(u8 component, u8 shift, u8 paletteNum)
{
    u8 bestDistance = 0xFF;
    u8 transformed = component;
    u16 offset = paletteNum * 16;
    u8 i;

    for (i = 0; i < 16; i++)
    {
        u8 input = (gPlttBufferUnfaded[offset + i] >> shift) & 0x1F;
        u8 output = (gPlttBufferFaded[offset + i] >> shift) & 0x1F;
        u8 distance = input > component ? input - component : component - input;

        if (distance < bestDistance)
        {
            bestDistance = distance;
            transformed = output;
            if (distance == 0)
                break;
        }
    }
    return transformed;
}

static u16 GetNativeFieldColor(const struct NativeFieldTile *fieldTile,
                               u8 paletteNum,
                               u8 paletteIndex,
                               const u16 *palette)
{
    const struct Tileset *tileset;
    const struct Tileset *activeTileset;
    u16 color;

    if (paletteNum < NUM_PALS_IN_PRIMARY)
    {
        tileset = fieldTile->primaryTileset;
        activeTileset = gMapHeader.mapLayout->primaryTileset;
    }
    else
    {
        tileset = fieldTile->secondaryTileset;
        activeTileset = gMapHeader.mapLayout->secondaryTileset;
    }
    if (tileset == NULL || tileset == activeTileset)
        return palette[paletteNum * 16 + paletteIndex];

    color = tileset->palettes[paletteNum][paletteIndex];
    return TransformNativeColorComponent(color & 0x1F, 0, paletteNum)
         | (TransformNativeColorComponent((color >> 5) & 0x1F, 5, paletteNum) << 5)
         | (TransformNativeColorComponent((color >> 10) & 0x1F, 10, paletteNum) << 10);
}

// The original 240-pixel region still uses the GBA tilemap ring. Only pixels
// outside it resolve metatiles from the map, so native width cannot consume VRAM.
static struct NativeFieldTile GetNativeFieldTileEntry(u8 bg,
                                                       s32 mapX,
                                                       s32 mapY,
                                                       u8 withinX,
                                                       u8 withinY)
{
    struct NativeFieldTile result = {0};
    struct MapRenderMetatile renderMetatile;
    const struct Tileset *metatileTileset;
    const u16 *metatiles;
    u16 metatileId;
    u16 localMetatileId;
    u8 quadrant = (withinY >= 8 ? 2 : 0) + (withinX >= 8 ? 1 : 0);
    u8 layerType;

    MapGridGetMetatileAtForRender(mapX, mapY, &renderMetatile);
    result.primaryTileset = renderMetatile.primaryTileset;
    result.secondaryTileset = renderMetatile.secondaryTileset;
    if (result.primaryTileset == NULL || result.secondaryTileset == NULL)
        return result;

    metatileId = renderMetatile.metatileId;
    if (metatileId >= NUM_METATILES_TOTAL)
        metatileId = 0;
    if (metatileId < NUM_METATILES_IN_PRIMARY)
    {
        metatileTileset = result.primaryTileset;
        localMetatileId = metatileId;
    }
    else
    {
        metatileTileset = result.secondaryTileset;
        localMetatileId = metatileId - NUM_METATILES_IN_PRIMARY;
    }
    metatiles = metatileTileset->metatiles + localMetatileId * NUM_TILES_PER_METATILE;
    layerType = UNPACK_LAYER_TYPE(metatileTileset->metatileAttributes[localMetatileId]);

    switch (layerType)
    {
    case METATILE_LAYER_TYPE_SPLIT:
        if (bg == 3)
            result.entry = metatiles[quadrant];
        else if (bg == 1)
            result.entry = metatiles[4 + quadrant];
        break;
    case METATILE_LAYER_TYPE_COVERED:
        if (bg == 3)
            result.entry = metatiles[quadrant];
        else if (bg == 2)
            result.entry = metatiles[4 + quadrant];
        break;
    case METATILE_LAYER_TYPE_NORMAL:
    default:
        if (bg == 3)
            result.entry = 0x3014;
        else if (bg == 2)
            result.entry = metatiles[quadrant];
        else if (bg == 1)
            result.entry = metatiles[4 + quadrant];
        break;
    }
    return result;
}

static void DrawTextBgScanline(struct PixelStack *line,
                               const u8 *windowMasks,
                               const struct BgScanlineState *state,
                               u16 mosaic,
                               u8 bg,
                               u8 layer,
                               s32 screenY,
                               s32 outputWidth,
                               s32 outputOffsetX,
                               bool8 clipToGbaDisplay)
{
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)PLTT;
    u16 control = state->control;
    u32 size = control >> 14;
    u32 width = (size & 1) ? 512 : 256;
    u32 height = (size & 2) ? 512 : 256;
    u32 sourceY;
    u32 tileY;
    u32 mapBase = ((control >> 8) & 0x1F) * BG_SCREEN_SIZE;
    u32 charBase = ((control >> 2) & 3) * BG_CHAR_SIZE;
    bool8 mosaicEnabled = (control & BGCNT_MOSAIC) != 0;
    u8 mosaicWidth = (mosaic & 0xF) + 1;
    u8 mosaicHeight = ((mosaic >> 4) & 0xF) + 1;
    u32 cachedTileX = UINT32_MAX;
    bool8 cachedNativeField = FALSE;
    struct NativeFieldTile cachedNativeFieldTile = {0};
    const u8 *tileRow = NULL;
    u16 entry = 0;
    u8 fieldTileOffsetX = 0;
    u8 fieldTileOffsetY = 0;
    s32 fieldPhaseX = 0;
    s32 fieldPhaseY = 0;
    s32 outputX;

    if (mosaicEnabled)
        screenY = ApplyMosaic(screenY, mosaicHeight);
    sourceY = (screenY + state->vofs) & (height - 1);
    tileY = sourceY >> 3;
    if (outputWidth > DISPLAY_WIDTH && bg != 0)
    {
        GetFieldCameraBgTileOffset(&fieldTileOffsetX, &fieldTileOffsetY);
        fieldPhaseX = GetNativeFieldPhase(state->hofs, fieldTileOffsetX);
        fieldPhaseY = GetNativeFieldPhase(state->vofs, fieldTileOffsetY);
    }

    for (outputX = 0; outputX < outputWidth; outputX++)
    {
        s32 screenX = outputX - outputOffsetX;
        s32 effectiveX;
        s32 mapPixelX = 0;
        s32 mapPixelY = 0;
        s32 mapX = 0;
        s32 mapY = 0;
        u32 sourceX;
        u32 tileX;
        u32 pixelX;
        u32 pixelY;
        u8 paletteIndex;
        u16 color;
        bool8 nativeField = FALSE;

        if (clipToGbaDisplay
         && (screenX < 0 || screenX >= DISPLAY_WIDTH))
            continue;
        if (!(windowMasks[outputX] & layer))
            continue;

        effectiveX = mosaicEnabled ? ApplyMosaic(screenX, mosaicWidth) : screenX;
        if (outputWidth > DISPLAY_WIDTH && bg != 0)
        {
            mapPixelX = effectiveX + fieldPhaseX;
            mapPixelY = screenY + fieldPhaseY;
            mapX = gSaveBlock1Ptr->pos.x
                 + FloorDivideByPowerOfTwo(mapPixelX, 4);
            mapY = gSaveBlock1Ptr->pos.y
                 + FloorDivideByPowerOfTwo(mapPixelY, 4);
            nativeField = screenX < 0
                       || screenX >= DISPLAY_WIDTH
                       || mapX - MAP_OFFSET < 0
                       || mapX - MAP_OFFSET >= gMapHeader.mapLayout->width
                       || mapY - MAP_OFFSET < 0
                       || mapY - MAP_OFFSET >= gMapHeader.mapLayout->height;
        }
        if (nativeField)
        {
            s32 fieldTileX = FloorDivideByPowerOfTwo(mapPixelX, 3);

            pixelX = PositiveModulo16(mapPixelX) & 7;
            pixelY = PositiveModulo16(mapPixelY) & 7;
            tileX = (u32)fieldTileX;
            if (!cachedNativeField || tileX != cachedTileX)
            {
                u32 tile;

                cachedNativeFieldTile = GetNativeFieldTileEntry(bg,
                                                                mapX,
                                                                mapY,
                                                                PositiveModulo16(mapPixelX),
                                                                PositiveModulo16(mapPixelY));
                entry = cachedNativeFieldTile.entry;
                tile = entry & 0x3FF;
                if (entry & 0x800)
                    pixelY = 7 - pixelY;
                if (control & 0x80)
                    tileRow = vram + charBase + tile * TILE_SIZE_8BPP + pixelY * 8;
                else
                    tileRow = GetNativeFieldTileRow(entry,
                                                    pixelY,
                                                    &cachedNativeFieldTile,
                                                    vram,
                                                    charBase);
                cachedTileX = tileX;
                cachedNativeField = TRUE;
            }
        }
        else
        {
            sourceX = (effectiveX + state->hofs) & (width - 1);
            tileX = sourceX >> 3;
            pixelX = sourceX & 7;
            pixelY = sourceY & 7;
            if (cachedNativeField || tileX != cachedTileX)
            {
                u32 block = (tileX >> 5) + (tileY >> 5) * (width >> 8);
                u32 mapOffset = block * BG_SCREEN_SIZE
                              + ((tileY & 31) * 32 + (tileX & 31)) * 2;
                u32 tile;

                entry = *(const u16 *)(vram + mapBase + mapOffset);
                tile = entry & 0x3FF;
                if (entry & 0x800)
                    pixelY = 7 - pixelY;
                if (control & 0x80)
                    tileRow = vram + charBase + tile * TILE_SIZE_8BPP + pixelY * 8;
                else
                    tileRow = vram + charBase + tile * TILE_SIZE_4BPP + pixelY * 4;
                cachedTileX = tileX;
                cachedNativeField = FALSE;
            }
        }
        if (entry & 0x400)
            pixelX = 7 - pixelX;
        if (control & 0x80)
        {
            paletteIndex = tileRow[pixelX];
            color = palette[paletteIndex];
        }
        else
        {
            u8 packed = tileRow[pixelX >> 1];

            paletteIndex = (pixelX & 1) ? packed >> 4 : packed & 0xF;
            if (nativeField)
                color = GetNativeFieldColor(&cachedNativeFieldTile,
                                            (entry >> 12) & 0xF,
                                            paletteIndex,
                                            palette);
            else
                color = palette[((entry >> 12) & 0xF) * 16 + paletteIndex];
        }
        if (paletteIndex == 0)
            continue;
        PushPixel(&line[outputX], color, layer, FALSE);
    }
}

static bool8 ReadAffineBgPixel(const struct BgScanlineState *state,
                               u16 mosaic,
                               s32 screenX,
                               s32 screenY,
                               u16 *color)
{
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)PLTT;
    u16 control = state->control;
    s32 x;
    s32 y;
    u32 size = 128u << (control >> 14);
    u32 tileMapWidth = size >> 3;
    u32 mapBase = ((control >> 8) & 0x1F) * BG_SCREEN_SIZE;
    u32 charBase = ((control >> 2) & 3) * BG_CHAR_SIZE;
    u8 tile;
    u8 paletteIndex;

    ApplyBgMosaic(control, mosaic, &screenX, &screenY);
    x = (state->referenceX + state->pa * screenX + state->pb * screenY) >> 8;
    y = (state->referenceY + state->pc * screenX + state->pd * screenY) >> 8;

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

static bool8 ReadBitmapPixel(u8 mode,
                             u16 displayControl,
                             u16 bgControl,
                             u16 mosaic,
                             s32 x,
                             s32 y,
                             u16 *color)
{
    const u8 *vram = (const u8 *)VRAM;

    if (x < 0 || y < 0 || x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
        return FALSE;
    ApplyBgMosaic(bgControl, mosaic, &x, &y);

    if (mode == 3)
    {
        *color = *(const u16 *)(vram + (y * DISPLAY_WIDTH + x) * 2);
        return TRUE;
    }

    if (mode == 4)
    {
        u32 page = (displayControl & (1 << 4)) ? 0xA000 : 0;
        u8 paletteIndex = vram[page + y * DISPLAY_WIDTH + x];
        *color = ((const u16 *)PLTT)[paletteIndex];
        return TRUE;
    }

    if (mode == 5 && x < 160 && y < 128)
    {
        u32 page = (displayControl & (1 << 4)) ? 0xA000 : 0;
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

static void BuildWindowMasks(u8 *masks,
                             const bool8 *objectWindow,
                             s32 y,
                             s32 outputWidth,
                             s32 outputOffsetX)
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
    s32 outputX;

    if (!(displayControl & (DISPCNT_WIN0_ON | DISPCNT_WIN1_ON | DISPCNT_OBJWIN_ON)))
    {
        for (outputX = 0; outputX < outputWidth; outputX++)
            masks[outputX] = 0x3F;
        return;
    }

    for (outputX = 0; outputX < outputWidth; outputX++)
    {
        s32 screenX = outputX - outputOffsetX;
        bool8 inGbaDisplay = screenX >= 0 && screenX < DISPLAY_WIDTH;

        if (!inGbaDisplay)
        {
            // The overworld's full-screen WIN0 exposes BG1-BG3 and objects,
            // while WINOUT intentionally exposes only the BG0 text layer.
            // Extended columns are part of the field view, not GBA WINOUT.
            masks[outputX] = (windowIn & 0x3F) & ~(1 << 0);
            continue;
        }
        if ((displayControl & DISPCNT_WIN0_ON)
         && inWin0Y
         && CoordinateInWindow(screenX, win0Horizontal >> 8, win0Horizontal & 0xFF))
        {
            masks[outputX] = windowIn & 0x3F;
        }
        else if ((displayControl & DISPCNT_WIN1_ON)
              && inWin1Y
              && CoordinateInWindow(screenX, win1Horizontal >> 8, win1Horizontal & 0xFF))
        {
            masks[outputX] = (windowIn >> 8) & 0x3F;
        }
        else if ((displayControl & DISPCNT_OBJWIN_ON) && objectWindow[outputX])
        {
            masks[outputX] = (windowOut >> 8) & 0x3F;
        }
        else
        {
            masks[outputX] = windowOut & 0x3F;
        }
    }
}

static void DrawSpritesForPriority(struct PixelStack *line,
                                   const u8 *windowMasks,
                                   bool8 *objectWindow,
                                   s32 y,
                                   u8 priority,
                                   u8 mode,
                                   u16 displayControl,
                                   u16 mosaic,
                                   bool8 objectWindowPass,
                                   s32 outputWidth,
                                   s32 outputOffsetX)
{
    const u16 *oam = (const u16 *)OAM;
    const u8 *vram = (const u8 *)VRAM;
    const u16 *palette = (const u16 *)OBJ_PLTT;
    u8 mosaicWidth = ((mosaic >> 8) & 0xF) + 1;
    u8 mosaicHeight = ((mosaic >> 12) & 0xF) + 1;
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
        bool8 mosaicEnabled = (attr0 & (1 << 12)) != 0;
        bool8 color256 = (attr0 & (1 << 13)) != 0;
        u8 shape = attr0 >> 14;
        u8 size = attr1 >> 14;
        s32 width;
        s32 height;
        s32 drawWidth;
        s32 drawHeight;
        s32 rawObjectX = attr1 & 0x1FF;
        s32 objectX;
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
        if (outputWidth == DISPLAY_WIDTH)
        {
            objectX = rawObjectX;
            if (objectX >= 256)
                objectX -= 512;
        }
        else
        {
            s32 positiveX = rawObjectX;
            s32 negativeX = rawObjectX - 512;
            s32 viewportLeft = -outputOffsetX;
            s32 viewportRight = DISPLAY_WIDTH + outputOffsetX;
            bool8 positiveVisible = positiveX < viewportRight
                                 && positiveX + drawWidth > viewportLeft;
            bool8 negativeVisible = negativeX < viewportRight
                                 && negativeX + drawWidth > viewportLeft;

            if (!positiveVisible && negativeVisible)
                objectX = negativeX;
            else
                objectX = positiveX;
        }
        if (objectY >= 160)
            objectY -= 256;
        if (y < objectY || y >= objectY + drawHeight)
            continue;

        for (screenX = objectX; screenX < objectX + drawWidth; screenX++)
        {
            s32 outputX = screenX + outputOffsetX;
            s32 drawX;
            s32 drawY;
            s32 sourceX;
            s32 sourceY;
            u32 tileNumber;
            u32 tileOffset;
            u8 paletteIndex;
            u16 color;

            if (outputX < 0 || outputX >= outputWidth)
                continue;

            drawX = screenX - objectX;
            drawY = y - objectY;
            if (mosaicEnabled)
            {
                drawX = ApplyMosaic(drawX, mosaicWidth);
                drawY = ApplyMosaic(drawY, mosaicHeight);
            }

            if (affine)
            {
                u8 matrix = (attr1 >> 9) & 0x1F;
                const s16 *matrixBase = (const s16 *)(OAM + matrix * 32 + 6);
                s16 pa = matrixBase[0];
                s16 pb = matrixBase[4];
                s16 pc = matrixBase[8];
                s16 pd = matrixBase[12];
                s32 relativeX = drawX - drawWidth / 2;
                s32 relativeY = drawY - drawHeight / 2;

                sourceX = ((pa * relativeX + pb * relativeY) >> 8) + width / 2;
                sourceY = ((pc * relativeX + pd * relativeY) >> 8) + height / 2;
            }
            else
            {
                sourceX = drawX;
                sourceY = drawY;
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
            if (displayControl & (1 << 6))
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
                objectWindow[outputX] = TRUE;
            else if (windowMasks[outputX] & (1 << 4))
                PushObjectPixel(&line[outputX], color, objectMode == 1);
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

static u16 ApplyColorEffects(const struct PixelStack *stack,
                             const struct ColorEffectState *effects,
                             bool8 effectsEnabled)
{
    u8 effect = (effects->control >> 6) & 3;
    bool8 firstTarget = (effects->control & stack->top.layer) != 0;
    bool8 secondTarget = (effects->control & (stack->second.layer << 8)) != 0;

    // Semi-transparent OBJ pixels force alpha blending even where a window
    // disables the regular BLDCNT color effect.
    if (stack->top.semiTransparent && secondTarget)
        return BlendColors(stack->top.color, stack->second.color, effects->eva, effects->evb);
    if (!effectsEnabled)
        return stack->top.color;
    if (effect == 1 && firstTarget && secondTarget)
        return BlendColors(stack->top.color, stack->second.color, effects->eva, effects->evb);
    if (effect == 2 && firstTarget)
        return BrightenColor(stack->top.color, effects->evy);
    if (effect == 3 && firstTarget)
        return DarkenColor(stack->top.color, effects->evy);
    return stack->top.color;
}

void PcPpuRender(u32 *pixels,
                 u32 width,
                 PcInterruptCallback hblankCallback)
{
    const u16 *palette = (const u16 *)PLTT;
    u8 mode = REG_DISPCNT & 7;
    s32 outputOffsetX;
    s32 y;

    if (width < DISPLAY_WIDTH)
        width = DISPLAY_WIDTH;
    if (width > PC_FRAME_MAX_WIDTH)
        width = PC_FRAME_MAX_WIDTH;
    outputOffsetX = (width - DISPLAY_WIDTH) / 2;

    if (REG_DISPCNT & (1 << 7))
    {
        PcDiagnosticsSetRenderProgress(UINT32_MAX, PC_DIAGNOSTIC_RENDER_CLEAR);
        for (y = 0; y < (s32)(width * DISPLAY_HEIGHT); y++)
            pixels[y] = 0xFFFFFFFFu;
        REG_VCOUNT = 161;
        return;
    }

    for (y = 0; y < DISPLAY_HEIGHT; y++)
    {
        struct PixelStack line[PC_FRAME_MAX_WIDTH];
        bool8 objectWindow[PC_FRAME_MAX_WIDTH] = {FALSE};
        u8 windowMasks[PC_FRAME_MAX_WIDTH];
        struct BgScanlineState bgStates[4];
        struct ColorEffectState effects;
        u16 displayControl = REG_DISPCNT;
        u16 mosaic = REG_MOSAIC;
        s32 x;
        s32 priority;
        s32 bg;

        PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_CLEAR);
        REG_VCOUNT = (u16)y;
        for (bg = 0; bg < 4; bg++)
        {
            u32 registerBase;

            bgStates[bg].control = ReadIo16(REG_OFFSET_BG0CNT + bg * 2);
            bgStates[bg].hofs = ReadIo16(REG_OFFSET_BG0HOFS + bg * 4);
            bgStates[bg].vofs = ReadIo16(REG_OFFSET_BG0VOFS + bg * 4);
            if (bg < 2)
                continue;
            registerBase = bg == 2 ? REG_OFFSET_BG2PA : REG_OFFSET_BG3PA;
            bgStates[bg].pa = (s16)ReadIo16(registerBase + 0);
            bgStates[bg].pb = (s16)ReadIo16(registerBase + 2);
            bgStates[bg].pc = (s16)ReadIo16(registerBase + 4);
            bgStates[bg].pd = (s16)ReadIo16(registerBase + 6);
            bgStates[bg].referenceX = SignExtend28(*(vu32 *)(REG_BASE + registerBase + 8));
            bgStates[bg].referenceY = SignExtend28(*(vu32 *)(REG_BASE + registerBase + 12));
        }
        effects.control = REG_BLDCNT;
        effects.eva = REG_BLDALPHA & 0x1F;
        effects.evb = (REG_BLDALPHA >> 8) & 0x1F;
        effects.evy = REG_BLDY & 0x1F;
        if (effects.eva > 16)
            effects.eva = 16;
        if (effects.evb > 16)
            effects.evb = 16;
        if (effects.evy > 16)
            effects.evy = 16;

        if (displayControl & DISPCNT_OBJWIN_ON)
        {
            PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_OBJECT_WINDOW);
            DrawSpritesForPriority(NULL,
                                   NULL,
                                   objectWindow,
                                   y,
                                   0,
                                   mode,
                                   displayControl,
                                   mosaic,
                                   TRUE,
                                   width,
                                   outputOffsetX);
        }
        PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_WINDOWS);
        BuildWindowMasks(windowMasks, objectWindow, y, width, outputOffsetX);
        for (x = 0; x < (s32)width; x++)
        {
            line[x].top.color = palette[0];
            line[x].top.layer = 1 << 5;
            line[x].top.semiTransparent = FALSE;
            line[x].second = line[x].top;
        }

        for (priority = 3; priority >= 0; priority--)
        {
            PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_BACKGROUNDS);
            for (bg = 3; bg >= 0; bg--)
            {
                u16 control = bgStates[bg].control;

                if (!(displayControl & (1 << (8 + bg))))
                    continue;
                if ((control & 3) != priority)
                    continue;

                if (mode == 0 || (mode == 1 && bg < 2))
                {
                    DrawTextBgScanline(line,
                                       windowMasks,
                                       &bgStates[bg],
                                       mosaic,
                                       bg,
                                       (u8)(1 << bg),
                                       y,
                                       width,
                                       outputOffsetX,
                                       bg == 0);
                    continue;
                }

                for (x = 0; x < (s32)width; x++)
                {
                    s32 screenX = x - outputOffsetX;
                    u16 color;
                    bool8 opaque = FALSE;

                    if (!(windowMasks[x] & (1 << bg)))
                        continue;
                    if (mode == 1 && bg == 2)
                        opaque = ReadAffineBgPixel(&bgStates[bg], mosaic, screenX, y, &color);
                    else if (mode == 2 && bg >= 2)
                        opaque = ReadAffineBgPixel(&bgStates[bg], mosaic, screenX, y, &color);
                    else if (mode >= 3 && mode <= 5 && bg == 2)
                        opaque = ReadBitmapPixel(mode,
                                                 displayControl,
                                                 control,
                                                 mosaic,
                                                 screenX,
                                                 y,
                                                 &color);

                    if (opaque)
                        PushPixel(&line[x], color, (u8)(1 << bg), FALSE);
                }
            }

            if (displayControl & (1 << 12))
            {
                PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_SPRITES);
                DrawSpritesForPriority(line,
                                       windowMasks,
                                       objectWindow,
                                       y,
                                       (u8)priority,
                                       mode,
                                       displayControl,
                                       mosaic,
                                       FALSE,
                                       width,
                                       outputOffsetX);
            }
        }

        PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_OUTPUT);
        for (x = 0; x < (s32)width; x++)
        {
            bool8 effectsEnabled = (windowMasks[x] & (1 << 5)) != 0;

            pixels[y * width + x] = ColorToArgb(ApplyColorEffects(&line[x],
                                                                   &effects,
                                                                   effectsEnabled));
        }

        PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_HBLANK_DMA);
        PcDmaRunHBlank();
        if ((REG_IE & INTR_FLAG_HBLANK) && hblankCallback != NULL)
        {
            PcDiagnosticsSetRenderProgress((u32)y, PC_DIAGNOSTIC_RENDER_HBLANK_CALLBACK);
            hblankCallback();
        }
    }

    PcDiagnosticsSetRenderProgress(UINT32_MAX, PC_DIAGNOSTIC_RENDER_NONE);
    REG_VCOUNT = 161;
}
