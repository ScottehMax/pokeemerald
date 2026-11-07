"""Fuzzes translator.py with mutated Showdown logs.

    uv run tools/link_viewer/test/fuzz_translator.py [--cases N] [--seed S] [LOG ...]

Each case takes a log from the corpus (magneton records and the sample by
default), mutates it (drops, repeats, swaps and splices lines, mangles
fields, cuts it off), and feeds it the way the bridge does: live, or caught
up silently from a random point then resynced, with random resyncs as if
the GBA had reset. A case fails if the translator raises, or if the stream
it emits breaks a rule in lvcheck.py. Unmutated logs must also translate
without "couldn't handle" complaints.
"""

from __future__ import annotations

import argparse
import collections
import random
import re
import sys
import traceback
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE))

import lvproto as P  # noqa: E402
from lvcheck import check_stream  # noqa: E402
from sources import file_kind, lines_of  # noqa: E402
from translator import Translator  # noqa: E402

DEFAULT_CORPUS = [HERE / "sample_battle.log", *sorted((HERE.parents[3] / "magneton/runs/showdown").glob("*.jsonl"))]

WEIRD_TOKENS = [
    "", " ", "|", "0", "-1", "999999999999", "1e9", "nan", "0 fnt", "100/100", "0/0", "5/0", "-3/100",
    "abc/def", "50/100 tox", "50/100 xyz", "p1a: ", "p2a", "p3a: Ghost", "p1", "p2", "p1a: Pikachu",
    "p2a: Missingno", "Missingno, L50", "Pikachu, L0", "Pikachu, L999", "Pikachu, L50, M, shiny",
    "Lucario, L50, F", "Unown-?", "Deoxys-Speed", "Castform-Snowy", "[from] item: Leftovers",
    "[from] ability: Intimidate", "[of] p1a: Nobody", "[silent]", "[still]", "[miss]", "[msg]",
    "[from]", "move: Substitute", "ability: Drizzle", "item: ", "Hidden Power Fire 70",
    "Not A Move", "éèê", "☃", "x" * 300, "\x00", "\t",
]
COMMANDS = ["move", "switch", "drag", "faint", "turn", "win", "tie", "-damage", "-heal", "-sethp", "-status",
            "-curestatus", "-cureteam", "-boost", "-unboost", "-setboost", "-clearallboost", "-weather",
            "-start", "-end", "-activate", "-item", "-enditem", "-ability", "-sidestart", "-sideend",
            "-fieldactivate", "-singleturn", "-singlemove", "-crit", "-supereffective", "-resisted", "-miss",
            "-fail", "-immune", "-prepare", "-mustrecharge", "-transform", "-formechange", "-hitcount",
            "-message", "cant", "player", "teamsize", "start", "init", "-clearnegativeboost", "-ohko",
            "-notarget", "-nothing", "detailschange", "replace", "-mega", "poke", "teampreview"]


def mutate(lines: list[str], corpus: list[list[str]], rng: random.Random) -> list[str]:
    lines = list(lines)
    for _ in range(rng.choice([1, 1, 2, 3, 5, 10, 30])):
        if not lines:
            break
        op = rng.randrange(10)
        i = rng.randrange(len(lines))
        if op == 0:
            del lines[i]
        elif op == 1:
            lines.insert(i, lines[i])
        elif op == 2 and i + 1 < len(lines):
            lines[i], lines[i + 1] = lines[i + 1], lines[i]
        elif op == 3:
            other = rng.choice(corpus)
            if other:
                j = rng.randrange(len(other))
                lines[i:i] = other[j:j + rng.randint(1, 20)]
        elif op in (4, 5):
            parts = lines[i].split("|")
            if len(parts) > 1:
                k = rng.randrange(1, len(parts))
                parts[k] = rng.choice(WEIRD_TOKENS + parts)
                lines[i] = "|".join(parts)
        elif op == 6:
            parts = lines[i].split("|")
            lines[i] = "|".join(parts[:rng.randint(1, max(1, len(parts) - 1))])
        elif op == 7:
            args = [rng.choice(WEIRD_TOKENS + lines[rng.randrange(len(lines))].split("|")) for _ in range(rng.randint(0, 5))]
            lines.insert(i, "|" + "|".join([rng.choice(COMMANDS)] + args))
        elif op == 8:
            lines = lines[:i]
        elif op == 9:
            # Swap the two sides in one line.
            lines[i] = lines[i].replace("p1", "\0").replace("p2", "p1").replace("\0", "p2")
    return lines


