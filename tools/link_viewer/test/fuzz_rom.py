"""Fuzzes the viewer ROM with malformed message streams, headlessly.

    python3 tools/link_viewer/test/fuzz_rom.py --cases 200 [--seed S] [--mode structured|bytes|mixed]
    python3 tools/link_viewer/test/fuzz_rom.py --replay OUT/case_17.bin

Runs mgba_harness (see build_harness.sh) and plays the GameCube side of the
link itself. Each case is a battle: teams and a START with fuzzed fields,
then a run of fuzzed events (ids out of range, short or oversized payloads,
control codes in text, garbage bytes...), then padding and a RESET, which
must bring the viewer back to its waiting screen for the next case.

A case fails if:
- mGBA reports an emulation error after boot (bad memory access, a jump to
  an invalid address, an invalid DMA...),
- the viewer stops taking the stream for a while (stuck or crashed),
- the GBA stops answering or restarts on its own.
A failing case is saved (bytes + description + screenshot) and the
emulator restarted. Linux/WSL only (like the harness).
"""

from __future__ import annotations

import argparse
import os
import queue
import random
import shutil
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import lvproto as P  # noqa: E402
from joybus import GBA_HZ, JoyBusHost  # noqa: E402
from lvcheck import NUM_SPECIES, MOVES_COUNT, NUM_ANIMS, ITEMS_COUNT, ABILITIES_COUNT  # noqa: E402

C = P.C
T = {k.removeprefix("LVMSG_"): v for k, v in C.items() if k.startswith("LVMSG_")}
BOOT_FRAMES = 30          # mGBA reports BIOS/flash noise while booting
STALL_SECONDS = 45        # emulated seconds without progress = stuck
SONG_IDS = sorted(v for k, v in P.SONGS.items() if k.startswith(("MUS_", "SE_")))
FRAME_CYCLES = 280896


def edge8(rng):
    return rng.choice([0, 1, 2, 3, 4, 5, 6, 7, 8, 0x0F, 0x10, 0x7F, 0x80, 0xFE, 0xFF, rng.randrange(256)])


def edge16(rng, valid=None):
    picks = [0, 1, 0x7FFF, 0x8000, 0xFFFF, rng.randrange(0x10000)]
    if valid:
        picks += [valid - 1, valid, valid + 1] + [rng.randrange(1, valid)] * 6
    return rng.choice(picks) & 0xFFFF


def fuzz_text(rng, max_len=254) -> bytes:
    """Text with control codes, placeholders (valid and not) and no EOS sometimes."""
    out = bytearray()
    for _ in range(rng.randint(0, 40)):
        r = rng.random()
        if r < 0.5:
            out += P.encode_text(rng.choice(["Foe ", "PIKACHU", " used ", "!", " ", "It's super effective!",
                                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "WWWWWWWWWWWWWWWWWWWWWWWWW"]))
        elif r < 0.62:
            code = rng.choice([C["LV_PH_MOVE"], C["LV_PH_ITEM"], C["LV_PH_ABILITY"], C["LV_PH_SPECIES"], C["LV_PH_TRAINER"], edge8(rng)])
            out += bytes([P.PLACEHOLDER, code]) + struct.pack("<H", rng.choice(
                [edge16(rng, MOVES_COUNT), edge16(rng, ITEMS_COUNT), edge16(rng, ABILITIES_COUNT), edge16(rng, NUM_SPECIES)]))
        elif r < 0.72:
            out += bytes([0xFC, edge8(rng), edge8(rng), edge8(rng)])  # EXT_CTRL_CODE_* with arguments
        elif r < 0.82:
            out += bytes([rng.choice([0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFD, 0xFE])])
        elif r < 0.9:
            out += bytes([P.PLACEHOLDER])  # a placeholder cut off at the end
        else:
            out += bytes(rng.randrange(256) for _ in range(rng.randint(1, 8)))
    out = out[:max_len]
    if rng.random() < 0.85:
        out.append(P.EOS)
    return bytes(out)


