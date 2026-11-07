"""Showdown battle protocol -> Link Battle Viewer messages.

The Translator follows a battle from its protocol lines (as a Showdown client
sees them) and emits viewer messages that replay it in Gen 3 style: the
move's text, its animation, the hit, the HP bar, then "It's super
effective!", and so on.

It keeps the whole battle state, so the viewer can be (re)started at any
point: resync() emits a fresh session that starts from the current state.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

import lvproto as P
from lvproto import Text

C = P.C

STATUS = {"": C["LV_STATUS_NONE"], "slp": C["LV_STATUS_SLP"], "psn": C["LV_STATUS_PSN"],
          "brn": C["LV_STATUS_BRN"], "frz": C["LV_STATUS_FRZ"], "par": C["LV_STATUS_PAR"],
          "tox": C["LV_STATUS_TOX"]}
WEATHER = {"none": C["LV_WEATHER_NONE"], "raindance": C["LV_WEATHER_RAIN"], "sunnyday": C["LV_WEATHER_SUN"],
           "sandstorm": C["LV_WEATHER_SAND"], "hail": C["LV_WEATHER_HAIL"]}
STATS = {"atk": (1, "ATTACK"), "def": (2, "DEFENSE"), "spe": (3, "SPEED"), "spa": (4, "SP. ATK"),
         "spd": (5, "SP. DEF"), "accuracy": (6, "accuracy"), "evasion": (7, "evasiveness")}
PLACEHOLDER_SPECIES = P.SPECIES.get("Unown")  # unrevealed party slots (only their Poké Ball shows)

WEATHER_TEXT = {
    "raindance": ("It started to rain!", "Rain continues to fall.", "The rain stopped."),
    "sunnyday": ("The sunlight got bright!", "The sunlight is strong.", "The sunlight faded."),
    "sandstorm": ("A sandstorm brewed!", "The sandstorm rages.", "The sandstorm subsided."),
    "hail": ("It started to hail!", "Hail continues to fall.", "The hail stopped."),
}
WEATHER_ABILITY_TEXT = {
    "raindance": " made it rain!",
    "sunnyday": " intensified the sun's rays!",
    "sandstream": " whipped up a sandstorm!",
    "sandstorm": " whipped up a sandstorm!",
}
PREPARE_TEXT = {
    "solarbeam": " took in sunlight!", "fly": " flew up high!", "dig": " dug a hole!",
    "dive": " hid underwater!", "bounce": " sprang up!", "razorwind": " whipped up a whirlwind!",
    "skyattack": " is glowing!", "skullbash": " lowered its head!",
}
STATUS_TEXT = {
    "par": " is paralyzed! It may be unable to move!", "brn": " was burned!", "psn": " was poisoned!",
    "tox": " is badly poisoned!", "slp": " fell asleep!", "frz": " was frozen solid!",
}
STATUS_NAME = {"par": "paralysis", "brn": "its burn", "psn": "poison", "tox": "poison",
               "slp": "sleep", "frz": "the ice"}
CANT_TEXT = {
    "par": " is paralyzed! It can't move!", "slp": " is fast asleep.", "frz": " is frozen solid!",
    "recharge": " must recharge!", "flinch": " flinched!", "truant": " is loafing around!",
    "attract": " is immobilized by love!", "focuspunch": " lost its focus and couldn't move!",
    "nopp": " has no PP left!", "damp": " can't use explosive moves!",
}
SIDE_TEXT = {  # (start, end) after "<trainer>'s team"
    "reflect": (" became stronger against physical moves!", "'s REFLECT wore off!"),
    "lightscreen": (" became stronger against special moves!", "'s LIGHT SCREEN wore off!"),
    "safeguard": (" became cloaked in a mystical veil!", " is no longer protected by SAFEGUARD!"),
    "mist": (" became shrouded in MIST!", " is no longer protected by MIST!"),
}


@dataclass
class Mon:
    key: str            # nickname as in "p1a: Nickname"
    species: str
    level: int = 100
    gender: str = ""
    shiny: bool = False
    hp: int = 100
    max_hp: int = 100
    status: str = ""
    boosts: dict = field(default_factory=dict)

    @property
    def fainted(self) -> bool:
        return self.hp == 0


@dataclass
class Side:
    sid: str            # "p1" / "p2"
    name: str = ""
    teamsize: int = 0
    mons: dict = field(default_factory=dict)   # key -> Mon, in order of appearance
    active: Mon | None = None


def parse_hp(cond: str) -> tuple[int, int | None, str]:
    """'53/100 par' -> (53, 100, 'par'); '0 fnt' -> (0, None, '')."""
    parts = cond.split()
    if not parts:
        return 0, None, ""
    status = parts[1] if len(parts) > 1 and parts[1] != "fnt" else ""
    if "/" in parts[0]:
        hp, mx = parts[0].split("/")
        return int(hp), int(mx), status
    return int(parts[0]) if parts[0].isdigit() else 0, None, status


def parse_details(details: str) -> tuple[str, int, str, bool]:
    """'Zapdos, L50, F, shiny' -> ('Zapdos', 50, 'F', True)."""
    bits = [b.strip() for b in details.split(",")]
    species, level, gender, shiny = bits[0], 100, "", False
    for b in bits[1:]:
        if b.startswith("L") and b[1:].isdigit():
            level = int(b[1:])
        elif b in ("M", "F"):
            gender = b
        elif b == "shiny":
            shiny = True
    return species, level, gender, shiny


def split_tags(args: list[str]) -> tuple[list[str], dict[str, str]]:
    """Positional args and [tag] args: '[from] item: Leftovers' -> {'from': 'item: Leftovers'}."""
    pos, tags = [], {}
    for a in args:
        if a.startswith("["):
            k, _, v = a[1:].partition("]")
            tags[k] = v.strip()
        else:
            pos.append(a)
    return pos, tags


def effect_name(s: str) -> tuple[str, str]:
    """'item: Leftovers' -> ('item', 'Leftovers'); 'Leech Seed' -> ('', 'Leech Seed')."""
    kind, sep, name = s.partition(": ")
    return (kind, name) if sep else ("", s)


class Translator:
    def __init__(self, emit: Callable[[bytes, str], None], bottom: str = "", me: str = "",
                 bgm: int = 0, environment: int = 0xFF, text_delay: int = 0, log=print):
        self.emit_raw = emit
        self.bottom_pref = bottom   # "p1" / "p2" / ""
        self.me = P.to_id(me)       # the player whose side goes at the bottom (if bottom is "")
        self.bgm = bgm
        self.environment = environment
        self.text_delay = text_delay
        self.log = log
        self.silent = False
        self.reset_battle()

    # --- state -------------------------------------------------------------

    def reset_battle(self):
        self.sides = {"p1": Side("p1"), "p2": Side("p2")}
        self.phase = "pre"          # pre -> intro (after |start|) -> battle -> over
        self.weather = "none"
        self.session_slots: dict[int, list[str | None]] | None = None
        self.held_move: list[str] | None = None   # a |move| line waiting for the next line
        self.hits: dict = {}        # target key -> hits by the current move
        self.eff: dict = {}         # target key -> effectiveness of the current move
        self.crit: set = set()
        self.eff_pending: list = []
        self.charging: dict = {}
        self.last_item: dict = {}
        self.intimidate: Mon | None = None
        self.current_move: tuple | None = None

    def bottom(self) -> str:
        if self.bottom_pref in ("p1", "p2"):
            return self.bottom_pref
        if self.me:
            for sid, side in self.sides.items():
                if P.to_id(side.name) == self.me:
                    return sid
        return "p1"

    def vside(self, sid: str) -> int:
        return 0 if sid == self.bottom() else 1

    def side_of(self, ident: str) -> Side:
        return self.sides[ident[:2]]

    def mon(self, ident: str, details: str | None = None) -> Mon:
        """The mon for 'p1a: Nick' (created on first sight)."""
        side = self.side_of(ident)
        key = ident.split(": ", 1)[1] if ": " in ident else ident[4:]
        m = side.mons.get(key)
        if m is None:
            species = parse_details(details)[0] if details else key
            m = side.mons[key] = Mon(key, species)
        if details:
            m.species, m.level, m.gender, m.shiny = parse_details(details)
        return m

    def set_hp(self, m: Mon, cond: str):
        hp, mx, status = parse_hp(cond)
        if mx:
            if m.max_hp != mx and m.max_hp == 100 and m.hp == 100:
                m.max_hp = mx        # first exact value seen
            elif m.max_hp != mx:
                hp = round(hp * m.max_hp / mx)
        m.hp = max(0, min(hp, m.max_hp))
        if hp == 0:
            m.status = ""
        elif status:
            m.status = status

    # --- output ------------------------------------------------------------

    def emit(self, msg: bytes, desc: str = ""):
        if not self.silent:
            self.emit_raw(msg, desc)

    @property
    def live(self) -> bool:
        return self.session_slots is not None and not self.silent

    def text(self, *parts):
        t = Text(*parts)
        self.emit(P.msg_text(t), t.text())

    def name(self, m: Mon, sid: str, prefix: bool = True) -> Text:
        prefix = "Foe " if prefix and self.vside(sid) == 1 else ""
        if P.to_id(m.key) == P.to_id(m.species) or not m.key:
            return Text(prefix, Text.species(m.species))
        return Text(prefix + m.key[:10])

    def nm(self, ident: str) -> Text:
        return self.name(self.mon(ident), ident[:2])

    def trainer(self, sid: str) -> str:
        return (self.sides[sid].name or sid)[:7]

    def slot_of(self, sid: str, m: Mon) -> int:
        """The viewer party slot for a mon, assigning one on first sight."""
        slots = self.session_slots[self.vside(sid)]
        if m.key in slots:
            return slots.index(m.key)
        for i, k in enumerate(slots):
            if k is None or k == "#":
                slots[i] = m.key
                return i
        # More mons than slots (shouldn't happen): reuse the last one.
        slots[-1] = m.key
        return len(slots) - 1

    def send_mon(self, sid: str, slot: int, m: Mon | None):
        vs = self.vside(sid)
        if m is None:
            self.emit(P.msg_mon(vs, slot, PLACEHOLDER_SPECIES, 50, 1, 1), f"{sid} slot {slot}: (unrevealed)")
            return
        nick = "" if P.to_id(m.key) == P.to_id(m.species) else m.key
        self.emit(P.msg_mon(vs, slot, P.SPECIES.get(m.species), m.level, m.hp, m.max_hp,
                            STATUS.get(m.status, 0), m.gender, m.shiny, nick),
                  f"{sid} slot {slot}: {m.key} ({m.species}) L{m.level} {m.hp}/{m.max_hp} {m.status}")

    def refresh_mon(self, sid: str, m: Mon):
        if self.live and m.key in self.session_slots[self.vside(sid)]:
            self.send_mon(sid, self.slot_of(sid, m), m)

    # --- sessions ----------------------------------------------------------

    def start_session(self):
        """Emit a fresh viewer session for the current state: both teams with
        the active mons first (the battle intro sends out slot 0), then START."""
        self.session_slots = {}
        self.emit(P.msg_reset(), "reset")
        for sid in ("p1", "p2"):
            side = self.sides[sid]
            vs = self.vside(sid)
            self.emit(P.msg_trainer(vs, False, self.trainer(sid)), f"{sid} trainer {side.name!r}")
            order = list(side.mons.values())
            if side.active in order:
                order.remove(side.active)
                order.insert(0, side.active)
            slots: list[str | None] = [m.key for m in order[:6]]
            count = max(side.teamsize, len(slots)) if side.teamsize else max(len(slots), 1)
            slots += [None] * (min(count, 6) - len(slots))
            slots += ["#"] * (6 - len(slots))  # beyond the team size: empty
            self.session_slots[vs] = slots
            for i, k in enumerate(slots):
                if k == "#":
                    continue
                self.send_mon(sid, i, side.mons.get(k) if k else None)
            for i, k in enumerate(slots):
                if k is None:
                    slots[i] = None  # still free for a mon revealed later
        self.emit(P.msg_start(self.bgm, self.environment, self.text_delay), "start")
        if self.weather != "none":
            self.emit(P.msg_weather(WEATHER.get(self.weather, 0), False), f"weather {self.weather}")
        self.phase = "battle"

    def resync(self):
        """Restart the viewer from the current state (it lost the stream)."""
        if self.phase in ("battle", "over") or (self.phase == "intro" and all(s.active for s in self.sides.values())):
            self.start_session()
        else:
            self.session_slots = None

    # --- input -------------------------------------------------------------

    def feed(self, line: str):
        if not line.startswith("|"):
            return
        parts = line[1:].split("|")
        if self.held_move is not None:
            held, self.held_move = self.held_move, None
            self.handle(held, parts)
        if parts[0] == "move" and self.phase == "battle":
            self.held_move = parts   # decide on its animation once the next line is known
            return
        self.handle(parts, None)

    def flush(self):
        """The source went quiet: finish a held |move| line and pending texts."""
        if self.held_move is not None:
            held, self.held_move = self.held_move, None
            self.handle(held, None)
        self.flush_effectiveness()

    def feed_silent(self, lines):
        """Take in a backlog without showing it (then resync() shows the result)."""
        self.silent = True
        try:
            for line in lines:
                self.feed(line)
            self.flush()
        finally:
            self.silent = False

    DAMAGE_GROUP = {"-damage", "-crit", "-supereffective", "-resisted", "-activate"}

    def handle(self, parts: list[str], nxt: list[str] | None):
        cmd, args = parts[0], parts[1:]
        if cmd not in self.DAMAGE_GROUP or (cmd == "-damage" and "[from]" in "".join(args)):
            self.flush_effectiveness()
        if cmd.startswith("-"):
            handler = getattr(self, "on__" + cmd[1:], None)
        else:
            handler = getattr(self, "on_" + cmd, None) if cmd.isidentifier() else None
        try:
            if cmd == "move":
                self.on_move(args, nxt)
            elif handler is not None:
                handler(args)
        except (IndexError, ValueError, KeyError) as e:
            self.log(f"[translator] couldn't handle |{'|'.join(parts)}: {e!r}")

    # --- battle structure ----------------------------------------------------

    def on_init(self, args):
        self.reset_battle()

    def on_player(self, args):
        if len(args) >= 2 and args[1]:
            self.sides[args[0]].name = args[1]

    def on_teamsize(self, args):
        self.sides[args[0]].teamsize = int(args[1])

    def on_start(self, args):
        self.phase = "intro"

    def maybe_start(self):
        if self.phase == "intro" and all(s.active for s in self.sides.values()):
            self.start_session()

    def on_turn(self, args):
        self.maybe_start()

    def on_win(self, args):
        self.maybe_start()
        winner = args[0]
        sid = next((s for s, side in self.sides.items() if side.name == winner), None)
        if self.live:
            self.text(f"{winner[:16]} won the battle!")
            self.emit(P.msg_end(self.vside(sid) if sid else 2), "end")
        self.phase = "over"

    def on_tie(self, args):
        self.maybe_start()
        if self.live:
            self.text("The battle ended in a draw!")
            self.emit(P.msg_end(2), "end")
        self.phase = "over"

    # --- switching -----------------------------------------------------------

    def on_switch(self, args, drag=False):
        ident, details = args[0], args[1]
        sid = ident[:2]
        side = self.sides[sid]
        m = self.mon(ident, details)
        if len(args) > 2:
            self.set_hp(m, args[2])
        old = side.active
        if self.phase == "intro":
            side.active = m
            return
        self.maybe_start()
        if not self.live:
            side.active = m
            return
        vs = self.vside(sid)
        baton_pass = self.current_move is not None and self.current_move[0][:2] == sid and self.current_move[1] == P.move_id("Baton Pass")
        if old is not None and old is not m and not old.fainted:
            if not drag and not baton_pass:
                if vs == 0:
                    self.text(self.name(old, sid), ", come back!")
                else:
                    self.text(f"{self.trainer(sid)} withdrew ", self.name(old, sid, False), "!")
            self.emit(P.msg_switch_out(vs), "switch out")
        old_boosts_cleared = old
        if old_boosts_cleared is not None:
            old_boosts_cleared.boosts = {}
        slot = self.slot_of(sid, m)
        self.send_mon(sid, slot, m)
        if not drag:
            if vs == 0:
                self.text("Go! ", self.name(m, sid), "!")
            else:
                self.text(f"{self.trainer(sid)} sent out ", self.name(m, sid, False), "!")
        self.emit(P.msg_switch_in(vs, slot), f"switch in slot {slot}")
        if drag:
            self.text(self.name(m, sid), " was dragged out!")
        side.active = m
        self.current_move = None

    def on_drag(self, args):
        self.on_switch(args, drag=True)

    def on_faint(self, args):
        m = self.mon(args[0])
        had_hp = m.hp > 0
        m.hp, m.status = 0, ""
        if self.live:
            if had_hp:  # Explosion, Destiny Bond, Perish Song: no -damage line
                self.emit(P.msg_hp(self.vside(args[0][:2]), 0), "hp 0")
            self.emit(P.msg_faint(self.vside(args[0][:2])), "faint")
            self.text(self.nm(args[0]), " fainted!")

    # --- moves -------------------------------------------------------------

    def on_move(self, args, nxt):
        pos, tags = split_tags(args)
        ident, move = pos[0], pos[1]
        target = pos[2] if len(pos) > 2 and pos[2] else ""
        self.maybe_start()
        self.hits, self.eff, self.crit = {}, {}, set()
        sid = ident[:2]
        m = self.mon(ident)
        mid = P.move_id(move)
        tsid = target[:2] if target else ("p2" if sid == "p1" else "p1")
        self.current_move = (ident, mid, target)
        if not self.live:
            return
        if "still" in tags and nxt is not None and nxt[0] == "-prepare":
            return  # the charge turn: -prepare shows it
        self.text(self.name(m, sid), " used ", Text.move(move), "!")
        if "still" in tags or "miss" in tags or "notarget" in tags:
            return
        if nxt is not None:
            ncmd = nxt[0]
            nargs = "|".join(nxt[1:])
            if ncmd in ("-miss", "-fail", "-immune", "-notarget", "-prepare"):
                return
            if ncmd == "-activate" and ("Protect" in nargs or "Detect" in nargs):
                return
        turn = 0
        if self.charging.get(m.key) == P.to_id(move):
            turn = 1
            del self.charging[m.key]
        self.emit(P.msg_move(self.vside(sid), self.vside(tsid), mid, turn), f"anim {move}")

    def on__prepare(self, args):
        ident, move = args[0], args[1]
        m = self.mon(ident)
        self.charging[m.key] = P.to_id(move)
        if self.live:
            vs = self.vside(ident[:2])
            self.emit(P.msg_move(vs, 1 - vs, P.move_id(move), 0), f"anim {move} (charge)")
            suffix = PREPARE_TEXT.get(P.to_id(move))
            if suffix:
                self.text(self.nm(ident), suffix)

    def on__crit(self, args):
        self.crit.add(args[0])

    def on__supereffective(self, args):
        self.eff[args[0]] = C["LV_EFFECT_SUPER"]

    def on__resisted(self, args):
        self.eff[args[0]] = C["LV_EFFECT_NOT_VERY"]

    def flush_effectiveness(self):
        for target, eff in self.eff_pending:
            if eff == C["LV_EFFECT_SUPER"]:
                self.text("It's super effective!")
            elif eff == C["LV_EFFECT_NOT_VERY"]:
                self.text("It's not very effective…")
        self.eff_pending = []

    def direct_hit(self, ident: str):
        """A hit by the current move on ident: animation (again, for the
        second hit of a multi-hit move), hit flash and sound."""
        vs = self.vside(ident[:2])
        n = self.hits.get(ident, 0)
        if n > 0 and self.current_move:
            atk, mid, _ = self.current_move
            self.emit(P.msg_move(self.vside(atk[:2]), vs, mid, 0), "anim (next hit)")
        self.hits[ident] = n + 1
        eff = self.eff.get(ident, C["LV_EFFECT_NORMAL"])
        self.emit(P.msg_hit(vs, eff), "hit")
        return eff

    def after_hit(self, ident: str, eff: int):
        if ident in self.crit:
            self.text("A critical hit!")
            self.crit.discard(ident)
        if eff != C["LV_EFFECT_NORMAL"] and all(t != ident for t, _ in self.eff_pending):
            self.eff_pending.append((ident, eff))

    def on__damage(self, args):
        pos, tags = split_tags(args)
        ident = pos[0]
        sid = ident[:2]
        m = self.mon(ident)
        before = m.hp
        self.set_hp(m, pos[1])
        if not self.live:
            return
        vs = self.vside(sid)
        source = tags.get("from", "")
        kind, what = effect_name(source)
        wid = P.to_id(what)
        of = tags.get("of", "")
        hp_msg = (P.msg_hp(vs, m.hp), f"hp {m.hp}/{m.max_hp}")
        if not source and self.current_move is not None and self.current_move[0][:2] == sid                 and self.current_move[0][4:] == ident[4:]:
            self.emit(*hp_msg)  # the user's own cost (Substitute)
        elif not source:
            eff = self.direct_hit(ident)
            self.emit(*hp_msg)
            self.after_hit(ident, eff)
        elif wid in ("psn", "tox"):
            self.text(self.nm(ident), " is hurt by poison!")
            self.emit(P.msg_status(vs, STATUS[m.status or wid], True), "poison anim")
            self.emit(*hp_msg)
        elif wid == "brn":
            self.text(self.nm(ident), " is hurt by its burn!")
            self.emit(P.msg_status(vs, STATUS["brn"], True), "burn anim")
            self.emit(*hp_msg)
        elif wid == "confusion":
            self.text("It hurt itself in its confusion!")
            self.emit(P.msg_hit(vs, C["LV_EFFECT_NORMAL"]), "hit")
            self.emit(*hp_msg)
        elif wid == "sandstorm":
            self.text(self.nm(ident), " is buffeted by the sandstorm!")
            self.emit(*hp_msg)
        elif wid == "hail":
            self.text(self.nm(ident), " is pelted by HAIL!")
            self.emit(*hp_msg)
        elif wid == "leechseed":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_LEECH_SEED_DRAIN"]), "leech seed anim")
            self.emit(*hp_msg)
            self.text(self.nm(ident), "'s health is sapped by LEECH SEED!")
        elif wid == "spikes":
            self.emit(*hp_msg)
            self.text(self.nm(ident), " is hurt by SPIKES!")
        elif wid == "recoil":
            self.emit(*hp_msg)
            self.text(self.nm(ident), " is hit with recoil!")
        elif wid == "curse":
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_CURSE"]), "curse anim")
            self.emit(*hp_msg)
            self.text(self.nm(ident), " is afflicted by the CURSE!")
        elif wid == "nightmare":
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_NIGHTMARE"]), "nightmare anim")
            self.emit(*hp_msg)
            self.text(self.nm(ident), " is locked in a NIGHTMARE!")
        elif kind == "ability" and of:
            self.emit(*hp_msg)
            self.text(self.nm(of), "'s ", Text.ability(what), " hurt ", self.nm(ident), "!")
        elif kind == "item":
            self.emit(*hp_msg)
            self.text(self.nm(ident), " was hurt by its ", Text.item(what), "!")
        elif kind == "move" or what:
            # Partial trapping (Wrap, Fire Spin...), crash damage, Belly Drum...
            self.emit(*hp_msg)
            if P.move_id(what) and wid not in ("bellydrum", "substitute", "painsplit"):
                if wid in ("highjumpkick", "jumpkick"):
                    self.text(self.nm(ident), " kept going and crashed!")
                else:
                    self.text(self.nm(ident), " is hurt by ", Text.move(what), "!")
        else:
            self.emit(*hp_msg)
        del before

    def on__heal(self, args):
        pos, tags = split_tags(args)
        ident = pos[0]
        m = self.mon(ident)
        self.set_hp(m, pos[1])
        if not self.live:
            return
        vs = self.vside(ident[:2])
        hp_msg = (P.msg_hp(vs, m.hp), f"hp {m.hp}/{m.max_hp}")
        kind, what = effect_name(tags.get("from", ""))
        wid = P.to_id(what)
        if "silent" in tags:
            self.emit(*hp_msg)
        elif kind == "item":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_HELD_ITEM_EFFECT"]), "item anim")
            if wid == "leftovers":
                self.text(self.nm(ident), " restored a little HP using its ", Text.item(what), "!")
            else:
                self.text(self.nm(ident), "'s ", Text.item(what), " restored health!")
            self.emit(*hp_msg)
        elif wid == "drain":
            self.emit(*hp_msg)
            of = tags.get("of")
            if of:
                self.text(self.nm(of), " had its energy drained!")
        elif wid == "wish":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_WISH_HEAL"]), "wish anim")
            wisher = tags.get("wisher", "")
            self.text(f"{wisher[:10] if wisher else 'The'}'s WISH came true!" if wisher else "The WISH came true!")
            self.emit(*hp_msg)
        elif wid == "ingrain":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_INGRAIN_HEAL"]), "ingrain anim")
            self.text(self.nm(ident), " absorbed nutrients with its roots!")
            self.emit(*hp_msg)
        elif kind == "ability":
            self.emit(*hp_msg)
            self.text(self.nm(ident), "'s ", Text.ability(what), " restored its HP a little!")
        else:
            self.emit(*hp_msg)
            self.text(self.nm(ident), " regained health!")

    def on__sethp(self, args):
        pos, tags = split_tags(args)
        m = self.mon(pos[0])
        self.set_hp(m, pos[1])
        if self.live:
            self.emit(P.msg_hp(self.vside(pos[0][:2]), m.hp), f"hp {m.hp}/{m.max_hp}")
            if "silent" not in tags:
                self.text("The battlers shared their pain!")

    def on__miss(self, args):
        if self.live:
            self.text(self.nm(args[0]), "'s attack missed!")

    def on__fail(self, args):
        pos, tags = split_tags(args)
        if not self.live:
            return
        what = P.to_id(effect_name(pos[1])[1]) if len(pos) > 1 else ""
        if what == "substitute" and "weak" in tags:
            self.text("It was too weak to make a SUBSTITUTE!")
        elif what == "substitute":
            self.text(self.nm(pos[0]), " already has a SUBSTITUTE!")
        else:
            self.text("But it failed!")

    def on__immune(self, args):
        pos, tags = split_tags(args)
        if self.live:
            self.text("It doesn't affect ", self.nm(pos[0]), "…")

    def on__notarget(self, args):
        if self.live:
            self.text("But there was no target…")

    def on__ohko(self, args):
        if self.live:
            self.text("It's a one-hit KO!")

    def on__hitcount(self, args):
        if self.live:
            n = int(args[1])
            self.text(f"Hit {n} time{'s' if n != 1 else ''}!")

    def on__nothing(self, args):
        if self.live:
            self.text("But nothing happened!")

    # --- status -------------------------------------------------------------

    def set_status(self, ident: str, status: str, anim: bool):
        sid = ident[:2]
        m = self.mon(ident)
        m.status = status
        side = self.sides[sid]
        if not self.live:
            return
        if side.active is m:
            self.emit(P.msg_status(self.vside(sid), STATUS.get(status, 0), anim), f"status {status or 'none'}")
        else:
            self.refresh_mon(sid, m)

    def on__status(self, args):
        pos, tags = split_tags(args)
        ident, status = pos[0], pos[1]
        kind, what = effect_name(tags.get("from", ""))
        if P.to_id(what) == "rest":
            self.set_status(ident, status, False)
            if self.live:
                self.text(self.nm(ident), " slept and became healthy!")
            return
        self.set_status(ident, status, True)
        if self.live and "silent" not in tags:
            self.text(self.nm(ident), STATUS_TEXT.get(status, " got a status condition!"))

    def on__curestatus(self, args):
        pos, tags = split_tags(args)
        ident, status = pos[0], pos[1]
        if ident[2:3] not in ("a", "b", "c"):
            ident = ident[:2] + "a" + ident[2:]  # "p1: Blissey" (party mon)
        m = self.mon(ident)
        kind, what = effect_name(tags.get("from", ""))
        self.set_status(ident, "", False)
        if not self.live or "silent" in tags:
            return
        item = self.last_item.pop(m.key, None)
        if kind == "item" or item:
            self.text(self.nm(ident), "'s ", Text.item(what if kind == "item" else item),
                      f" cured {STATUS_NAME.get(status, 'its status')}!")
        elif kind == "ability":
            pass  # Natural Cure, Shed Skin: silent here (Shed Skin's text comes from -activate)
        elif status == "slp":
            self.text(self.nm(ident), " woke up!")
        elif status == "frz":
            self.text(self.nm(ident), " was defrosted!")
        elif "msg" in tags:
            self.text(self.nm(ident), f" was cured of {STATUS_NAME.get(status, 'its status')}!")

    def on__cureteam(self, args):
        pos, tags = split_tags(args)
        sid = pos[0][:2]
        kind, what = effect_name(tags.get("from", ""))
        for m in self.sides[sid].mons.values():
            if m.status:
                m.status = ""
                if self.live and self.sides[sid].active is not m:
                    self.refresh_mon(sid, m)
        if self.live:
            act = self.sides[sid].active
            if act:
                self.emit(P.msg_status(self.vside(sid), 0, False), "status none")
            if P.to_id(what) == "healbell":
                self.text("A bell chimed!")
            else:
                self.text("A soothing aroma wafted through the area!")

    def on_cant(self, args):
        pos, tags = split_tags(args)
        ident, reason = pos[0], pos[1]
        self.maybe_start()
        if not self.live:
            return
        kind, what = effect_name(reason)
        rid = P.to_id(what)
        m = self.mon(ident)
        vs = self.vside(ident[:2])
        if rid in ("par", "slp", "frz"):
            self.emit(P.msg_status(vs, STATUS[rid if m.status != "tox" else "tox"] if m.status else STATUS[rid], True), "status anim")
        if rid in CANT_TEXT:
            self.text(self.nm(ident), CANT_TEXT[rid])
        elif rid == "taunt" and len(pos) > 2:
            self.text(self.nm(ident), " can't use ", Text.move(pos[2]), " after the taunt!")
        elif rid == "disable" and len(pos) > 2:
            self.text(self.nm(ident), "'s ", Text.move(pos[2]), " is disabled!")
        elif rid in ("imprison",) and len(pos) > 2:
            self.text(self.nm(ident), " can't use the sealed ", Text.move(pos[2]), "!")
        else:
            self.text(self.nm(ident), " can't move!")

    # --- stats ---------------------------------------------------------------

    def boost(self, ident: str, stat: str, delta: int, text: bool = True):
        m = self.mon(ident)
        num, label = STATS.get(stat, (0, stat))
        cur = m.boosts.get(stat, 0)
        new = max(-6, min(6, cur + delta))
        m.boosts[stat] = new
        if not self.live:
            return
        vs = self.vside(ident[:2])
        if delta != 0 and num:
            self.emit(P.msg_stat_change(vs, num, delta), f"{stat} {delta:+d}")
        if not text:
            return
        if delta == 0:
            self.text(self.nm(ident), f"'s {label} won't go {'higher' if cur >= 0 else 'lower'}!")
        elif delta > 0:
            self.text(self.nm(ident), f"'s {label} {'sharply ' if delta >= 2 else ''}rose!")
        else:
            self.text(self.nm(ident), f"'s {label} {'harshly ' if delta <= -2 else ''}fell!")

    def on__boost(self, args):
        pos, tags = split_tags(args)
        self.boost(pos[0], pos[1], int(pos[2]))

    def on__unboost(self, args):
        pos, tags = split_tags(args)
        ident, stat, amount = pos[0], pos[1], int(pos[2])
        if self.intimidate is not None and self.live:
            by, self.intimidate = self.intimidate, None
            self.boost(ident, stat, -amount, text=False)
            self.text(by, "'s INTIMIDATE cuts ", self.nm(ident), "'s ATTACK!")
            return
        self.boost(ident, stat, -amount)

    def on__setboost(self, args):
        pos, tags = split_tags(args)
        ident, stat, value = pos[0], pos[1], int(pos[2])
        m = self.mon(ident)
        self.boost(ident, stat, value - m.boosts.get(stat, 0), text=False)
        if self.live and P.to_id(effect_name(tags.get("from", ""))[1]) == "bellydrum":
            self.text(self.nm(ident), " cut its own HP and maximized ATTACK!")

    def on__clearallboost(self, args):
        for side in self.sides.values():
            for m in side.mons.values():
                m.boosts = {}
        if self.live:
            self.text("All stat changes were eliminated!")

    def on__clearnegativeboost(self, args):
        m = self.mon(args[0])
        m.boosts = {k: v for k, v in m.boosts.items() if v > 0}

    def on__clearboost(self, args):
        self.mon(args[0]).boosts = {}

    # --- field ---------------------------------------------------------------

    def on__weather(self, args):
        pos, tags = split_tags(args)
        weather = P.to_id(pos[0])
        old = self.weather
        self.weather = weather
        if not self.live:
            return
        code = WEATHER.get(weather, 0)
        if weather == "none":
            self.emit(P.msg_weather(0, False), "weather none")
            if old in WEATHER_TEXT:
                self.text(WEATHER_TEXT[old][2])
            return
        start, upkeep, _ = WEATHER_TEXT.get(weather, ("The weather changed!", "", ""))
        kind, what = effect_name(tags.get("from", ""))
        if "upkeep" in tags:
            self.emit(P.msg_weather(code, True), f"weather {weather} (upkeep)")
            if upkeep:
                self.text(upkeep)
        elif kind == "ability":
            self.emit(P.msg_weather(code, True), f"weather {weather}")
            of = tags.get("of")
            suffix = WEATHER_ABILITY_TEXT.get(P.to_id(what), WEATHER_ABILITY_TEXT.get(weather, "!"))
            if of:
                self.text(self.nm(of), "'s ", Text.ability(what), suffix)
            else:
                self.text(start)
        else:
            self.emit(P.msg_weather(code, False), f"weather {weather}")  # the move's animation already ran
            self.text(start)

    def team_text(self, side_ident: str) -> str:
        sid = side_ident[:2]
        return "Your team" if self.vside(sid) == 0 else f"{self.trainer(sid)}'s team"

    def on__sidestart(self, args):
        if not self.live:
            return
        sid, what = args[0], P.to_id(effect_name(args[1])[1])
        if what == "spikes":
            foe = "your team" if self.vside(sid[:2]) == 0 else "the foe's team"
            self.text(f"SPIKES were scattered all around {foe}'s feet!")
        elif what in SIDE_TEXT:
            self.text(self.team_text(sid), SIDE_TEXT[what][0])

    def on__sideend(self, args):
        if not self.live:
            return
        pos, tags = split_tags(args)
        sid, what = pos[0], P.to_id(effect_name(pos[1])[1])
        if what == "spikes":
            of = tags.get("of")
            if of:
                self.text(self.nm(of), " blew away SPIKES!")
        elif what in SIDE_TEXT:
            self.text(self.team_text(sid), SIDE_TEXT[what][1])

    def on__fieldactivate(self, args):
        if self.live and P.to_id(effect_name(args[0])[1]) == "perishsong":
            self.text("All affected POKéMON will faint in three turns!")

    # --- volatiles -------------------------------------------------------------

    def on__start(self, args):
        pos, tags = split_tags(args)
        ident = pos[0]
        what = P.to_id(effect_name(pos[1])[1])
        if not self.live:
            return
        vs = self.vside(ident[:2])
        n = self.nm(ident)
        if what == "substitute":
            self.text(n, " made a SUBSTITUTE!")
        elif what == "confusion":
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_CONFUSION"]), "confusion anim")
            self.text(n, " became confused due to fatigue!" if "fatigue" in tags else " became confused!")
        elif what == "leechseed":
            self.text(n, " was seeded!")
        elif what == "attract":
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_ATTRACT"]), "attract anim")
            self.text(n, " fell in love!")
        elif what == "taunt":
            self.text(n, " fell for the TAUNT!")
        elif what == "encore":
            self.text(n, " got an ENCORE!")
        elif what == "disable" and len(pos) > 2:
            self.text(n, "'s ", Text.move(pos[2]), " was disabled!")
        elif what == "curse":
            of = tags.get("of")
            self.text(self.nm(of) if of else Text("It"), " cut its own HP and laid a CURSE on ", n, "!")
        elif what == "nightmare":
            self.text(n, " began having a NIGHTMARE!")
        elif what == "yawn":
            self.text(n, " grew drowsy!")
        elif what == "focusenergy":
            self.text(n, " is getting pumped!")
        elif what in ("foresight", "odorsleuth"):
            self.text(n, " was identified!")
        elif what == "ingrain":
            self.text(n, " planted its roots!")
        elif what == "charge":
            self.text(n, " began charging power!")
        elif what == "stockpile1" or what.startswith("stockpile"):
            self.text(n, " stockpiled!")
        elif what.startswith("perish"):
            self.text(n, f"'s PERISH count fell to {what[-1]}!")
        elif what == "typechange" and len(pos) > 2:
            self.text(n, f" transformed into the {pos[2].upper()} type!")
        elif what in ("mist",):
            self.text(n, " became shrouded in MIST!")
        elif what == "bide":
            self.text(n, " is storing energy!")
        elif what == "uproar":
            self.text(n, " caused an UPROAR!")
        elif what == "lockon" or what == "mindreader":
            pass
        elif what == "flashfire":
            self.text("The power of ", n, "'s FIRE-type moves rose!")

    def on__end(self, args):
        pos, tags = split_tags(args)
        ident = pos[0]
        what = P.to_id(effect_name(pos[1])[1])
        if not self.live:
            return
        vs = self.vside(ident[:2])
        n = self.nm(ident)
        if what == "substitute":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_SUBSTITUTE_FADE"]), "substitute fade")
            self.text(n, "'s SUBSTITUTE faded!")
        elif what == "confusion":
            self.text(n, " snapped out of confusion!")
        elif what == "taunt":
            self.text(n, "'s taunt wore off!")
        elif what == "encore":
            self.text(n, "'s ENCORE ended!")
        elif what == "disable":
            self.text(n, " is no longer disabled!")
        elif what == "leechseed":
            of = tags.get("of")
            if of:
                self.text(self.nm(of), " shed LEECH SEED!")
        elif what == "uproar":
            self.text(n, " calmed down.")
        elif what in ("wrap", "bind", "firespin", "clamp", "whirlpool", "sandtomb", "partiallytrapped"):
            self.text(n, " was freed!")

    def on__activate(self, args):
        pos, tags = split_tags(args)
        ident = pos[0]
        what = P.to_id(effect_name(pos[1])[1]) if len(pos) > 1 else ""
        if not self.live:
            return
        vs = self.vside(ident[:2])
        n = self.nm(ident)
        if what == "substitute" and "damage" in tags:
            eff = self.direct_hit(ident)
            self.text("The SUBSTITUTE took damage for ", n, "!")
            self.after_hit(ident, eff)
            return
        self.flush_effectiveness()
        if what == "confusion":
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_CONFUSION"]), "confusion anim")
            self.text(n, " is confused!")
        elif what == "attract":
            of = tags.get("of")
            self.emit(P.msg_status_anim(vs, C["LV_VOLATILE_ATTRACT"]), "attract anim")
            self.text(n, " is in love with ", self.nm(of) if of else Text("the foe"), "!")
        elif what in ("protect", "detect"):
            self.text(n, " protected itself!")
        elif what == "endure":
            self.text(n, " endured the hit!")
        elif what == "destinybond":
            self.text(n, " took its attacker down with it!")
        elif what == "focusband":
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_FOCUS_BAND"]), "focus band anim")
            self.text(n, " hung on using its FOCUS BAND!")
        elif what in ("wrap", "bind", "firespin", "clamp", "whirlpool", "sandtomb"):
            of = tags.get("of")
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_TURN_TRAP"], 0), "trap anim")
            self.text(n, " is hurt by ", Text.move(pos[1]), "!" if not of else "!")
        elif what == "shedskin":
            self.text(n, "'s SHED SKIN cured its problem!")
        elif what in ("spite", "grudge") and len(pos) > 2:
            self.text("It reduced the PP of ", n, "'s ", Text.move(pos[2]), "!")
        elif what == "mimic" and len(pos) > 2:
            self.text(n, " learned ", Text.move(pos[2]), "!")
        elif what == "trick":
            of = tags.get("of")
            self.text(n, " switched items with its target!")
        elif what == "magnitude" and len(pos) > 2:
            self.text(f"MAGNITUDE {pos[2]}!")
        elif what == "lockon" or what == "mindreader":
            of = tags.get("of")
            self.text(n, " took aim at ", self.nm(of) if of else Text("the foe"), "!")

    def on__singleturn(self, args):
        pos, tags = split_tags(args)
        if not self.live:
            return
        n = self.nm(pos[0])
        what = P.to_id(effect_name(pos[1])[1])
        if what in ("protect", "detect"):
            self.text(n, " protected itself!")
        elif what == "endure":
            self.text(n, " braced itself!")
        elif what == "magiccoat":
            self.text(n, " shrouded itself in MAGIC COAT!")
        elif what == "snatch":
            self.text(n, " waits for a target to make a move!")
        elif what == "focuspunch":
            self.text(n, " is tightening its focus!")
        elif what == "followme":
            self.text(n, " became the center of attention!")

    def on__singlemove(self, args):
        pos, tags = split_tags(args)
        if not self.live:
            return
        n = self.nm(pos[0])
        what = P.to_id(effect_name(pos[1])[1])
        if what == "destinybond":
            self.text(n, " is trying to take its foe down with it!")
        elif what == "grudge":
            self.text(n, " wants the foe to bear a GRUDGE!")

    def on__mustrecharge(self, args):
        pass

    def on__transform(self, args):
        if self.live:
            target = self.mon(args[1])
            self.text(self.nm(args[0]), " transformed into ", Text.species(target.species), "!")

    def on__formechange(self, args):
        pos, tags = split_tags(args)
        if not self.live:
            return
        species = pos[1]
        if P.to_id(species).startswith("castform"):
            forms = {"castform": 0, "castformsunny": 1, "castformrainy": 2, "castformsnowy": 3}
            vs = self.vside(pos[0][:2])
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_CASTFORM_CHANGE"], forms.get(P.to_id(species), 0)), "castform")
            self.text(self.nm(pos[0]), " transformed!")

    # --- items and abilities ----------------------------------------------------

    def on__item(self, args):
        pos, tags = split_tags(args)
        if not self.live:
            return
        ident, item = pos[0], pos[1]
        kind, what = effect_name(tags.get("from", ""))
        wid = P.to_id(what)
        if wid in ("trick",):
            self.text(self.nm(ident), " obtained ", Text.item(item), "!")
        elif wid in ("thief", "covet"):
            of = tags.get("of")
            self.text(self.nm(ident), " stole ", self.nm(of) if of else Text("the foe"), "'s ", Text.item(item), "!")

    def on__enditem(self, args):
        pos, tags = split_tags(args)
        ident, item = pos[0], pos[1]
        m = self.mon(ident)
        if "eat" in tags:
            self.last_item[m.key] = item
            return
        if not self.live:
            return
        kind, what = effect_name(tags.get("from", ""))
        wid = P.to_id(what)
        if wid == "knockoff":
            of = tags.get("of")
            self.text(self.nm(of) if of else Text("The foe"), " knocked off ", self.nm(ident), "'s ", Text.item(item), "!")
        elif wid in ("thief", "covet", "trick"):
            pass
        elif "silent" not in tags and P.to_id(item) in ("whiteherb", "mentalherb"):
            vs = self.vside(ident[:2])
            self.emit(P.msg_anim(vs, P.ANIMS["B_ANIM_HELD_ITEM_EFFECT"]), "item anim")
            self.text(self.nm(ident), " used its ", Text.item(item), "!")

    def on__ability(self, args):
        pos, tags = split_tags(args)
        ident, ability = pos[0], pos[1]
        aid = P.to_id(ability)
        if aid == "intimidate":
            self.intimidate = self.nm(ident) if self.live else None
            return
        if not self.live or "silent" in tags:
            return
        kind, what = effect_name(tags.get("from", ""))
        if P.to_id(what) == "trace":
            of = tags.get("of")
            self.text(self.nm(ident), " TRACED ", self.nm(of) if of else Text("the foe"), "'s ", Text.ability(ability), "!")
        elif aid == "pressure":
            self.text(self.nm(ident), " is exerting its PRESSURE!")

    # --- misc -----------------------------------------------------------------

    def on__message(self, args):
        if self.live and args:
            self.text(args[0])
