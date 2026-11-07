# Link Battle Viewer

Shows a Pokémon Showdown battle in Pokémon Emerald's own battle scene. The
ROM on this branch boots into a viewer that waits for battle events on the
GBA link port. `lvbridge.py` reads a Showdown battle (a log file, magneton's
battle records, or a live Showdown server) and sends those events to the
viewer through mGBA's GameCube link ("Connect to Dolphin").

```
 Showdown log / server ──> lvbridge.py ──TCP (Dolphin JOY Bus)──> mGBA ──> viewer ROM
                           (translator)                                    (Emerald battle engine,
                                                                             driven event by event)
```

## Running it

1. Build the ROM as usual (`make`). It boots into the viewer's waiting
   screen. Hold **SELECT** while it boots to play the normal game instead.
2. Start the bridge with a battle source (examples below). It listens for
   mGBA on 127.0.0.1, ports 54970 (data) and 49420 (clock).
3. In mGBA (0.10 or later): **File > Connect to Dolphin... > Local computer > Connect**.
   The waiting screen says "Link connected" and the battle starts as soon as
   the bridge has one.

The bridge is plain Python 3.10+ with no required dependencies; `uv run`
works too. For `wss://` servers on Windows, have `certifi` installed
(`uv run --with certifi ...`): Python can otherwise reject a valid
certificate chain from the Windows store.

```sh
# A finished battle: a magneton record, a saved replay page, or raw protocol lines
python tools/link_viewer/lvbridge.py --file ../magneton/runs/showdown/battle-gen3ou-123-sableai.jsonl
python tools/link_viewer/lvbridge.py --file tools/link_viewer/test/sample_battle.log

# Watch magneton's bot: follow the newest record in its runs directory, then each new battle
python tools/link_viewer/lvbridge.py --dir ../magneton/runs/showdown

# A Showdown server: every battle a user plays, or one room
#   (local server: cd ../magneton/ref/pokemon-showdown && node pokemon-showdown start --no-security)
python tools/link_viewer/lvbridge.py --server ws://localhost:8000/showdown/websocket --user sableai
python tools/link_viewer/lvbridge.py --server ws://localhost:8000/showdown/websocket --room battle-gen3ou-42

# No mGBA: print the viewer messages
python tools/link_viewer/lvbridge.py --file tools/link_viewer/test/sample_battle.log --dump
```

Useful options:

- `--side p1|p2` puts that side at the bottom. By default it's the side of
  `--me NAME`, of `--user`, or of the bot named in a magneton file name
  (`battle-...-sableai.jsonl`), else p1.
- `--replay` shows a battle that's already in progress from its start. By
  default the bridge catches up silently and starts the viewer at the
  current turn (for `--dir`, `--follow`, and servers).
- `--bgm MUS_VS_FRONTIER_BRAIN`, `--environment GRASS`, `--text-delay 30`
  change the music, the background and how long messages stay up.

If the GBA is reset, or mGBA reconnects, the viewer reports it and the
bridge restarts the battle from about where the screen was.

## How it works

**Link.** mGBA's Dolphin link is two TCP sockets that mGBA connects to: a
clock socket (big-endian GBA cycle counts; mGBA runs the GBA in lockstep
with them) and a data socket carrying GameCube JOY Bus commands. The viewer
puts the GBA's serial port in JOY Bus mode, and `joybus.py` plays the
GameCube: it reads the viewer's status with command 0x14 and writes the
stream four bytes at a time with command 0x15. A word is written only when
the viewer has read the last one, and the viewer leaves a word unread while
its 2 KB buffer is full, so a slow screen holds the bridge back. The status
also says how much of the stream the viewer has used, and the bridge keeps
only a few hundred bytes ahead of that.

**Protocol.** `include/constants/link_viewer.h` defines the messages
(`[length][type][payload]`) and the handshake; `lvproto.py` reads the
defines from that header, the ids from the game's constants headers and the
character set from `charmap.txt`, so the two sides can't drift apart. Move,
item, ability and species names are sent as ids and printed from the ROM's
own tables.