def msg(kind_or_type, payload: bytes) -> bytes:
    t = T[kind_or_type] if isinstance(kind_or_type, str) else kind_or_type
    body = (bytes([t]) + payload)[:255]
    return bytes([len(body)]) + body


def shorten(rng, payload: bytes) -> bytes:
    """Sometimes cut a payload short (the ROM pads with EOS) or grow it."""
    r = rng.random()
    if r < 0.08:
        return payload[:rng.randrange(len(payload) + 1)]
    if r < 0.12:
        return payload + bytes(rng.randrange(256) for _ in range(rng.randint(1, 40)))
    return payload


def gen_mon(rng, side, slot, sane):
    if sane:
        species, level, hp, max_hp = rng.randrange(1, 412), rng.randint(1, 100), rng.randint(0, 300), 300
        status, flags = rng.randrange(7), rng.randrange(8)
        nick = rng.choice([b"", P.encode_text("NICK")])
    else:
        species, level, hp, max_hp = edge16(rng, NUM_SPECIES), edge8(rng), edge16(rng), edge16(rng)
        status, flags = edge8(rng), edge8(rng)
        nick = fuzz_text(rng, 20)[:rng.randint(0, 20)]
    payload = struct.pack("<BBHBHHBB", side, slot, species, level, hp & 0xFFFF, max_hp & 0xFFFF, status, flags)
    return msg("MON", payload + nick + bytes([P.EOS]) if rng.random() < 0.9 else payload + nick)


def gen_event(rng) -> bytes:
    kinds = ["TEXT", "MOVE", "HIT", "HP", "SWITCH_OUT", "SWITCH_IN", "FAINT", "STATUS", "STAT_CHANGE",
             "ANIM", "WAIT", "WEATHER", "SE", "STATUS_ANIM", "CRY", "MON", "TRAINER", "END"]
    weights = [10, 10, 6, 6, 3, 4, 3, 4, 4, 5, 1, 2, 2, 2, 2, 3, 1, 0.3]
    kind = rng.choices(kinds, weights)[0]
    side = rng.choice([0, 1, 0, 1, edge8(rng)])
    if kind == "TEXT":
        return msg(kind, fuzz_text(rng))
    if kind == "MOVE":
        p = struct.pack("<BBHB", side, rng.choice([0, 1, edge8(rng)]), edge16(rng, MOVES_COUNT), edge8(rng))
    elif kind == "HIT":
        p = bytes([side, edge8(rng)])
    elif kind == "HP":
        p = struct.pack("<BH", side, edge16(rng, 300))
    elif kind in ("SWITCH_OUT", "FAINT", "CRY"):
        p = bytes([side])
    elif kind == "SWITCH_IN":
        p = bytes([side, rng.choice([0, 1, 2, 3, 4, 5, edge8(rng)])])
    elif kind == "STATUS":
        p = bytes([side, edge8(rng), edge8(rng)])
    elif kind == "STAT_CHANGE":
        p = bytes([side, edge8(rng), edge8(rng)])
    elif kind == "ANIM":
        p = struct.pack("<BBH", side, rng.choice([edge8(rng), rng.randrange(NUM_ANIMS)]), edge16(rng))
    elif kind == "WAIT":
        p = struct.pack("<H", rng.randrange(60))
    elif kind == "WEATHER":
        p = bytes([edge8(rng), edge8(rng)])
    elif kind == "SE":
        p = struct.pack("<H", rng.choice([rng.choice(SONG_IDS), edge16(rng)]))
    elif kind == "STATUS_ANIM":
        p = bytes([side, edge8(rng)])
    elif kind == "MON":
        return gen_mon(rng, side, rng.choice([0, 1, 2, 3, 4, 5, edge8(rng)]), sane=rng.random() < 0.5)
    elif kind == "TRAINER":
        p = bytes([side, edge8(rng), edge8(rng)]) + fuzz_text(rng, 12)
    else:  # END (a real one: the battle ends early)
        p = bytes([rng.choice([0, 1, 2, 0xFF, edge8(rng)])])
    return msg(kind, shorten(rng, p))


