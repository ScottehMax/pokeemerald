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
- `pokeemerald-core.pdb` (crash-report symbols)
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

## Crash Reports

An abnormal core exit creates a timestamped text report and a capture of the
last completed frame in `crash-reports/` beside the active save file. Reports
include the fault type and address, CPU registers, a native stack trace, map and
battle state, and the main, task, sprite, and script callbacks active near the
failure. They do not include save contents, player names, connect codes, or
peer addresses.

Linux reports resolve functions from the core's ELF symbols. Windows reports
use `pokeemerald-core.pdb`, so keep that file beside `pokeemerald-core.exe` when
sharing a build. Every report also records a hash of the exact core executable
so raw addresses can be matched to the correct binary later.

## Filesystem Storage

The Pokémon Storage System has a PC-only `STORAGE` entry. It reads individual
boxed Pokémon from `./storage/` relative to the directory where the game is
started. Files use the standard 80-byte encrypted Gen III boxed Pokémon data
and the `.ek3` extension.

Press `R` from the STORAGE screen to open the normal box interface in
storage-transfer mode. Each transfer updates the save file before removing the
source copy, so an I/O error cannot silently discard a Pokémon.

## Link Play

PC link play supports two-player wired Cable Club activities. Both players use
the Direct Corner attendant and enter the same connect code when prompted. The
code is case-insensitive and can contain up to eight characters.

For two instances on the same computer, start the rendezvous server before the
games:

```sh
./build/pc/pokeemerald-link-server
```

On Windows, use `pokeemerald-link-server.exe` from the cross-build directory.

The clients use `127.0.0.1:8765` by default. Each instance must use a different
save path.

For internet play, run the server on a publicly reachable host and allow UDP
port 8765 through that host's firewall. Point both clients at it before starting
the game:

```sh
POKEEMERALD_LINK_SERVER=example.com:8765 ./build/pc/pokeemerald-pc
```

The server only matches codes and exchanges the clients' observed UDP
endpoints. Game traffic then travels directly between peers using UDP hole
punching, so the players do not need to configure port forwarding. The server
can use a different address or port with `--bind ADDRESS` and `--port PORT`.
Symmetric NATs that assign a different public port for each destination may
still prevent direct peer-to-peer connectivity.
