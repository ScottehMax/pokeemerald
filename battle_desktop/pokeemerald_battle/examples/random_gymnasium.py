#!/usr/bin/env python3
"""Example: run a single battle using the Gymnasium environment.

The agent picks a random *valid* action each turn (action masking ensures
it never tries an illegal move or switch). The built-in pokémerald AI
controls the opponent.

Usage:
    uv run examples/random_gymnasium.py
"""

import numpy as np

from pokeemerald_battle import PokemonBattleEnv

env = PokemonBattleEnv(render_mode="ansi", reward_shaping=True, verbose=True)
rng = np.random.default_rng(42)

obs, info = env.reset(seed=12345)
print("=== Battle started ===")
print(env.render())
print()

total_reward = 0.0
step = 0

while True:
    # Use the action mask to pick only legal actions
    mask = obs["action_mask"]
    valid_actions = np.flatnonzero(mask)
    action = rng.choice(valid_actions)

    # Describe the action
    if action < 4:
        move_id = obs["active_moves"][env.agent_side][action]
        print(f"Step {step}: use move slot {action} (move #{move_id})")
    else:
        slot = action - 4
        print(f"Step {step}: switch to party slot {slot}")

    obs, reward, terminated, truncated, info = env.step(action)
    total_reward += reward
    step += 1

    if step % 5 == 0:
        print(env.render())
        print()

    if terminated or truncated:
        break

outcome_names = {1: "WON", 2: "LOST", 3: "DREW"}
outcome = info.get("outcome", 0)
print(f"\n=== Battle finished after {step} steps ===")
print(f"Outcome: {outcome_names.get(outcome, 'TRUNCATED')}")
print(f"Total reward: {total_reward:.4f}")
