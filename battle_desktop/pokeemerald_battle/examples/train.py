#!/usr/bin/env python3
"""Train a Pokémon battle agent vs the in-game AI.

Uses featurized observations that encode move power, type effectiveness,
STAB, stat stages, HP fractions — everything a neural network needs to
learn competent play without memorising opaque integer IDs.

Usage
-----
    uv sync --group train
    uv run examples/train.py
    uv run examples/train.py --total-steps 1000000  # longer run
    uv run examples/train.py --n-envs 8 --total-steps 200000  # quick test
"""

from __future__ import annotations

import argparse
import glob
import os
import time

from sb3_contrib import MaskablePPO
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.vec_env import SubprocVecEnv, VecMonitor

from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper


def linear_schedule(initial: float, final: float = 0.0):
    """Linear schedule from *initial* to *final* over training."""

    def _schedule(progress: float) -> float:
        return final + progress * (initial - final)

    return _schedule

CHECKPOINT_DIR = "checkpoints"
LOG_DIR = "logs"


# ── env factory ──────────────────────────────────────────────────────────


def _make_env(rank: int, team_files: list[str] | None = None, team_mix_rate: float = 0.5):
    """Return a zero-arg callable that creates a FeaturizedObsWrapper env."""

    def _init():
        return FeaturizedObsWrapper(
            PokemonBattleEnv(
                reward_shaping=True,
                max_turns=200,
                team_files=team_files,
                team_mix_rate=team_mix_rate,
            )
        )

    return _init


def _make_eval_env(team_files: list[str] | None = None, team_mix_rate: float = 0.5):
    """Create a single eval env (no reward shaping)."""
    return FeaturizedObsWrapper(
        PokemonBattleEnv(
            reward_shaping=False,
            max_turns=200,
            team_files=team_files,
            team_mix_rate=team_mix_rate,
        )
    )


# ── callbacks ────────────────────────────────────────────────────────────


class EntCoefScheduleCallback(BaseCallback):
    """Linearly anneal ``ent_coef`` from *start* to *end* over training."""

    def __init__(self, start: float = 0.02, end: float = 0.002, verbose: int = 0):
        super().__init__(verbose)
        self._start = start
        self._end = end

    def _on_step(self) -> bool:
        progress = self.num_timesteps / self.model._total_timesteps
        self.model.ent_coef = self._end + (1 - progress) * (self._start - self._end)
        return True


class WinRateCallback(BaseCallback):
    """Evaluate the current policy vs the in-game AI periodically."""

    def __init__(
        self,
        eval_every_n_rollouts: int = 25,
        n_eval_battles: int = 100,
        verbose: int = 1,
        team_files: list[str] | None = None,
        team_mix_rate: float = 0.5,
    ):
        super().__init__(verbose)
        self.eval_every_n_rollouts = eval_every_n_rollouts
        self.n_eval_battles = n_eval_battles
        self._best_win_rate = -1.0
        self._rollout_count = 0
        self._eval_env: FeaturizedObsWrapper | None = None
        self._team_files = team_files
        self._team_mix_rate = team_mix_rate

    def _on_step(self) -> bool:
        return True

    def _on_rollout_end(self) -> None:
        self._rollout_count += 1
        if self._rollout_count % self.eval_every_n_rollouts != 0:
            return

        if self.verbose:
            print(
                f"\n  [eval] step {self.num_timesteps} "
                f"({self.n_eval_battles} battles)...",
                flush=True,
            )

        t0 = time.perf_counter()

        if self._eval_env is None:
            self._eval_env = _make_eval_env(team_files=self._team_files, team_mix_rate=self._team_mix_rate)

        wins = 0
        for i in range(self.n_eval_battles):
            obs, info = self._eval_env.reset(seed=i * 7919)
            done = False
            steps = 0
            while not done:
                mask = info["action_mask"]
                action, _ = self.model.predict(
                    obs, deterministic=True, action_masks=mask
                )
                obs, _, terminated, truncated, info = self._eval_env.step(
                    int(action)
                )
                done = terminated or truncated
                steps += 1
                if steps >= 2000:
                    break
            if info.get("outcome") == 1:
                wins += 1

        elapsed = time.perf_counter() - t0
        win_rate = wins / self.n_eval_battles
        self.logger.record("eval/win_rate_vs_ai", win_rate)
        if self.verbose:
            print(
                f"  [eval] win rate = {win_rate:.1%} "
                f"({wins}/{self.n_eval_battles}) "
                f"[{elapsed:.1f}s]"
            )
        if win_rate > self._best_win_rate:
            self._best_win_rate = win_rate
            os.makedirs(os.path.join(CHECKPOINT_DIR, "best"), exist_ok=True)
            self.model.save(os.path.join(CHECKPOINT_DIR, "best", "model"))
            if self.verbose:
                print(f"  [eval] new best! saved to {CHECKPOINT_DIR}/best/")

    def _on_training_end(self) -> None:
        if self._eval_env is not None:
            self._eval_env.close()
            self._eval_env = None


