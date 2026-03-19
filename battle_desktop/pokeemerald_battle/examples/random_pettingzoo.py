#!/usr/bin/env python3
"""Example: two random agents play each other via the PettingZoo AEC env.

Both sides are controlled externally (self-play). Each agent picks a
random valid action when it's their turn.

Usage:
    uv run examples/random_pettingzoo.py
"""

import numpy as np

from pokeemerald_battle import PokemonBattlePettingZooEnv

env = PokemonBattlePettingZooEnv(render_mode="ansi")
rng = np.random.default_rng(99)

env.reset(seed=54321)
print("=== Self-play battle started ===")
print(env.render())
print()

step = 0
final_rewards: dict[str, float] = {}

for agent in env.agent_iter():
    obs, reward, termination, truncation, info = env.last()

    if termination or truncation:
        action = None
    else:
        mask = info["action_mask"]
        valid_actions = np.flatnonzero(mask)
        action = int(rng.choice(valid_actions))

        if action < 4:
            print(f"Step {step} [{agent}]: use move slot {action}")
        else:
            print(f"Step {step} [{agent}]: switch to party slot {action - 4}")

    env.step(action)

    # Track cumulative rewards (rewards dict is cleared after dead-steps)
    for a in env.possible_agents:
        final_rewards[a] = final_rewards.get(a, 0.0) + env.rewards.get(a, 0.0)

    step += 1

    if step % 10 == 0:
        print(env.render())
        print()

print(f"\n=== Battle finished after {step} agent steps ===")
for agent in env.possible_agents:
    print(f"  {agent} reward: {final_rewards.get(agent, 0.0):.1f}")
