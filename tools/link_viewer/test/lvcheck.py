"""Checks a viewer message stream against what src/link_viewer.c expects.

check_stream() follows the stream like the ROM does (parties per session,
which mons are out) and returns a list of problems: malformed messages, ids
outside the ROM's tables, and events that the ROM would drop or that would
leave it stuck (a START without a lead, a switch to an empty slot...).
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import lvproto as P  # noqa: E402

C = P.C
T = {k.removeprefix("LVMSG_"): v for k, v in C.items() if k.startswith("LVMSG_")}
NUM_SPECIES = max(P.SPECIES.by_id.values()) + 1
MOVES_COUNT = max(P.MOVES.by_id.values()) + 1
ITEMS_COUNT = max(P.ITEMS.by_id.values()) + 1
ABILITIES_COUNT = max(P.ABILITIES.by_id.values()) + 1
# VIEWER_ANIMS in src/link_viewer.c: the general animations the viewer plays.
VIEWER_ANIMS = {P.ANIMS["B_ANIM_" + n] for n in (
    "CASTFORM_CHANGE", "SUBSTITUTE_FADE", "SUBSTITUTE_APPEAR", "ITEM_KNOCKOFF", "TURN_TRAP", "HELD_ITEM_EFFECT",
    "FOCUS_BAND", "RAIN_CONTINUES", "SUN_CONTINUES", "SANDSTORM_CONTINUES", "HAIL_CONTINUES", "LEECH_SEED_DRAIN",
    "MON_HIT", "INGRAIN_HEAL", "WISH_HEAL")}
NUM_ANIMS = P.ANIMS["B_ANIM_WISH_HEAL"] + 1

# Exact payload sizes (TEXT, TRAINER and MON end in a string).
FIXED = {"RESET": 0, "START": 4, "MOVE": 5, "HIT": 2, "HP": 3, "SWITCH_OUT": 1, "SWITCH_IN": 2, "FAINT": 1,
         "STATUS": 3, "STAT_CHANGE": 3, "ANIM": 4, "END": 1, "WAIT": 2, "WEATHER": 2, "SE": 2,
         "STATUS_ANIM": 2, "CRY": 1}
PH_SIZE = {C["LV_PH_MOVE"]: 2, C["LV_PH_ITEM"]: 2, C["LV_PH_ABILITY"]: 1, C["LV_PH_SPECIES"]: 2}


def split_messages(stream: bytes):
    """Yields (offset, message bytes); raises ValueError on a cut-off message."""
    i = 0
    while i < len(stream):
        n = stream[i]
        if n == 0:
            i += 1
            continue
        if i + 1 + n > len(stream):
            raise ValueError(f"message at {i} runs past the end of the stream")
        yield i, stream[i:i + 1 + n]
        i += 1 + n


def check_text(data: bytes, limit: int) -> list[str]:
    problems = []
    if not data or data[-1] != P.EOS:
        problems.append("text doesn't end in EOS")
    body = data[:-1]
    if P.EOS in body:
        problems.append("EOS inside the text")
    if len(body) > limit:
        problems.append(f"text longer than {limit} bytes")
    i = 0
    while i < len(body):
        b = body[i]
        if b == P.PLACEHOLDER:
            code = body[i + 1] if i + 1 < len(body) else None
            if code not in PH_SIZE:
                problems.append(f"unknown placeholder {code!r}")
                break
            arg = body[i + 2:i + 2 + PH_SIZE[code]]
            if len(arg) < PH_SIZE[code]:
                problems.append("placeholder cut off")
                break
            value = int.from_bytes(arg, "little")
            bound = {C["LV_PH_MOVE"]: MOVES_COUNT, C["LV_PH_ITEM"]: ITEMS_COUNT,
                     C["LV_PH_ABILITY"]: ABILITIES_COUNT, C["LV_PH_SPECIES"]: NUM_SPECIES}[code]
            if not 0 < value < bound:
                problems.append(f"placeholder id {value} out of range")
            i += 2 + PH_SIZE[code]
            continue
        if b in (0xFC, 0xF8, 0xF9, 0xFA, 0xFB, 0xF7):
            problems.append(f"control code {b:#x} in text")
        i += 1
    return problems


class RomModel:
    def __init__(self):
        self.reset()
        self.in_battle = False

    def reset(self):
        self.party = [[None] * 6, [None] * 6]   # species per slot
        self.out = [None, None]                 # slot out per side (None = no mon out)


def check_stream(stream: bytes) -> list[str]:
    problems: list[str] = []
    rom = RomModel()

    def bad(off, msg, what):
        problems.append(f"@{off} {P.NAMES.get(msg[1], hex(msg[1]))}: {what}")

    try:
        msgs = list(split_messages(stream))
    except ValueError as e:
        return [str(e)]
    for off, msg in msgs:
        kind = P.NAMES.get(msg[1], "").removeprefix("LVMSG_")
        data = msg[2:]
        if not kind:
            bad(off, msg, "unknown type")
            continue
        if kind in FIXED and len(data) != FIXED[kind]:
            bad(off, msg, f"payload {len(data)} bytes, expected {FIXED[kind]}")
            continue
        side = data[0] if data else 0
        if kind in ("MOVE", "HIT", "HP", "SWITCH_OUT", "SWITCH_IN", "FAINT", "STATUS", "STAT_CHANGE", "ANIM",
                    "STATUS_ANIM", "CRY", "TRAINER", "MON") and side > 1:
            bad(off, msg, f"side {side}")

        if kind == "RESET":
            rom.reset()
            rom.in_battle = False
        elif kind == "TRAINER":
            problems += [f"@{off} TRAINER: {p}" for p in check_text(data[3:], 7)]
        elif kind == "MON":
            if len(data) < 12:
                bad(off, msg, "too short")
                continue
            s, slot, species, level, hp, max_hp, status, flags = struct.unpack_from("<BBHBHHBB", data)
            problems += [f"@{off} MON: {p}" for p in check_text(data[11:], 10)]
            if slot >= 6:
                bad(off, msg, f"slot {slot}")
                continue
            if not 0 < species < NUM_SPECIES:
                bad(off, msg, f"species {species}")
            if not 1 <= level <= 100:
                bad(off, msg, f"level {level}")
            if max_hp == 0 or hp > max_hp:
                bad(off, msg, f"hp {hp}/{max_hp}")
            if status > 6:
                bad(off, msg, f"status {status}")
            rom.party[s & 1][slot] = species or None
            if rom.in_battle and rom.out[s & 1] == slot and not species:
                bad(off, msg, "removes the mon that is out")
        elif kind == "START":
            if rom.in_battle:
                bad(off, msg, "START during a battle (ends it)")
            if not rom.party[0][0] or not rom.party[1][0]:
                bad(off, msg, "no lead on one side: the ROM won't start the battle")
            rom.in_battle = True
            rom.out = [0, 0]
        elif not rom.in_battle:
            bad(off, msg, "battle event outside a battle (dropped)")
        elif kind == "TEXT":
            problems += [f"@{off} TEXT: {p}" for p in check_text(data, 250)]
        elif kind == "MOVE":
            move = struct.unpack_from("<H", data, 2)[0]
            if not 0 < move < MOVES_COUNT:
                bad(off, msg, f"move {move}")
            if rom.out[side & 1] is None:
                bad(off, msg, "attacker not out (dropped)")
        elif kind in ("HIT", "STATUS_ANIM", "STAT_CHANGE", "ANIM", "FAINT", "SWITCH_OUT", "CRY"):
            if rom.out[side & 1] is None:
                bad(off, msg, "no mon out (dropped)")
            if kind == "STAT_CHANGE" and (not 1 <= data[1] <= 7 or data[2] == 0):
                bad(off, msg, f"stat {data[1]} delta {data[2]}")
            if kind == "ANIM" and data[1] not in VIEWER_ANIMS:
                bad(off, msg, f"anim {data[1]} isn't one the viewer plays")
            if kind in ("FAINT", "SWITCH_OUT"):
                rom.out[side & 1] = None
        elif kind == "SWITCH_IN":
            slot = data[1]
            if slot >= 6 or not rom.party[side & 1][slot]:
                bad(off, msg, f"switch to empty slot {slot}")
            else:
                rom.out[side & 1] = slot
        elif kind == "WEATHER":
            if data[0] > 4:
                bad(off, msg, f"weather {data[0]}")
        elif kind == "END":
            rom.in_battle = False
    return problems
