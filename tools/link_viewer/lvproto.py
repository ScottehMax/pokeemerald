"""Link Battle Viewer protocol: message encoding and game data.

Everything the ROM agrees on is read from the pokeemerald tree, so the
bridge and the ROM can't drift apart:

- include/constants/link_viewer.h   message types and flags
- include/constants/{species,moves,items,abilities}.h   ids
- include/constants/{battle_anim,songs,battle}.h   animation, music, environment ids
- charmap.txt   the GBA character set
"""

from __future__ import annotations

import re
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

_DEFINE = re.compile(r"^\s*#define\s+(\w+)\s+(.+?)\s*(?://.*)?$")


def to_id(name: str) -> str:
    """Showdown's toID: lowercase letters and digits only."""
    return "".join(c for c in name.lower() if c.isalnum())


def read_defines(path: Path, prefix: str = "") -> dict[str, int]:
    """Integer #defines in a header (values that are plain numbers or that
    refer to earlier defines with simple arithmetic)."""
    out: dict[str, int] = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    # enum { A, B = 5, C, ... };
    for body in re.findall(r"\benum\s*\w*\s*\{(.*?)\}", text, re.S):
        body = re.sub(r"//[^\n]*|/\*.*?\*/", "", body, flags=re.S)
        value = -1
        for entry in body.split(","):
            entry = entry.strip()
            if not entry:
                continue
            name, _, expr = entry.partition("=")
            name = name.strip()
            if expr.strip():
                try:
                    value = eval(expr.strip(), {"__builtins__": {}}, dict(out))  # noqa: S307
                except Exception:
                    continue
            else:
                value += 1
            if name.startswith(prefix) and re.fullmatch(r"\w+", name):
                out[name] = value
    for line in text.splitlines():
        m = _DEFINE.match(line)
        if not m or not m[1].startswith(prefix):
            continue
        expr = m[2].split("//")[0].strip()
        if not re.fullmatch(r"[\w\s()+\-*<|]+", expr):
            continue
        try:
            value = eval(expr, {"__builtins__": {}}, dict(out))  # noqa: S307 - our own headers
        except Exception:
            continue
        if isinstance(value, int):
            out[m[1]] = value
    return out


C = read_defines(REPO / "include/constants/link_viewer.h")
ANIMS = read_defines(REPO / "include/constants/battle_anim.h", "B_ANIM_")
SONGS = read_defines(REPO / "include/constants/songs.h")
ENVIRONMENTS = read_defines(REPO / "include/constants/battle.h", "BATTLE_ENVIRONMENT_")


class IdTable:
    """Showdown name -> pokeemerald constant, matched by toID of both."""

    def __init__(self, header: str, prefix: str, aliases: dict[str, str] | None = None):
        self.by_id: dict[str, int] = {}
        for name, value in read_defines(REPO / header, prefix).items():
            key = to_id(name[len(prefix):])
            self.by_id.setdefault(key, value)
        for alias, target in (aliases or {}).items():
            if target in self.by_id:
                self.by_id[alias] = self.by_id[target]

    def get(self, name: str) -> int:
        key = to_id(name)
        if key in self.by_id:
            return self.by_id[key]
        # Formes ("Deoxys-Attack", "Castform-Sunny", "Unown-B") use the base species.
        if "-" in name:
            return self.get(name.split("-")[0])
        return 0


SPECIES = IdTable("include/constants/species.h", "SPECIES_")
MOVES = IdTable("include/constants/moves.h", "MOVE_", {
    "feintattack": "faintattack",
    "highjumpkick": "hijumpkick",
    "smellingsalts": "smellingsalt",
    "visegrip": "vicegrip",
})
ITEMS = IdTable("include/constants/items.h", "ITEM_")
ABILITIES = IdTable("include/constants/abilities.h", "ABILITY_")


def move_id(name: str) -> int:
    if to_id(name).startswith("hiddenpower"):
        return MOVES.get("hiddenpower")
    return MOVES.get(name)


# --- text -------------------------------------------------------------------

EOS = 0xFF
NEWLINE = 0xFE
PLACEHOLDER = 0xFD


def _load_charmap() -> dict[str, int]:
    table: dict[str, int] = {}
    pat = re.compile(r"^'(.+)'\s*=\s*([0-9A-Fa-f]{2})\s*$")
    for line in (REPO / "charmap.txt").read_text(encoding="utf-8").splitlines():
        m = pat.match(line.strip())
        if not m:
            continue
        ch = m[1].encode().decode("unicode_escape") if m[1].startswith("\\") else m[1]
        value = int(m[2], 16)
        if len(ch) == 1 and value < 0xF7:  # 0xF7+ are control codes ($ is EOS)
            table.setdefault(ch, value)
    # Typographic fallbacks
    for src, dst in {"’": "'", "‘": "'", "“": '"', "”": '"', "—": "-", "–": "-"}.items():
        if src not in table and dst in table:
            table[src] = table[dst]
    return table


CHARMAP = _load_charmap()


def encode_text(text: str) -> bytes:
    """Plain text in the GBA charset (no terminator). \\n is a line break."""
    out = bytearray()
    for ch in text:
        if ch == "\n":
            out.append(NEWLINE)
        elif ch in CHARMAP:
            out.append(CHARMAP[ch])
        elif ch == '"' and "“" in CHARMAP:
            out.append(CHARMAP["“"])
        else:
            out.append(CHARMAP.get("?", 0xAC))
    return bytes(out)


