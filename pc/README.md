# Native PC Port

This port runs the decompiled game code directly. It uses a 32-bit core to
preserve the GBA data and script pointer ABI and an SDL2 frontend for video,
audio, keyboard input, and process management.

## Prerequisites

- The normal pokeemerald tools and generated `build/assets` directory. Run a
  modern ROM build once if those assets do not exist: `make modern -j`.
- Zig 0.13 or newer.
- `uv`, GNU binutils, `curl`, and `tar`.
- SDL2 at runtime for a native Linux build. The Windows build downloads a
  pinned official SDL2 MinGW release and verifies its SHA-256 checksum.

## Linux

```sh
make pc -j
./build/pc/pokeemerald-pc
```

## Cross-Compile Windows

Run this on Linux:

```sh
make pc-windows -j
```

The distributable directory is `build/pc/windows`. Keep these files together:

- `pokeemerald-pc.exe` (launch this)
- `pokeemerald-core.exe`
- `SDL2.dll`
- `SDL2-LICENSE.txt`

The output targets 64-bit Windows 10 or newer. The launcher is 64-bit and the
game core is 32-bit by design. The core uses a fixed image base above the GBA
memory ranges and claims EWRAM, IWRAM, registers, palettes, VRAM, and OAM before
opening any shared mappings, so those regions remain available at their
original addresses.

## Controls

| Key | GBA input |
| --- | --- |
| Arrow keys | D-pad |
| X | A |
| Z | B |
| Enter | Start |
| Backspace | Select |
| A | L |
| S | R |
| Escape | Quit |

The save file is `pokeemerald.sav` beside the launcher unless `--save PATH` is
provided.

## Filesystem Storage

The Pokémon Storage System has a PC-only `STORAGE` entry. It reads individual
boxed Pokémon from `./storage/` relative to the directory where the game is
started. Files use the standard 80-byte encrypted Gen III boxed Pokémon data
and the `.ek3` extension.

Press `R` from the STORAGE screen to open the normal box interface in
storage-transfer mode. Each transfer updates the save file before removing the
source copy, so an I/O error cannot silently discard a Pokémon.
