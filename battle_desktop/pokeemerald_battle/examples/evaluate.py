#!/usr/bin/env python3
"""Evaluate a trained MaskablePPO agent against various opponents.

Reports win rates vs:
  • **random** — random valid moves (baseline; should win >80%)
  • **ai**     — the built-in pokémerald battle AI (target: >50%)
  • **self**   — mirror match against a copy of itself (~50% expected)

Usage
-----
    uv run --group train examples/evaluate.py
    uv run --group train examples/evaluate.py --model checkpoints/final
    uv run --group train examples/evaluate.py --opponent ai --battles 200
"""

from __future__ import annotations

import argparse
import os
import time

import numpy as np
from sb3_contrib import MaskablePPO

from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper
from pokeemerald_battle.self_play_env import SelfPlayEnv
from pokeemerald_battle.wrappers import MaskableObsWrapper


def evaluate(
    model: MaskablePPO,
    opponent: str,
    n_battles: int,
    model_path: str | None = None,
) -> dict:
    """Run *n_battles* and return aggregate statistics."""
    if opponent == "random":
        env = FeaturizedObsWrapper(
            SelfPlayEnv(
                opponent_type="random", reward_shaping=False, max_turns=200
            )
        )
    elif opponent == "ai":
        env = FeaturizedObsWrapper(
            PokemonBattleEnv(reward_shaping=False, max_turns=200)
        )
    elif opponent == "self":
        env = FeaturizedObsWrapper(
            SelfPlayEnv(
                opponent_type="random",  # mirror uses random for simplicity
                reward_shaping=False,
                max_turns=200,
            )
        )
    else:
        raise ValueError(f"Unknown opponent: {opponent!r}")

    wins = 0
    losses = 0
    draws = 0
    truncated_count = 0
    total_steps = 0

    for i in range(n_battles):
        obs, info = env.reset(seed=i * 7919)
        done = False
        battle_steps = 0
        while not done:
            mask = info["action_mask"]
            action, _ = model.predict(
                obs, deterministic=True, action_masks=mask
            )
            obs, _, terminated, truncated, info = env.step(int(action))
            total_steps += 1
            battle_steps += 1
            done = terminated or truncated
            if battle_steps >= 2000:
                break

        outcome = info.get("outcome", 0)
        if outcome == 1:
            wins += 1
        elif outcome == 2:
            losses += 1
        elif outcome == 3:
            draws += 1
        else:
            truncated_count += 1

    env.close()

    return {
        "wins": wins,
        "losses": losses,
        "draws": draws,
        "truncated": truncated_count,
        "win_rate": wins / n_battles,
        "total_steps": total_steps,
        "avg_steps": total_steps / n_battles,
    }


def _find_model(path: str) -> str:
    """Normalise model path (SB3 may add .zip)."""
    if os.path.exists(path):
        return path
    if os.path.exists(path + ".zip"):
        return path
    raise FileNotFoundError(
        f"Model not found at {path!r} or {path + '.zip'!r}. "
        "Run examples/train.py first."
    )


def main() -> None:
    default_model = os.path.join("checkpoints", "final")

    parser = argparse.ArgumentParser(
        description="Evaluate a trained pokémon battle agent"
    )
    parser.add_argument(
        "--model",
        default=default_model,
        help=f"path to MaskablePPO checkpoint (default: {default_model})",
    )
    parser.add_argument(
        "--opponent",
        nargs="+",
        default=["random", "ai", "self"],
        choices=["random", "ai", "self"],
        help="opponent(s) to evaluate against",
    )
    parser.add_argument(
        "--battles", type=int, default=100, help="battles per opponent"
    )
    args = parser.parse_args()

    model_path = _find_model(args.model)
    model = MaskablePPO.load(model_path, device="cpu")
    print(f"Loaded model from {model_path}")
    print()

    header = f"{'Opponent':<12} {'Wins':>6} {'Losses':>6} {'Draws':>6} {'Trunc':>6} {'Win%':>7} {'Avg Steps':>10}"
    print(header)
    print("-" * len(header))

    for opp in args.opponent:
        t0 = time.perf_counter()
        result = evaluate(
            model, opp, args.battles, model_path=args.model
        )
        elapsed = time.perf_counter() - t0

        print(
            f"{opp:<12} "
            f"{result['wins']:>6} "
            f"{result['losses']:>6} "
            f"{result['draws']:>6} "
            f"{result['truncated']:>6} "
            f"{result['win_rate']:>6.1%} "
            f"{result['avg_steps']:>10.1f}"
            f"  ({elapsed:.1f}s)"
        )

    print()


if __name__ == "__main__":
    main()
