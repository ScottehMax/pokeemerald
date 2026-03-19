#!/usr/bin/env python3
"""Example: use the low-level C API directly via ctypes.

Demonstrates the full battle lifecycle without Gymnasium/PettingZoo —
useful for custom training loops, debugging, or understanding the API.

Usage:
    uv run examples/low_level_api.py
"""

import random

from pokeemerald_battle._ffi import (
    BATTLE_ACTION_FIGHT,
    BATTLE_ACTION_SWITCH,
    BATTLE_MAX_MOVES,
    BATTLE_OUTCOME_NONE,
    BATTLE_PARTY_SIZE,
    BATTLE_REQUEST_ACTION,
    BATTLE_REQUEST_MOVE,
    BATTLE_REQUEST_SWITCH,
    BATTLE_STEP_DECIDE,
    BATTLE_STEP_DONE,
    BattleAction,
    BattleActionRequest,
    BattleConfig,
    BattleState,
    get_lib,
)

lib = get_lib()

# --- Initialise ---
seed = 42
lib.battle_init(seed)

# --- Configure: control side 0 (player), AI controls side 1 ---
config = BattleConfig()
config.doubles = 0
config.controlSide[0] = 1  # we control player
config.controlSide[1] = 0  # AI controls opponent
lib.battle_configure(config)

# --- Set random teams ---
lib.battle_set_team_random(0, seed)
lib.battle_set_team_random(1, seed + 1)

# --- Start ---
lib.battle_start()
print("Battle started!")

state = BattleState()
request = BattleActionRequest()
turn = 0

while True:
    rc = lib.battle_step()

    if rc == BATTLE_STEP_DONE:
        break

    if rc == BATTLE_STEP_DECIDE:
        lib.battle_get_action_request(request)
        lib.battle_get_state(state)

        action = BattleAction()

        if request.type == BATTLE_REQUEST_ACTION:
            # Randomly choose fight or switch
            can_switch = any(request.canSwitch[j] for j in range(BATTLE_PARTY_SIZE))
            if can_switch and random.random() < 0.2:
                action.type = BATTLE_ACTION_SWITCH
                # Pick a random valid switch target
                valid = [j for j in range(BATTLE_PARTY_SIZE) if request.canSwitch[j]]
                action.switchSlot = random.choice(valid)
                print(f"  Turn {state.turn}: switch to party slot {action.switchSlot}")
            else:
                action.type = BATTLE_ACTION_FIGHT
                print(f"  Turn {state.turn}: choose to fight")

        elif request.type == BATTLE_REQUEST_MOVE:
            # Pick a random move with PP
            valid = [
                j for j in range(BATTLE_MAX_MOVES)
                if request.availableMoves[j] != 0 and request.movePp[j] > 0
            ]
            slot = random.choice(valid) if valid else 0
            action.type = BATTLE_ACTION_FIGHT
            action.moveSlot = slot
            action.target = 1  # opponent battler
            print(f"  Turn {state.turn}: use move slot {slot} (move #{request.availableMoves[slot]})")

        elif request.type == BATTLE_REQUEST_SWITCH:
            valid = [j for j in range(BATTLE_PARTY_SIZE) if request.canSwitch[j]]
            action.type = BATTLE_ACTION_SWITCH
            action.switchSlot = random.choice(valid) if valid else 0
            print(f"  Turn {state.turn}: forced switch to party slot {action.switchSlot}")

        lib.battle_submit_action(action)

# --- Result ---
outcome = lib.battle_get_outcome()
outcome_names = {1: "WON", 2: "LOST", 3: "DREW"}
lib.battle_get_state(state)
print(f"\nBattle finished on turn {state.turn}: {outcome_names.get(outcome, 'UNKNOWN')}")

# --- Cleanup ---
lib.battle_free()
