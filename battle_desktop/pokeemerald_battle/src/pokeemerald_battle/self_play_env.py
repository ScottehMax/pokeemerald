"""Self-play Gymnasium wrapper around the PettingZoo battle environment.

Wraps the PettingZoo AEC environment so that one side (player_0) is exposed
as a standard Gymnasium agent, while the other side (player_1) is controlled
by either a random policy or a frozen MaskablePPO checkpoint loaded from disk.

For self-play curriculum training, save the learner's checkpoint periodically
and point *opponent_path* at it.  The env reloads the opponent model on reset.
"""

from __future__ import annotations

import os
from typing import Any

import gymnasium as gym
import numpy as np
from gymnasium import spaces

from pokeemerald_battle._ffi import (
    BATTLE_MAX_BATTLERS,
    BATTLE_MAX_MOVES,
    BATTLE_NUM_SIDES,
    BATTLE_NUM_STATS,
    BATTLE_PARTY_SIZE,
)
from pokeemerald_battle.env_pettingzoo import PokemonBattlePettingZooEnv

NUM_ACTIONS = BATTLE_MAX_MOVES + BATTLE_PARTY_SIZE


class SelfPlayEnv(gym.Env):
    """Gymnasium env for self-play Pokémon battles.

    *player_0* is the learning agent (Gymnasium interface).
    *player_1* is the opponent (random or frozen model).

    Parameters
    ----------
    opponent_type : ``"random"`` | ``"model"``
        How the opponent selects actions.
    opponent_path : str | None
        Path to a ``MaskablePPO`` checkpoint (without ``.zip`` suffix).
        Only used when *opponent_type* is ``"model"``.
    opponent_update_interval : int
        Re-check the checkpoint file every *N* ``reset()`` calls.
    reward_shaping : bool
        When ``True``, non-terminal steps return an HP-delta shaped reward
        (opponent HP lost − agent HP lost, as fractions).  Terminal steps
        always return +1 (win) / −1 (loss) / 0 (draw).
    """

    metadata = {"render_modes": ["ansi"], "name": "PokemonBattleSelfPlay-v0"}

    def __init__(
        self,
        *,
        render_mode: str | None = None,
        doubles: bool = False,
        reward_shaping: bool = True,
        max_turns: int = 200,
        opponent_type: str = "random",
        opponent_path: str | None = None,
        opponent_update_interval: int = 10,
        verbose: bool = False,
    ) -> None:
        super().__init__()
        self.render_mode = render_mode
        self.reward_shaping = reward_shaping
        self._opponent_type = opponent_type
        self._opponent_path = opponent_path
        self._opponent_update_interval = opponent_update_interval
        self._opponent_model: Any = None
        self._opponent_load_mtime: float = 0.0
        self._reset_count = 0

        self._pz_env = PokemonBattlePettingZooEnv(
            render_mode=render_mode,
            doubles=doubles,
            max_turns=max_turns,
            verbose=verbose,
        )

        self.agent_id = "player_0"
        self.opponent_id = "player_1"

        # --- spaces ---
        self.action_space = spaces.Discrete(NUM_ACTIONS)

        # Observation space is the same as the PettingZoo env **minus**
        # ``action_mask`` (provided separately via ``action_masks()``).
        self.observation_space = spaces.Dict(
            {
                "active_species": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_hp": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_max_hp": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_stats": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS, 5), dtype=np.uint16),
                "active_stat_stages": spaces.Box(-6, 6, shape=(BATTLE_MAX_BATTLERS, BATTLE_NUM_STATS), dtype=np.int8),
                "active_status1": spaces.Box(0, 2**32 - 1, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint32),
                "active_status2": spaces.Box(0, 2**32 - 1, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint32),
                "active_moves": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS, BATTLE_MAX_MOVES), dtype=np.uint16),
                "active_pp": spaces.Box(0, 255, shape=(BATTLE_MAX_BATTLERS, BATTLE_MAX_MOVES), dtype=np.uint8),
                "active_types": spaces.Box(0, 255, shape=(BATTLE_MAX_BATTLERS, 2), dtype=np.uint8),
                "active_ability": spaces.Box(0, 255, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint8),
                "active_item": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_level": spaces.Box(0, 255, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint8),
                "active_alive": spaces.Box(0, 1, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint8),
                "party_species": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_hp": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_max_hp": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_alive": spaces.Box(0, 1, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint8),
                "party_moves": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE, BATTLE_MAX_MOVES), dtype=np.uint16),
                "party_pp": spaces.Box(0, 255, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE, BATTLE_MAX_MOVES), dtype=np.uint8),
                "party_types": spaces.Box(0, 255, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE, 2), dtype=np.uint8),
                "party_ability": spaces.Box(0, 255, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint8),
                "party_level": spaces.Box(0, 255, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint8),
                "party_status": spaces.Box(0, 2**32 - 1, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint32),
                "party_item": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "weather": spaces.Box(0, 65535, shape=(1,), dtype=np.uint16),
                "side_status": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES,), dtype=np.uint16),
                "spikes": spaces.Box(0, 3, shape=(BATTLE_NUM_SIDES,), dtype=np.uint8),
                "turn": spaces.Box(0, 255, shape=(1,), dtype=np.uint8),
            }
        )

        self._rng = np.random.default_rng()
        self._prev_hp_frac: tuple[float, float] = (1.0, 1.0)
        self._last_mask = np.ones(NUM_ACTIONS, dtype=np.uint8)

    # ------------------------------------------------------------------
    # Opponent management
    # ------------------------------------------------------------------

    def _maybe_load_opponent(self) -> None:
        """Reload the frozen opponent if a newer checkpoint exists on disk."""
        if self._opponent_type != "model" or self._opponent_path is None:
            return
        # SB3 may or may not append .zip — check both variants
        for candidate in (self._opponent_path + ".zip", self._opponent_path):
            if os.path.exists(candidate):
                mtime = os.path.getmtime(candidate)
                if mtime > self._opponent_load_mtime:
                    from sb3_contrib import MaskablePPO

                    self._opponent_model = MaskablePPO.load(
                        self._opponent_path, device="cpu"
                    )
                    self._opponent_load_mtime = mtime
                return

    def _get_opponent_action(
        self, obs: dict[str, np.ndarray], mask: np.ndarray
    ) -> int:
        if self._opponent_type == "model" and self._opponent_model is not None:
            action, _ = self._opponent_model.predict(
                obs, deterministic=False, action_masks=mask
            )
            return int(action)
        # Fallback: uniform random over valid actions
        valid = np.flatnonzero(mask)
        return int(self._rng.choice(valid))

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _strip_mask(obs: dict[str, np.ndarray]) -> dict[str, np.ndarray]:
        return {k: v for k, v in obs.items() if k != "action_mask"}

    def _compute_hp_fractions(self) -> tuple[float, float]:
        state = self._pz_env._state
        self._pz_env._lib.battle_get_state(state)
        fracs = [0.0, 0.0]
        for side in range(BATTLE_NUM_SIDES):
            total_hp = 0.0
            total_max = 0.0
            for j in range(BATTLE_PARTY_SIZE):
                mon = state.party[side][j]
                if mon.species != 0:
                    total_hp += mon.hp
                    total_max += mon.maxHp
            fracs[side] = total_hp / total_max if total_max > 0 else 0.0
        return (fracs[0], fracs[1])

    def _is_done(self) -> bool:
        return all(self._pz_env.terminations.values()) or all(
            self._pz_env.truncations.values()
        )

    def _advance_to_agent(
        self,
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        """Play the opponent's turns until it is our agent's turn or the
        battle ends.  Returns a Gymnasium-style ``(obs, reward, term, trunc,
        info)`` tuple for the learning agent.
        """
        while not self._is_done():
            if self._pz_env.agent_selection == self.agent_id:
                break
            # --- opponent's turn ---
            _, _, term, trunc, info_opp = self._pz_env.last()
            if term or trunc:
                self._pz_env.step(None)
            else:
                mask = info_opp["action_mask"]
                opp_obs = self._strip_mask(self._pz_env.observe(self.opponent_id))
                self._pz_env.step(self._get_opponent_action(opp_obs, mask))

        # --- build agent response ---
        obs_full = self._pz_env.observe(self.agent_id)
        self._last_mask = obs_full["action_mask"]
        obs = self._strip_mask(obs_full)

        terminated = self._pz_env.terminations.get(self.agent_id, False)
        truncated = self._pz_env.truncations.get(self.agent_id, False)

        reward = 0.0
        if terminated or truncated:
            reward = self._pz_env.rewards.get(self.agent_id, 0.0)
        elif self.reward_shaping:
            cur_hp = self._compute_hp_fractions()
            # agent is side 0, opponent is side 1
            reward = (self._prev_hp_frac[1] - cur_hp[1]) - (
                self._prev_hp_frac[0] - cur_hp[0]
            )
            self._prev_hp_frac = cur_hp

        info: dict[str, Any] = {"action_mask": self._last_mask}
        if terminated:
            state = self._pz_env._state
            self._pz_env._lib.battle_get_state(state)
            info["outcome"] = state.outcome

        return obs, reward, terminated, truncated, info

    # ------------------------------------------------------------------
    # Gymnasium API
    # ------------------------------------------------------------------

    def action_masks(self) -> np.ndarray:
        """Return the current action mask (called by ``MaskablePPO``)."""
        return self._last_mask

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[dict[str, np.ndarray], dict[str, Any]]:
        super().reset(seed=seed)

        self._reset_count += 1
        if self._reset_count % self._opponent_update_interval == 0:
            self._maybe_load_opponent()

        if seed is not None:
            self._rng = np.random.default_rng(seed)

        self._pz_env.reset(seed=seed)
        self._prev_hp_frac = self._compute_hp_fractions()

        # If the opponent moves first, play their turn(s) automatically.
        if self._pz_env.agent_selection != self.agent_id and not self._is_done():
            obs, _, _, _, info = self._advance_to_agent()
        else:
            obs_full = self._pz_env.observe(self.agent_id)
            self._last_mask = obs_full["action_mask"]
            obs = self._strip_mask(obs_full)
            info = {"action_mask": self._last_mask}

        return obs, info

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        self._pz_env.step(action)
        return self._advance_to_agent()

    def render(self) -> str | None:
        return self._pz_env.render()

    def close(self) -> None:
        self._pz_env.close()