def gen_case(rng, mode: str, events: int) -> tuple[bytes, list[str]]:
    """A whole case: setup, events, then padding + RESET back to the waiting screen."""
    out = bytearray()
    desc = []

    def add(m, d=None):
        out.extend(m)
        if d is None:
            try:
                d = P.describe(m)
            except Exception:
                d = "?"
            d = f"{d}   [{m.hex()}]"
        desc.append(d)

    add(P.msg_reset())
    for side in (0, 1):
        name = fuzz_text(rng, 12) if rng.random() < 0.4 else P.encode_text(rng.choice(["RED", "BLUE", "ABCDEFGHIJ"])) + b"\xff"
        add(msg("TRAINER", bytes([side if rng.random() < 0.9 else edge8(rng), edge8(rng), edge8(rng)]) + name))
        for slot in range(rng.choice([1, 2, 6, 6, 6])):
            add(gen_mon(rng, side, slot, sane=slot == 0 and rng.random() < 0.8 or rng.random() < 0.5))
    bgm = rng.choice([0, 0, rng.choice(SONG_IDS), edge16(rng)])
    add(msg("START", shorten(rng, struct.pack("<HBB", bgm, edge8(rng), rng.randrange(8)))))
    for _ in range(events):
        if mode == "bytes" or (mode == "mixed" and rng.random() < 0.1):
            junk = bytes(rng.randrange(256) for _ in range(rng.randint(1, 64)))
            add(junk, f"junk {junk.hex()}")
        else:
            add(gen_event(rng))
    # Whatever framing the junk left, 256 zero bytes end any message and then
    # read as padding; the RESET then ends the battle.
    add(bytes(256), "padding")
    add(P.msg_reset())
    return bytes(out), desc


class Harness:
    def __init__(self, harness: Path, rom: Path, out: Path):
        self.out = out
        self.lines: queue.Queue = queue.Queue()
        self.proc = subprocess.Popen([str(harness), str(rom), "0", "-30", str(out)],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, errors="replace")
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for line in self.proc.stderr:
            self.lines.put(line.rstrip())

    def errors(self) -> list[str]:
        out = []
        while True:
            try:
                line = self.lines.get_nowait()
            except queue.Empty:
                return out
            if line.startswith("[mgba] frame "):
                frame = int(line.split()[2])
                if frame > BOOT_FRAMES:
                    out.append(line)

    def alive(self) -> bool:
        return self.proc.poll() is None

    def kill(self):
        self.proc.kill()
        self.proc.wait()


