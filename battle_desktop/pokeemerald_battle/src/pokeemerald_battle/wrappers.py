"""Wrappers to adapt battle environments for sb3-contrib's MaskablePPO."""

from __future__ import annotations

import gymnasium as gym
import numpy as np
from gymnasium import spaces


class MaskableObsWrapper(gym.Wrapper):
    """Separate ``action_mask`` from the observation dict.

    ``MaskablePPO`` expects:

    * An observation space that does **not** contain the action mask.
    * An ``action_masks()`` method that returns the current valid-action mask.

    This wrapper strips ``action_mask`` from observations returned by the
    inner :class:`PokemonBattleEnv` and exposes it via ``action_masks()``.
    """

    def __init__(self, env: gym.Env) -> None:
        super().__init__(env)
        self.observation_space = spaces.Dict(
            {k: v for k, v in env.observation_space.spaces.items() if k != "action_mask"}
        )
        self._last_mask = np.ones(env.action_space.n, dtype=np.uint8)

    def reset(self, **kwargs):  # type: ignore[override]
        obs, info = self.env.reset(**kwargs)
        self._last_mask = obs.pop("action_mask")
        info["action_mask"] = self._last_mask
        return obs, info

    def step(self, action):  # type: ignore[override]
        obs, reward, terminated, truncated, info = self.env.step(action)
        self._last_mask = obs.pop("action_mask")
        info["action_mask"] = self._last_mask
        return obs, reward, terminated, truncated, info

    def action_masks(self) -> np.ndarray:
        """Return the current action mask (called by ``MaskablePPO``)."""
        return self._last_mask