**Translation.** `translator.py` follows the Showdown protocol and turns
each event into Gen 3 battle steps and text: "Foe ZAPDOS used THUNDERBOLT!",
the move animation, the hit and its sound, the HP bar, then "A critical
hit!" and "It's super effective!". It holds each `|move|` line until the
next one arrives, to leave out the animation of a move that misses or hits
Protect. It keeps the whole battle state, so it can start the viewer in the
middle of a battle: the active Pokémon go in party slot 0 (which the battle
intro sends out) and unrevealed party members are placeholders that only
show as Poké Balls.

**ROM.** `src/link_viewer.c` has the serial receiver, the waiting screen and
the event player. A battle is started as a secret base trainer battle (the
one battle type whose opponent name and picture come from RAM), with both
parties built from the stream. After the intro, `gBattleMainFunc` becomes
`LinkViewer_BattleMainFunc` instead of the turn loop: it takes one message
at a time and drives the normal battle controllers with it (move and status
animations, HP bars, switching, fainting), so nothing is simulated on the
GBA. The rest of the engine changes are small hooks:

- `src/main.c`: boot into the viewer.
- `src/battle_main.c`: the viewer's background; hand over after the intro.
- `src/battle_util2.c`: don't build a secret base party from the save.
- `src/battle_bg.c`: don't read `gTrainers[TRAINER_SECRET_BASE]` (past the end of the table).
- `src/battle_message.c`: battle text scrolls by itself, as in link battles.

## Testing

`test/mgba_harness.c` runs the ROM headlessly with mGBA's Dolphin link
attached (what "Connect to Dolphin" does), saves screenshots, and can reset
the GBA partway to test resyncing. Build it on Linux/WSL against an mGBA
checkout with `MGBA_SRC=path/to/mgba test/build_harness.sh`, then run the
bridge and `mgba_harness ROM FRAMES SHOT_EVERY OUT_DIR [RESET_AT_FRAME]`.
The bridge's hidden `--clock-rate N` lets the headless emulator run N times
faster than real time.

### Fuzzing

The viewer checks everything it receives (ids against its tables, control
codes in text and names, unknown placeholders, music ids, the animations it
can play, HP and wait limits), so a bad stream can't crash or stall it. Two
fuzzers keep it that way:

- `test/fuzz_translator.py` mutates the corpus of Showdown logs (magneton's
  records and the sample) by dropping, repeating, swapping and splicing lines
  and mangling fields, then feeds them the way the bridge does (live, caught
  up from a random point, with random resyncs). It fails on an exception and
  on any message `test/lvcheck.py` rejects: malformed, an id outside the ROM's
  tables, a START without a lead, a switch to an empty slot. Events the ROM
  would just drop are counted but allowed.

  ```sh
  uv run tools/link_viewer/test/fuzz_translator.py --cases 20000
  ```

- `test/fuzz_rom.py` runs the harness and sends the ROM battles made of
  fuzzed messages (ids out of range, short or oversized payloads, control
  codes, garbage bytes), each followed by padding and a RESET. It fails if
  mGBA reports an emulation error after boot (bad memory access, invalid
  jump or DMA), if the viewer stops taking the stream, or if the GBA restarts.
  Failing cases are saved with a screenshot and can be replayed.

  ```sh
  python3 tools/link_viewer/test/fuzz_rom.py --cases 300 --seed 2 --out fuzz_out
  python3 tools/link_viewer/test/fuzz_rom.py --replay fuzz_out/case_17.bin
  ```

## Limitations

- Singles only.
- Trainer names are cut to 7 characters and nicknames to 10.
- Showdown shows the opponent's HP as a percentage, so their HP bar counts
  out of 100 (your own side shows real HP when the log has it).
- A live battle can outrun the viewer, whose animations are slower than
  Showdown's; the viewer then falls behind and doesn't skip ahead.
- Less common effects get their HP or status change but no message.
