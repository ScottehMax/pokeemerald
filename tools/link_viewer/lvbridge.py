"""Link Battle Viewer bridge: shows a Pokémon Showdown battle on the viewer
ROM running in mGBA, over mGBA's GameCube link (JOY Bus).

    python tools/link_viewer/lvbridge.py --file battle.jsonl
    python tools/link_viewer/lvbridge.py --dir ../magneton/runs/showdown
    python tools/link_viewer/lvbridge.py --server ws://localhost:8000/showdown/websocket --user sableai
    python tools/link_viewer/lvbridge.py --room battle-gen3ou-123456 --dump

Then in mGBA, with the viewer ROM running: File > Connect to Dolphin... >
"Local computer" > Connect. See tools/link_viewer/README.md.
"""

from __future__ import annotations

import argparse
import queue
import sys
import time
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import lvproto as P  # noqa: E402
from joybus import JoyBusHost  # noqa: E402
from sources import DirectorySource, FileSource, ShowdownSource  # noqa: E402
from translator import Translator  # noqa: E402

LOW_WATER = 128   # keep about this many bytes queued for the GBA
FLUSH_AFTER = 0.4  # seconds without input before a held |move| line is shown


def parse_id(value: str, table: dict[str, int], prefix: str) -> int:
    if value.isdigit():
        return int(value)
    key = value.upper()
    if not key.startswith(prefix):
        key = prefix + key
    if key not in table:
        raise argparse.ArgumentTypeError(f"unknown {prefix}* constant {value!r}")
    return table[key]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_argument_group("battle source (pick one)")
    src.add_argument("--file", type=Path, help="a battle log: magneton .jsonl record, Showdown replay .html, or protocol lines")
    src.add_argument("--dir", type=Path, help="follow the newest log in a directory, then each new one (e.g. magneton's runs/showdown)")
    src.add_argument("--server", default="", help="Showdown websocket URL (default: the main server)")
    src.add_argument("--room", default="", help="spectate this battle room, e.g. battle-gen3ou-123456")
    src.add_argument("--user", default="", help="spectate every battle this user plays")

    p.add_argument("--pattern", default="*.jsonl", help="with --dir: which files (default *.jsonl)")
    p.add_argument("--follow", action="store_true", help="with --file: keep reading as the file grows")
    p.add_argument("--replay", action="store_true", help="show a battle in progress from its start instead of catching up to the present")
    p.add_argument("--side", choices=["p1", "p2"], default="", help="the side shown at the bottom (default: --me's or --user's side, else p1)")
    p.add_argument("--me", default="", help="player name whose side goes at the bottom (default: from a magneton file name)")
    p.add_argument("--bgm", default="0", help="battle music: a MUS_* name or song id (default MUS_VS_TRAINER)")
    p.add_argument("--environment", default="BUILDING", help="battle background: a BATTLE_ENVIRONMENT_* name (default BUILDING)")
    p.add_argument("--text-delay", type=int, default=0, help="frames each message stays up after printing (default 40)")

    p.add_argument("--dump", action="store_true", help="print the viewer messages instead of sending them to mGBA")
    p.add_argument("--host", default="127.0.0.1", help="address to accept mGBA on (default 127.0.0.1)")
    p.add_argument("--data-port", type=int, default=54970)
    p.add_argument("--clock-port", type=int, default=49420)
    p.add_argument("--quiet", action="store_true", help="don't print each message as it is sent")
    p.add_argument("--clock-rate", type=float, default=1.0, help=argparse.SUPPRESS)  # testing with an unthrottled emulator
    args = p.parse_args(argv)

    if sum(bool(x) for x in (args.file, args.dir, args.room or args.user or args.server)) != 1:
        p.error("give one source: --file, --dir, or --server/--room/--user")
    bgm = parse_id(args.bgm, P.SONGS, "MUS_") if args.bgm != "0" else 0
    environment = parse_id(args.environment, P.ENVIRONMENTS, "BATTLE_ENVIRONMENT_")

    def log(msg):
        print(msg, flush=True)

    events: queue.Queue = queue.Queue()
    if args.file:
        lines_so_far = args.file.exists()
        source = FileSource(events, args.file, follow=args.follow, catch_up=args.follow and lines_so_far and not args.replay,
                            me=args.me, log=log)
    elif args.dir:
        source = DirectorySource(events, args.dir, args.pattern, replay_last=args.replay, log=log)
    else:
        if not args.room and not args.user:
            p.error("--server needs --room or --user")
        source = ShowdownSource(events, args.server, room=args.room, user=args.user, catch_up=not args.replay, log=log)

    joy = None if args.dump else JoyBusHost(args.host, args.data_port, args.clock_port, log=log, clock_rate=args.clock_rate)

    def emit(msg: bytes, desc: str):
        if joy is not None:
            joy.send(msg)
        if args.dump or not args.quiet:
            kind = P.NAMES.get(msg[1], "?").removeprefix("LVMSG_")
            log(f"  {kind:11} {desc}")

    translator = Translator(emit, bottom=args.side, me=args.me or args.user, bgm=bgm,
                            environment=environment, text_delay=args.text_delay, log=log)

    source.start()
    if joy is not None:
        joy.start()

    last_input = time.time()
    eof = False
    todo: deque = deque()  # source events split into single lines, taken one at a time

    def take_events(block: bool):
        try:
            ev = events.get(timeout=0.05) if block else events.get_nowait()
        except queue.Empty:
            return False
        while True:
            if ev[0] == "lines" and not ev[2]:
                todo.extend(("line", l) for l in ev[1])
            else:
                todo.append(ev)
            try:
                ev = events.get_nowait()
            except queue.Empty:
                return True

    try:
        while True:
            if joy is not None and joy.take_sync_request():
                log("[bridge] (re)starting the viewer")
                translator.resync()
                if translator.session_slots is None:
                    emit(P.msg_reset(), "reset")
                joy.resume()

            take_events(block=not todo)
            if not todo:
                if translator.held_move is not None and time.time() - last_input > FLUSH_AFTER:
                    translator.flush()
                if eof and joy is None:
                    break
                continue

            # Translate only as fast as the GBA shows the battle, so the
            # translator's state stays close to what is on screen (a resync
            # restarts the viewer from that state).
            if joy is not None and not (joy.viewer_ready and not joy.paused and joy.backlog() < LOW_WATER):
                time.sleep(0.01)
                continue

            last_input = time.time()
            ev = todo.popleft()
            kind = ev[0]
            if kind == "line":
                translator.feed(ev[1])
            elif kind == "battle":
                translator.flush()
                translator.reset_battle()
                if ev[1] and not args.me and not args.user:
                    translator.me = P.to_id(ev[1])
            elif kind == "lines":  # a backlog to catch up on
                translator.feed_silent(ev[1])
                log(f"[bridge] caught up on {len(ev[1])} lines")
                translator.resync()
            elif kind == "eof":
                translator.flush()
                eof = True
                if joy is not None:
                    log("[bridge] end of the log (Ctrl+C to quit)")
    except KeyboardInterrupt:
        pass
    if joy is not None:
        log(f"[bridge] sent {joy.words_sent * 4} bytes")


if __name__ == "__main__":
    main()
