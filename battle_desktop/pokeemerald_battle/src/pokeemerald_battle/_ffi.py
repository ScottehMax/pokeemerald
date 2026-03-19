"""Low-level ctypes bindings to the pokeemerald battle engine shared library."""

from __future__ import annotations

import ctypes
import os
import platform
from ctypes import (
    POINTER,
    Structure,
    c_int8,
    c_uint8,
    c_uint16,
    c_uint32,
)
from pathlib import Path

# ---------------------------------------------------------------------------
# Constants (must match battle_api.h)
# ---------------------------------------------------------------------------
BATTLE_MAX_BATTLERS = 4
BATTLE_PARTY_SIZE = 6
BATTLE_MAX_MOVES = 4
BATTLE_NUM_SIDES = 2
BATTLE_NUM_STATS = 8

BATTLE_STEP_CONTINUE = 0
BATTLE_STEP_DECIDE = 1
BATTLE_STEP_DONE = 2

BATTLE_REQUEST_NONE = 0
BATTLE_REQUEST_ACTION = 1
BATTLE_REQUEST_MOVE = 2
BATTLE_REQUEST_SWITCH = 3
BATTLE_REQUEST_YES_NO = 4

BATTLE_ACTION_FIGHT = 0
BATTLE_ACTION_SWITCH = 1
BATTLE_ACTION_YES = 2
BATTLE_ACTION_NO = 3

BATTLE_OUTCOME_NONE = 0
BATTLE_OUTCOME_WON = 1
BATTLE_OUTCOME_LOST = 2
BATTLE_OUTCOME_DREW = 3

# ---------------------------------------------------------------------------
# ctypes Structure definitions (must match battle_api.h layout exactly)
# ---------------------------------------------------------------------------


class BattleActiveMon(Structure):
    _fields_ = [
        ("species", c_uint16),
        ("hp", c_uint16),
        ("maxHp", c_uint16),
        ("attack", c_uint16),
        ("defense", c_uint16),
        ("speed", c_uint16),
        ("spAttack", c_uint16),
        ("spDefense", c_uint16),
        ("statStages", c_int8 * BATTLE_NUM_STATS),
        ("status1", c_uint32),
        ("status2", c_uint32),
        ("moves", c_uint16 * BATTLE_MAX_MOVES),
        ("pp", c_uint8 * BATTLE_MAX_MOVES),
        ("types", c_uint8 * 2),
        ("ability", c_uint8),
        ("item", c_uint16),
        ("level", c_uint8),
        ("isAlive", c_uint8),
    ]


class BattlePartyMon(Structure):
    _fields_ = [
        ("species", c_uint16),
        ("hp", c_uint16),
        ("maxHp", c_uint16),
        ("item", c_uint16),
        ("status", c_uint32),
        ("level", c_uint8),
        ("moves", c_uint16 * BATTLE_MAX_MOVES),
        ("pp", c_uint8 * BATTLE_MAX_MOVES),
        ("types", c_uint8 * 2),
        ("ability", c_uint8),
        ("isAlive", c_uint8),
    ]


class BattleSideState(Structure):
    _fields_ = [
        ("sideStatus", c_uint16),
        ("reflectTimer", c_uint8),
        ("lightscreenTimer", c_uint8),
        ("mistTimer", c_uint8),
        ("safeguardTimer", c_uint8),
        ("spikesAmount", c_uint8),
    ]


BattlePartyRow = BattlePartyMon * BATTLE_PARTY_SIZE


class BattleState(Structure):
    _fields_ = [
        ("active", BattleActiveMon * BATTLE_MAX_BATTLERS),
        ("party", BattlePartyRow * BATTLE_NUM_SIDES),
        ("sides", BattleSideState * BATTLE_NUM_SIDES),
        ("weather", c_uint16),
        ("turn", c_uint8),
        ("battlerCount", c_uint8),
        ("outcome", c_uint8),
    ]


