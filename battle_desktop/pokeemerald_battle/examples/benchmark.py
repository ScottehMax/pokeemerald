#!/usr/bin/env python3
"""Benchmark: measure battle throughput (battles/sec, steps/sec).

Runs many battles with random actions and no log output, reporting timing.

Usage:
    uv run examples/benchmark.py
    uv run examples/benchmark.py --battles 500 --env gymnasium
    uv run examples/benchmark.py --battles 500 --env pettingzoo
"""

import argparse
import time

import numpy as np

from pokeemerald_battle import PokemonBattleEnv, PokemonBattlePettingZooEnv


def bench_gymnasium(n_battles: int, seed: int) -> dict:
    env = PokemonBattleEnv(reward_shaping=False, verbose=False)
    rng = np.random.default_rng(seed)

    total_steps = 0
    outcomes = {1: 0, 2: 0, 3: 0, 0: 0}  # won/lost/drew/truncated

    t0 = time.perf_counter()
    for i in range(n_battles):
        obs, info = env.reset(seed=int(rng.integers(0, 2**31)))
        done = False
        while not done:
            mask = obs["action_mask"]
            valid = np.flatnonzero(mask)
            action = int(rng.choice(valid))
            obs, reward, terminated, truncated, info = env.step(action)
            total_steps += 1
            done = terminated or truncated
        outcomes[info.get("outcome", 0)] += 1
    elapsed = time.perf_counter() - t0

    return {
        "battles": n_battles,
        "total_steps": total_steps,
        "elapsed": elapsed,
        "battles_per_sec": n_battles / elapsed,
        "steps_per_sec": total_steps / elapsed,
        "avg_steps": total_steps / n_battles,
        "outcomes": outcomes,
    }


def bench_pettingzoo(n_battles: int, seed: int) -> dict:
    env = PokemonBattlePettingZooEnv(verbose=False)
    rng = np.random.default_rng(seed)

    total_steps = 0
    outcomes = {"player_0_wins": 0, "player_1_wins": 0, "draws": 0, "truncated": 0}

    t0 = time.perf_counter()
    for i in range(n_battles):
        env.reset(seed=int(rng.integers(0, 2**31)))
        battle_steps = 0
        final_rewards: dict[str, float] = {}
        for agent in env.agent_iter():
            obs, reward, termination, truncation, info = env.last()
            if termination or truncation:
                action = None
            else:
                mask = info["action_mask"]
                valid = np.flatnonzero(mask)
                action = int(rng.choice(valid))
            env.step(action)
            for a in env.possible_agents:
                final_rewards[a] = final_rewards.get(a, 0.0) + env.rewards.get(a, 0.0)
            battle_steps += 1
        total_steps += battle_steps

        r0 = final_rewards.get("player_0", 0.0)
        r1 = final_rewards.get("player_1", 0.0)
        if r0 > r1:
            outcomes["player_0_wins"] += 1
        elif r1 > r0:
            outcomes["player_1_wins"] += 1
        else:
            outcomes["draws"] += 1
    elapsed = time.perf_counter() - t0

    return {
        "battles": n_battles,
        "total_steps": total_steps,
        "elapsed": elapsed,
        "battles_per_sec": n_battles / elapsed,
        "steps_per_sec": total_steps / elapsed,
        "avg_steps": total_steps / n_battles,
        "outcomes": outcomes,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark pokeemerald battle engine")
    parser.add_argument("--battles", type=int, default=100, help="Number of battles to run")
    parser.add_argument("--env", choices=["gymnasium", "pettingzoo", "both"], default="both")
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    if args.env in ("gymnasium", "both"):
        print(f"Benchmarking Gymnasium env ({args.battles} battles)...")
        r = bench_gymnasium(args.battles, args.seed)
        print(f"  Elapsed:       {r['elapsed']:.2f}s")
        print(f"  Battles/sec:   {r['battles_per_sec']:.1f}")
        print(f"  Steps/sec:     {r['steps_per_sec']:.0f}")
        print(f"  Avg steps/bat: {r['avg_steps']:.1f}")
        print(f"  Outcomes:      W={r['outcomes'][1]} L={r['outcomes'][2]} D={r['outcomes'][3]} T={r['outcomes'][0]}")
        print()

    if args.env in ("pettingzoo", "both"):
        print(f"Benchmarking PettingZoo env ({args.battles} battles)...")
        r = bench_pettingzoo(args.battles, args.seed)
        print(f"  Elapsed:       {r['elapsed']:.2f}s")
        print(f"  Battles/sec:   {r['battles_per_sec']:.1f}")
        print(f"  Steps/sec:     {r['steps_per_sec']:.0f}")
        print(f"  Avg steps/bat: {r['avg_steps']:.1f}")
        print(f"  Outcomes:      {r['outcomes']}")


if __name__ == "__main__":
    main()
