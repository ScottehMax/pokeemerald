# Touch-Native Android Design

## Goals

The Android port should behave like a native touch game rather than an emulator
wrapped in virtual controls. It should:

- fill landscape and portrait displays with useful game content;
- use taps to walk, interact with object events, and advance dialogue;
- keep the original game rules, scripts, collision, and interaction logic as the
  source of truth;
- adapt menus, battles, and dialogue to the available viewport over time;
- retain controller and keyboard input as first-class alternatives.

## Rendering Invariant

Every visible game element uses one logical pixel grid and one uniform nearest-
neighbour scale. World tiles, object sprites, text, windows, menus, effects, and
touch affordances must never use independent pixel scales.

The logical framebuffer follows the physical display aspect ratio:

- landscape keeps a 160-pixel logical height and expands the logical width;
- portrait keeps a 240-pixel logical width and expands the logical height;
- the complete logical framebuffer is scaled once to the display;
- rounding may leave less than one logical pixel unused, but the image is never
  stretched to hide an aspect mismatch.

Responsive UI means rearranging or enlarging layout regions on this shared grid,
not scaling individual UI components separately. Compose is appropriate for
separate platform screens such as settings, but not for overlays that visually
mix with the game framebuffer.

## Architecture

### Dynamic viewport

The frontend publishes the desired logical width and height. The game core uses
the expanded dimensions only for scenes that support them. Other scenes remain
on the original 240x160 canvas until their layouts have been adapted, and are
centred without distortion.

The PPU continues to execute exactly 160 emulated GBA scanlines. Extra field rows
are rendered without advancing VCOUNT, HBlank DMA, or HBlank interrupts. This
keeps game timing and scanline effects compatible with the original engine.

### Expanded overworld

Pixels outside the original canvas resolve metatiles through the map grid and
map connections. The original VRAM tilemap remains unchanged. Vertical expansion
must extend all viewport-dependent systems together:

- background tile resolution;
- connected-map coverage;
- object-event spawning and removal;
- sprite visibility and unwrapped screen coordinates;
- camera-relative hit testing.

An expanded view is gameplay space, not decoration. NPCs and interactable
objects must appear and behave throughout the visible viewport.

### Semantic touch commands

The frontend sends touch position, phase, and viewport metadata to the core. The
core interprets a tap according to game state:

- overworld ground: find a collision-valid A* path and walk it;
- object event: path to a valid adjacent tile, face it, then invoke the normal
  interaction path;
- active dialogue: advance through the normal A-button input path;
- menus and battles: dispatch to semantic hit regions supplied by the active
  screen.

Touch commands must not write player coordinates, bypass scripts, or duplicate
interaction rules. Manual input cancels queued navigation immediately. Map
changes, scripts, battles, forced movement, and collision changes also invalidate
the active path.

### Pathfinding

A* runs on the current map-grid collision data with the same movement constraints
used by the field engine. The search is bounded to a practical region and is
recomputed when the route becomes invalid. Warps and map scripts are reached by
normal movement rather than simulated directly.

## Delivery Phases

1. Generalize the shared framebuffer and Android frontend for dynamic width and
   height while preserving one uniform scale.
2. Extend overworld backgrounds, connected maps, object events, and sprites to
   portrait viewports.
3. Add the semantic touch bridge and tap-to-advance dialogue.
4. Add A* walking and object-event interaction with cancellation and invalidation.
5. Introduce responsive dialogue, menu, and battle layouts on the same logical
   grid, screen by screen.
6. Replace virtual controls with contextual touch affordances while retaining an
   optional compatibility control mode.

## Acceptance Criteria

- Landscape and portrait overworlds fill the display at one uniform pixel scale.
- No tile, sprite, UI, or text element has a different pixel size from another.
- Connected maps and object events cover every visible field tile.
- Rotation cannot corrupt renderer, map, object, or input state.
- Tapping a reachable tile follows a valid route and triggers normal field logic.
- Tapping an object walks to it and uses the same scripts as an A-button action.
- Tapping dialogue advances it without accidental field movement.
- Controller and keyboard behaviour remains compatible.
- Unsupported screens remain undistorted until their responsive layout is ready.