# ── main ─────────────────────────────────────────────────────────────────


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Train a pokémon battle agent vs in-game AI"
    )
    parser.add_argument(
        "--n-envs", type=int, default=8, help="parallel envs (SubprocVecEnv)"
    )
    parser.add_argument(
        "--total-steps", type=int, default=5_000_000, help="total training timesteps"
    )
    parser.add_argument(
        "--eval-rollouts",
        type=int,
        default=25,
        help="evaluate win rate every N rollouts",
    )
    parser.add_argument(
        "--eval-battles",
        type=int,
        default=100,
        help="number of battles per evaluation",
    )
    parser.add_argument(
        "--teams-dir",
        type=str,
        default=None,
        help="directory containing Showdown-format team .txt files",
    )
    parser.add_argument(
        "--team-mix-rate",
        type=float,
        default=0.5,
        help="probability of using preset teams vs random (default: 0.5)",
    )
    parser.add_argument(
        "--resume",
        type=str,
        default=None,
        help="path to MaskablePPO checkpoint to resume from (fine-tuning)",
    )
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()

    os.makedirs(CHECKPOINT_DIR, exist_ok=True)
    os.makedirs(LOG_DIR, exist_ok=True)

    # Discover team files
    team_files: list[str] | None = None
    if args.teams_dir:
        team_files = sorted(glob.glob(os.path.join(args.teams_dir, "*.txt")))
        if not team_files:
            print(f"Warning: no .txt files found in {args.teams_dir}")
            team_files = None
        else:
            print(f"Loaded {len(team_files)} team files from {args.teams_dir}")

    print("=" * 60)
    print("Training vs in-game AI (featurized observations)")
    print(f"  envs={args.n_envs}  steps={args.total_steps}")
    if team_files:
        print(f"  teams={len(team_files)} (mix rate {args.team_mix_rate:.0%})")
    if args.resume:
        print(f"  resume={args.resume}")
    print("=" * 60)

    env_fns = [_make_env(i, team_files=team_files, team_mix_rate=args.team_mix_rate) for i in range(args.n_envs)]
    vec_env = VecMonitor(
        SubprocVecEnv(env_fns), os.path.join(LOG_DIR, "train")
    )

    if args.resume:
        print(f"Resuming from {args.resume}")
        model = MaskablePPO.load(
            args.resume,
            env=vec_env,
            learning_rate=linear_schedule(3e-4, 3e-5),
            tensorboard_log=LOG_DIR,
            seed=args.seed,
        )
    else:
        model = MaskablePPO(
            "MlpPolicy",
            vec_env,
            learning_rate=linear_schedule(3e-4, 3e-5),
            n_steps=512,
            batch_size=1024,
            n_epochs=6,
            gamma=0.99,
            gae_lambda=0.95,
            clip_range=0.2,
            ent_coef=0.02,
            vf_coef=0.5,
            max_grad_norm=0.5,
            policy_kwargs={"net_arch": [512, 256, 128]},
            verbose=1,
            tensorboard_log=LOG_DIR,
            seed=args.seed,
        )

    callbacks = [
        EntCoefScheduleCallback(start=0.02, end=0.002),
        WinRateCallback(
            eval_every_n_rollouts=args.eval_rollouts,
            n_eval_battles=args.eval_battles,
            verbose=1,
            team_files=team_files,
            team_mix_rate=args.team_mix_rate,
        ),
    ]

    tb_name = "curriculum" if args.resume else "vs_ai"
    t0 = time.perf_counter()
    model.learn(
        total_timesteps=args.total_steps,
        callback=callbacks,
        tb_log_name=tb_name,
    )
    elapsed = time.perf_counter() - t0
    print(f"\nTraining done in {elapsed:.0f}s")

    model.save(os.path.join(CHECKPOINT_DIR, "final"))
    vec_env.close()

    print()
    print("=" * 60)
    print("Training complete!")
    print(f"  Final model  -> {os.path.join(CHECKPOINT_DIR, 'final.zip')}")
    print(f"  Best vs AI   -> {os.path.join(CHECKPOINT_DIR, 'best', 'model.zip')}")
    print()
    print("Evaluate with:  uv run examples/evaluate.py")
    print("TensorBoard:    uv run --with tensorboard tensorboard --logdir logs/")
    print("=" * 60)


if __name__ == "__main__":
    main()