class Case:
    def __init__(self):
        self.stream = bytearray()
        self.complaints: list[str] = []
        self.unexpected: list[str] = []

    def emit(self, msg: bytes, desc: str):
        self.stream += msg

    def log(self, msg: str):
        if "couldn't handle" in msg:
            self.complaints.append(msg)
            # Malformed fields raise these; anything else is a translator bug.
            if not re.search(r": (IndexError|ValueError|KeyError)\(", msg):
                self.unexpected.append(msg)


def run_case(lines: list[str], rng: random.Random, plain: bool) -> tuple[Case, list[str]]:
    """Feeds lines like the bridge does. Returns the case and the problems found."""
    case = Case()
    sessions: list[bytearray] = []
    tr = Translator(lambda m, d: case.emit(m, d), bottom=rng.choice(["", "p1", "p2"]), me="", log=case.log)

    def resync():
        tr.resync()
        if tr.session_slots is None:
            case.emit(P.msg_reset(), "reset")

    if plain:
        resync()
        for line in lines:
            tr.feed(line)
        tr.flush()
    else:
        cut = rng.randrange(len(lines) + 1) if rng.random() < 0.5 else 0
        if cut:
            tr.feed_silent(lines[:cut])
        resync()
        for line in lines[cut:]:
            if rng.random() < 0.003:
                # The GBA reset: the bridge drops what wasn't sent and resyncs.
                sessions.append(case.stream)
                case.stream = bytearray()
                resync()
            if rng.random() < 0.02:
                tr.flush()  # the source went quiet
            tr.feed(line)
        tr.flush()
    sessions.append(case.stream)
    problems = []
    for s in sessions:
        problems += check_stream(bytes(s))
    return case, problems


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="*", type=Path)
    ap.add_argument("--cases", type=int, default=2000)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", type=Path, default=None, help="directory to save failing cases in")
    ap.add_argument("--show", type=int, default=3, help="examples to print per kind of problem")
    args = ap.parse_args()

    paths = args.logs or [p for p in DEFAULT_CORPUS if p.exists()]
    corpus = [lines_of(p.read_text(encoding="utf-8", errors="replace"), file_kind(p)) for p in paths]
    print(f"corpus: {len(corpus)} logs, {sum(map(len, corpus))} lines")

    kinds: collections.Counter = collections.Counter()
    examples: dict[str, list[str]] = collections.defaultdict(list)

    def note(kind: str, detail: str):
        kinds[kind] += 1
        if len(examples[kind]) < args.show:
            examples[kind].append(detail)

    # 1. The corpus itself must be clean.
    for path, lines in zip(paths, corpus):
        try:
            case, problems = run_case(lines, random.Random(0), plain=True)
        except Exception:
            note("crash (unmutated)", f"{path.name}\n{traceback.format_exc()}")
            continue
        for c in case.complaints:
            note("complaint (unmutated)", f"{path.name}: {c}")
        for p in problems:
            note("check (unmutated): " + p.split(": ", 1)[-1].split(" ")[0], f"{path.name}: {p}")

    # 2. Mutated logs.
    for n in range(args.cases):
        rng = random.Random(args.seed * 1_000_003 + n)
        base = rng.randrange(len(corpus))
        lines = mutate(corpus[base], corpus, rng)
        try:
            case, problems = run_case(lines, rng, plain=False)
        except Exception as e:
            tb = traceback.format_exc()
            where = traceback.extract_tb(e.__traceback__)[-1]
            note(f"crash: {type(e).__name__} at {Path(where.filename).name}:{where.lineno}",
                 f"case {n} ({paths[base].name})\n{tb}")
            if args.out:
                args.out.mkdir(parents=True, exist_ok=True)
                (args.out / f"crash_{n}.log").write_text("\n".join(lines), encoding="utf-8")
            continue
        for c in case.unexpected:
            note("unexpected exception: " + c.rsplit(": ", 1)[-1].split("(")[0], f"case {n} ({paths[base].name}): {c[:300]}")
        for p in problems:
            what = p.split(": ", 1)[-1]
            key = "check: " + p.split(" ")[1] + " " + "".join(c for c in what if not c.isdigit())[:50]
            note(key, f"case {n} ({paths[base].name}): {p}")

    # Events the ROM drops (a mon that isn't out, after the battle ended) are
    # expected from garbage input and harmless: count them, but don't fail.
    def benign(kind):
        return "(dropped)" in kind and "unmutated" not in kind

    print(f"{args.cases} mutated cases")
    failures = [k for k in kinds if not benign(k)]
    for kind, count in kinds.most_common():
        if not benign(kind):
            print(f"\n== {count:5}  {kind}")
            for ex in examples[kind]:
                print("   " + ex.strip().replace("\n", "\n   "))
    dropped = sum(c for k, c in kinds.items() if benign(k))
    print(f"\n{len(failures)} kinds of problems; {dropped} events the ROM would drop (harmless)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