class Rig:
    """The harness plus our end of the link."""

    def __init__(self, args):
        self.args = args
        self.joy: JoyBusHost | None = None
        self.harness: Harness | None = None
        self.logs: list[str] = []
        self.previous = b""

    def log(self, m):
        self.logs.append(m)
        if self.args.verbose:
            print(m, flush=True)

    def start(self):
        if self.joy is None:
            self.joy = JoyBusHost(log=self.log, clock_rate=self.args.rate)
            self.joy.start()
        self.args.out.mkdir(parents=True, exist_ok=True)
        self.harness = Harness(self.args.harness, self.args.rom, self.args.out)
        deadline = time.time() + 60
        while not self.joy.take_sync_request():
            if time.time() > deadline or not self.harness.alive():
                raise RuntimeError("the viewer never came up: " + "; ".join(self.logs[-5:]))
            time.sleep(0.01)
        self.joy.resume()
        self.sent = 0           # stream bytes queued since the viewer's reset
        self.harness.errors()   # boot noise

    def stop(self):
        if self.harness:
            self.harness.kill()
            self.harness = None
        # Wait for our end to notice the disconnect.
        deadline = time.time() + 5
        while self.joy.connected and time.time() < deadline:
            time.sleep(0.01)
        self.joy.take_sync_request()

    def run(self, data: bytes) -> str | None:
        """Sends one case and waits until the viewer has taken all of it. Returns why it failed, or None."""
        joy = self.joy
        data += bytes(-len(data) % 4)  # the link sends whole words
        joy.send(data)
        self.sent += len(data)
        target = self.sent & 0xFFFF
        last_progress, last_consumed = joy.cycles, joy.consumed
        while True:
            errors = self.harness.errors()
            if errors:
                time.sleep(0.3)  # let the rest of the burst arrive
                return "emulation error:\n" + "\n".join((errors + self.harness.errors())[:20])
            if not self.harness.alive():
                return "the emulator exited"
            if joy.sync_needed.is_set():
                return "the GBA restarted (or stopped answering): " + "; ".join(self.logs[-3:])
            if joy.consumed == target and joy.backlog() == 0:
                return None
            if joy.consumed != last_consumed:
                last_progress, last_consumed = joy.cycles, joy.consumed
            elif joy.cycles - last_progress > STALL_SECONDS * GBA_HZ:
                behind = (target - joy.consumed) & 0xFFFF
                return f"stuck: the viewer stopped taking the stream with {behind} bytes to go"
            time.sleep(0.005)


def save_failure(args, n, data, desc, why, rig):
    stem = args.out / f"case_{n}"
    stem.with_suffix(".bin").write_bytes(data)
    if rig.previous:  # the failure may come from the end of the case before
        (args.out / f"case_{n}_with_previous.bin").write_bytes(rig.previous + data)
    stem.with_suffix(".txt").write_text(f"{why}\n\n" + "\n".join(desc) + "\n", encoding="utf-8")
    now = args.out / "now.png"
    if now.exists():
        shutil.copy(now, stem.with_suffix(".png"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cases", type=int, default=100)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--events", type=int, default=40, help="fuzzed events per case")
    ap.add_argument("--mode", choices=["structured", "bytes", "mixed"], default="mixed")
    ap.add_argument("--rom", type=Path, default=HERE.parents[2] / "link_viewer.gba")
    ap.add_argument("--harness", type=Path, default=Path(os.environ.get("MGBA_BUILD", Path.home() / "mgba-build")) / "mgba_harness")
    ap.add_argument("--out", type=Path, default=Path("fuzz_out"))
    ap.add_argument("--rate", type=float, default=8.0, help="emulated seconds per real second")
    ap.add_argument("--replay", type=Path, help="send one saved case (.bin) instead of fuzzing")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    rig = Rig(args)
    rig.start()
    failures = 0
    t0 = time.time()
    try:
        if args.replay:
            why = rig.run(args.replay.read_bytes())
            if why is None:  # it may still be animating the last events
                end = rig.joy.cycles + 10 * GBA_HZ
                while why is None and rig.joy.cycles < end:
                    errors = rig.harness.errors()
                    why = "emulation error:\n" + "\n".join(errors[:20]) if errors else None
                    time.sleep(0.01)
            print(why or "ok")
            return
        for n in range(args.cases):
            rng = random.Random(args.seed * 1_000_003 + n)
            data, desc = gen_case(rng, args.mode, args.events)
            why = rig.run(data)
            status = "ok" if why is None else "FAIL " + why.splitlines()[0]
            print(f"case {n}: {len(data)} bytes, {time.time() - t0:.0f}s: {status}", flush=True)
            if why:
                failures += 1
                print("    " + why.replace("\n", "\n    "), flush=True)
                save_failure(args, n, data, desc, why, rig)
                rig.stop()
                rig.start()
                data = b""
            rig.previous = data
    finally:
        rig.stop()
    print(f"{failures} of {args.cases} cases failed ({time.time() - t0:.0f}s)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