class BattleActionRequest(Structure):
    _fields_ = [
        ("type", c_uint8),
        ("battler", c_uint8),
        ("side", c_uint8),
        ("availableMoves", c_uint16 * BATTLE_MAX_MOVES),
        ("movePp", c_uint8 * BATTLE_MAX_MOVES),
        ("moveMaxPp", c_uint8 * BATTLE_MAX_MOVES),
        ("canSwitch", c_uint8 * BATTLE_PARTY_SIZE),
        ("numAlive", c_uint8),
        ("canFight", c_uint8),
        ("canStruggle", c_uint8),
        ("forced", c_uint8),
    ]


class BattleAction(Structure):
    _fields_ = [
        ("type", c_uint8),
        ("moveSlot", c_uint8),
        ("switchSlot", c_uint8),
        ("target", c_uint8),
    ]


class BattleConfig(Structure):
    _fields_ = [
        ("doubles", c_uint8),
        ("controlSide", c_uint8 * 2),
        ("verbose", c_uint8),
    ]


# ---------------------------------------------------------------------------
# Library loader
# ---------------------------------------------------------------------------

_lib: ctypes.CDLL | None = None


def _find_library() -> Path:
    """Find the battle_desktop shared library."""
    # Look relative to this file's location (inside the pokeemerald_battle package)
    pkg_dir = Path(__file__).resolve().parent
    battle_desktop_dir = pkg_dir.parent.parent.parent  # up from src/pokeemerald_battle/

    if platform.system() == "Windows":
        lib_name = "battle_desktop.dll"
    else:
        lib_name = "libbattle_desktop.so"

    # Check several candidate locations
    candidates = [
        battle_desktop_dir / lib_name,
        battle_desktop_dir / "battle_desktop" / lib_name,
        Path(os.environ.get("BATTLE_LIB_PATH", "")) / lib_name,
    ]

    for path in candidates:
        if path.is_file():
            return path

    msg = (
        f"Cannot find {lib_name}. Build it with 'make lib' in the battle_desktop/ directory, "
        f"or set BATTLE_LIB_PATH to the directory containing the library. "
        f"Searched: {[str(c) for c in candidates]}"
    )
    raise FileNotFoundError(msg)


def get_lib() -> ctypes.CDLL:
    """Load and return the shared library (cached)."""
    global _lib  # noqa: PLW0603
    if _lib is None:
        lib_path = _find_library()
        _lib = ctypes.CDLL(str(lib_path))
        _setup_prototypes(_lib)
    return _lib


def _setup_prototypes(lib: ctypes.CDLL) -> None:
    """Declare C function signatures for type safety."""
    lib.battle_init.argtypes = [c_uint32]
    lib.battle_init.restype = None

    lib.battle_free.argtypes = []
    lib.battle_free.restype = None

    lib.battle_reset.argtypes = [c_uint32]
    lib.battle_reset.restype = None

    lib.battle_configure.argtypes = [POINTER(BattleConfig)]
    lib.battle_configure.restype = None

    lib.battle_set_team_showdown.argtypes = [ctypes.c_int, ctypes.c_char_p]
    lib.battle_set_team_showdown.restype = ctypes.c_int

    lib.battle_set_team_random.argtypes = [ctypes.c_int, c_uint32]
    lib.battle_set_team_random.restype = None

    lib.battle_start.argtypes = []
    lib.battle_start.restype = None

    lib.battle_step.argtypes = []
    lib.battle_step.restype = ctypes.c_int

    lib.battle_get_state.argtypes = [POINTER(BattleState)]
    lib.battle_get_state.restype = None

    lib.battle_get_action_request.argtypes = [POINTER(BattleActionRequest)]
    lib.battle_get_action_request.restype = None

    lib.battle_get_outcome.argtypes = []
    lib.battle_get_outcome.restype = ctypes.c_int

    lib.battle_submit_action.argtypes = [POINTER(BattleAction)]
    lib.battle_submit_action.restype = None