class Text:
    """A battle message built from plain text and in-ROM names."""

    def __init__(self, *parts):
        self.data = bytearray()
        self.plain = []
        for p in parts:
            self.add(p)

    def add(self, part) -> "Text":
        if isinstance(part, Text):
            self.data += part.data
            self.plain.append(part.text())
        elif isinstance(part, str):
            self.data += encode_text(part)
            self.plain.append(part)
        else:
            raise TypeError(part)
        return self

    def __add__(self, other) -> "Text":
        return Text(self, other)

    def __radd__(self, other) -> "Text":
        return Text(other, self)

    def text(self) -> str:
        return "".join(self.plain)

    @classmethod
    def _ph(cls, code: int, payload: bytes, plain: str) -> "Text":
        t = cls()
        t.data += bytes([PLACEHOLDER, code]) + payload
        t.plain.append(plain)
        return t

    @classmethod
    def move(cls, name: str) -> "Text":
        mid = move_id(name)
        if not mid:
            return cls(name.upper())
        return cls._ph(C["LV_PH_MOVE"], struct.pack("<H", mid), name.upper())

    @classmethod
    def item(cls, name: str) -> "Text":
        iid = ITEMS.get(name)
        if not iid:
            return cls(name.upper())
        return cls._ph(C["LV_PH_ITEM"], struct.pack("<H", iid), name.upper())

    @classmethod
    def ability(cls, name: str) -> "Text":
        aid = ABILITIES.get(name)
        if not aid:
            return cls(name.upper())
        return cls._ph(C["LV_PH_ABILITY"], bytes([aid]), name.upper())

    @classmethod
    def species(cls, name: str) -> "Text":
        sid = SPECIES.get(name)
        if not sid:
            return cls(name.upper())
        return cls._ph(C["LV_PH_SPECIES"], struct.pack("<H", sid), name.upper())


# --- messages ----------------------------------------------------------------

NAMES = {v: k for k, v in C.items() if k.startswith("LVMSG_")}


def message(kind: str, payload: bytes = b"") -> bytes:
    body = bytes([C["LVMSG_" + kind]]) + payload
    if len(body) > 255:
        raise ValueError(f"{kind} message too long ({len(body)} bytes)")
    return bytes([len(body)]) + body


def msg_reset() -> bytes:
    return message("RESET")


def msg_trainer(side: int, female: bool, name: str, trainer_class: int = 4) -> bytes:
    return message("TRAINER", bytes([side, int(female), trainer_class]) + encode_text(name)[:7] + bytes([EOS]))


def msg_mon(side: int, slot: int, species: int, level: int, hp: int, max_hp: int,
            status: int = 0, gender: str = "", shiny: bool = False, nickname: str = "") -> bytes:
    flags = 0
    if gender == "M":
        flags |= C["LV_MONFLAG_MALE"]
    elif gender == "F":
        flags |= C["LV_MONFLAG_FEMALE"]
    if shiny:
        flags |= C["LV_MONFLAG_SHINY"]
    nick = encode_text(nickname)[:10] if nickname else b""
    max_hp = max(1, min(max_hp, 0xFFFF))
    hp = max(0, min(hp, max_hp))
    level = max(1, min(level, 100))
    return message("MON", struct.pack("<BBHBHHBB", side, slot, species, level, hp, max_hp, status, flags) + nick + bytes([EOS]))


def msg_start(bgm: int = 0, environment: int = 0xFF, text_delay: int = 0) -> bytes:
    return message("START", struct.pack("<HBB", bgm, environment, text_delay))


def msg_text(text: Text | str) -> bytes:
    if isinstance(text, str):
        text = Text(text)
    return message("TEXT", bytes(text.data[:250]) + bytes([EOS]))


def msg_move(attacker: int, target: int, move: int, turn: int = 0) -> bytes:
    return message("MOVE", struct.pack("<BBHB", attacker, target, move, turn))


def msg_hit(side: int, effect: int) -> bytes:
    return message("HIT", bytes([side, effect]))


def msg_hp(side: int, hp: int) -> bytes:
    return message("HP", struct.pack("<BH", side, max(0, min(hp, 0xFFFF))))


def msg_switch_out(side: int) -> bytes:
    return message("SWITCH_OUT", bytes([side]))


def msg_switch_in(side: int, slot: int) -> bytes:
    return message("SWITCH_IN", bytes([side, slot]))


def msg_faint(side: int) -> bytes:
    return message("FAINT", bytes([side]))


def msg_status(side: int, status: int, anim: bool) -> bytes:
    return message("STATUS", bytes([side, status, int(anim)]))


def msg_stat_change(side: int, stat: int, delta: int) -> bytes:
    return message("STAT_CHANGE", struct.pack("<BBb", side, stat, max(-12, min(12, delta))))


def msg_anim(side: int, anim: int, arg: int = 0) -> bytes:
    return message("ANIM", struct.pack("<BBH", side, anim, arg))


def msg_end(winner: int) -> bytes:
    return message("END", bytes([winner]))


def msg_wait(frames: int) -> bytes:
    return message("WAIT", struct.pack("<H", frames))


def msg_weather(weather: int, anim: bool) -> bytes:
    return message("WEATHER", bytes([weather, int(anim)]))


def msg_se(song: int) -> bytes:
    return message("SE", struct.pack("<H", song))


def msg_status_anim(side: int, which: int) -> bytes:
    return message("STATUS_ANIM", bytes([side, which]))


def msg_cry(side: int) -> bytes:
    return message("CRY", bytes([side]))


def describe(msg: bytes, text: str | None = None) -> str:
    """One message, human-readable (for --dump)."""
    kind = NAMES.get(msg[1], f"0x{msg[1]:02X}").removeprefix("LVMSG_")
    if text is not None:
        return f"{kind:11} {text!r}"
    return f"{kind:11} {msg[2:].hex(' ')}"
