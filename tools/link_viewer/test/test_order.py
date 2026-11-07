"""Checks the translator's event order and wording against a reviewed golden file.

    uv run tools/link_viewer/test/test_order.py           # compare
    uv run tools/link_viewer/test/test_order.py --update  # rewrite the golden file after reviewing a change

Each gen3_*.log (with its .expected) exercises cases checked against
Emerald's battle scripts (data/battle_scripts_1.s) and strings
(src/battle_message.c): weather text before its animation, "can't move" and
confusion text before the status animation, residual damage, partial
trapping, berry and Shed Skin cures, Rest, the player's switch lines by HP,
the opponent's class and name, the end of the battle; stat changes (one
animation for several stats, none for Curse, limits), Intimidate blocked,
ability immunities and heals, berry and ability boosts, Destiny Bond,
Substitute and Wish.
"""

import difflib
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

import lvproto as P  # noqa: E402
from sources import file_kind, lines_of  # noqa: E402
from translator import Translator  # noqa: E402


def run(log: Path) -> list[str]:
    out = []

    def emit(msg, desc):
        kind = P.NAMES.get(msg[1], "?").removeprefix("LVMSG_")
        if kind not in ("MON", "TRAINER"):
            out.append(f"{kind:11} {desc}".replace("\n", "\\n"))

    tr = Translator(emit, log=lambda m: out.append("LOG " + m))
    for line in lines_of(log.read_text(encoding="utf-8"), file_kind(log)):
        tr.feed(line)
    tr.flush()
    return out


def main():
    failed = False
    for log in sorted(HERE.glob("gen3_*.log")):
        golden = log.with_suffix(".expected")
        got = run(log)
        if "--update" in sys.argv:
            golden.write_text("\n".join(got) + "\n", encoding="utf-8")
            print(f"wrote {golden.name} ({len(got)} lines)")
            continue
        want = golden.read_text(encoding="utf-8").splitlines()
        if got == want:
            print(f"{log.name}: ok ({len(got)} events)")
            continue
        failed = True
        sys.stdout.writelines(l + "\n" for l in difflib.unified_diff(want, got, golden.name, "got", lineterm=""))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
