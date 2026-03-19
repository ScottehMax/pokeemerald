"""Gymnasium environment wrapping the pokeemerald Gen III battle engine.

Single-agent: one side is controlled by the agent, the other by the built-in AI.
"""

from __future__ import annotations

from typing import Any

import gymnasium as gym
import numpy as np
from gymnasium import spaces

from pokeemerald_battle._ffi import (
    BATTLE_ACTION_FIGHT,
    BATTLE_ACTION_SWITCH,
    BATTLE_MAX_BATTLERS,
    BATTLE_MAX_MOVES,
    BATTLE_NUM_SIDES,
    BATTLE_NUM_STATS,
    BATTLE_OUTCOME_DREW,
    BATTLE_OUTCOME_LOST,
    BATTLE_OUTCOME_NONE,
    BATTLE_OUTCOME_WON,
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

# Action space: 10 discrete actions
#   0-3: use move in slot 0-3
#   4-9: switch to party mon 0-5
NUM_ACTIONS = BATTLE_MAX_MOVES + BATTLE_PARTY_SIZE


class PokemonBattleEnv(gym.Env):
    """A Gymnasium environment for Gen III Pokémon battles.

    The agent controls one side (default: player side 0), and the opponent
    is controlled by the built-in pokeemerald AI.

    Observation: dictionary with battle state arrays (active mons, party, field).
    Action: Discrete(10) — moves 0-3, switch to party slot 0-5.
    Invalid actions are masked via the `action_mask` key in the info dict.
    """

    metadata = {"render_modes": ["ansi"], "name": "PokemonBattle-v0"}

    def __init__(
        self,
        *,
        render_mode: str | None = None,
        agent_side: int = 0,
        doubles: bool = False,
        reward_shaping: bool = False,
        max_turns: int = 200,
        verbose: bool = False,
    ) -> None:
        super().__init__()
        self.render_mode = render_mode
        self.agent_side = agent_side
        self.doubles = doubles
        self.reward_shaping = reward_shaping
        self.max_turns = max_turns
        self.verbose = verbose

        self._lib = get_lib()
        self._state = BattleState()
        self._request = BattleActionRequest()
        self._prev_hp_frac: tuple[float, float] = (1.0, 1.0)

        # Action space: Discrete(10) with masking
        self.action_space = spaces.Discrete(NUM_ACTIONS)

        # Observation space: flat dictionary of arrays
        self.observation_space = spaces.Dict(
            {
                # Active mons (up to 4 battlers): species, hp, maxHp, stats, stages, status, moves, pp, types, ability, item, level
                "active_species": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_hp": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_max_hp": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS,), dtype=np.uint16),
                "active_stats": spaces.Box(0, 65535, shape=(BATTLE_MAX_BATTLERS, 5), dtype=np.uint16),  # atk/def/spd/spa/spd
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
                # Party (both sides)
                "party_species": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_hp": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_max_hp": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint16),
                "party_alive": spaces.Box(0, 1, shape=(BATTLE_NUM_SIDES, BATTLE_PARTY_SIZE), dtype=np.uint8),
                # Field
                "weather": spaces.Box(0, 65535, shape=(1,), dtype=np.uint16),
                "side_status": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES,), dtype=np.uint16),
                "spikes": spaces.Box(0, 3, shape=(BATTLE_NUM_SIDES,), dtype=np.uint8),
                # Meta
                "turn": spaces.Box(0, 255, shape=(1,), dtype=np.uint8),
                # Action mask
                "action_mask": spaces.Box(0, 1, shape=(NUM_ACTIONS,), dtype=np.uint8),
            }
        )

    def _get_obs(self) -> dict[str, np.ndarray]:
        self._lib.battle_get_state(self._state)
        s = self._state

        obs: dict[str, np.ndarray] = {}

        # Active mons
        obs["active_species"] = np.array([s.active[i].species for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint16)
        obs["active_hp"] = np.array([s.active[i].hp for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint16)
        obs["active_max_hp"] = np.array([s.active[i].maxHp for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint16)
        obs["active_stats"] = np.array(
            [[s.active[i].attack, s.active[i].defense, s.active[i].speed, s.active[i].spAttack, s.active[i].spDefense] for i in range(BATTLE_MAX_BATTLERS)],
            dtype=np.uint16,
        )
        obs["active_stat_stages"] = np.array([list(s.active[i].statStages) for i in range(BATTLE_MAX_BATTLERS)], dtype=np.int8)
        obs["active_status1"] = np.array([s.active[i].status1 for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint32)
        obs["active_status2"] = np.array([s.active[i].status2 for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint32)
        obs["active_moves"] = np.array([list(s.active[i].moves) for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint16)
        obs["active_pp"] = np.array([list(s.active[i].pp) for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint8)
        obs["active_types"] = np.array([list(s.active[i].types) for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint8)
        obs["active_ability"] = np.array([s.active[i].ability for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint8)
        obs["active_item"] = np.array([s.active[i].item for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint16)
        obs["active_level"] = np.array([s.active[i].level for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint8)
        obs["active_alive"] = np.array([s.active[i].isAlive for i in range(BATTLE_MAX_BATTLERS)], dtype=np.uint8)

        # Party
        obs["party_species"] = np.array([[s.party[side][j].species for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_hp"] = np.array([[s.party[side][j].hp for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_max_hp"] = np.array([[s.party[side][j].maxHp for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_alive"] = np.array([[s.party[side][j].isAlive for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint8)

        # Field
        obs["weather"] = np.array([s.weather], dtype=np.uint16)
        obs["side_status"] = np.array([s.sides[i].sideStatus for i in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["spikes"] = np.array([s.sides[i].spikesAmount for i in range(BATTLE_NUM_SIDES)], dtype=np.uint8)

        obs["turn"] = np.array([s.turn], dtype=np.uint8)

        # Action mask
        obs["action_mask"] = self._get_action_mask()

        return obs

    def _get_action_mask(self) -> np.ndarray:
        mask = np.zeros(NUM_ACTIONS, dtype=np.uint8)
        req = self._request

        if req.type == BATTLE_REQUEST_ACTION:
            # Per-move availability (PP, disabled, taunted, etc. checked by engine)
            any_move = False
            for j in range(BATTLE_MAX_MOVES):
                if req.availableMoves[j] != 0 and req.movePp[j] > 0:
                    mask[j] = 1
                    any_move = True
            # When all moves are unusable the engine will Struggle;
            # enable slot 0 so the agent can still pick "fight".
            if not any_move and req.canFight:
                mask[0] = 1
            # Can switch
            for j in range(BATTLE_PARTY_SIZE):
                if req.canSwitch[j]:
                    mask[BATTLE_MAX_MOVES + j] = 1
        elif req.type == BATTLE_REQUEST_MOVE:
            for j in range(BATTLE_MAX_MOVES):
                if req.availableMoves[j] != 0 and req.movePp[j] > 0:
                    mask[j] = 1
        elif req.type == BATTLE_REQUEST_SWITCH:
            for j in range(BATTLE_PARTY_SIZE):
                if req.canSwitch[j]:
                    mask[BATTLE_MAX_MOVES + j] = 1

        # Ensure at least one action is valid (shouldn't happen, but defensive)
        if mask.sum() == 0:
            mask[0] = 1

        return mask

    def _compute_hp_fractions(self) -> tuple[float, float]:
        """Sum HP fractions for each side."""
        fracs = [0.0, 0.0]
        for side in range(BATTLE_NUM_SIDES):
            total_hp = 0.0
            total_max = 0.0
            for j in range(BATTLE_PARTY_SIZE):
                mon = self._state.party[side][j]
                if mon.species != 0:
                    total_hp += mon.hp
                    total_max += mon.maxHp
            fracs[side] = total_hp / total_max if total_max > 0 else 0.0
        return (fracs[0], fracs[1])

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[dict[str, np.ndarray], dict[str, Any]]:
        super().reset(seed=seed)

        rng_seed = seed if seed is not None else self.np_random.integers(0, 2**31)

        # Reset or init the engine
        self._lib.battle_reset(rng_seed)

        # Configure
        config = BattleConfig()
        config.doubles = 1 if self.doubles else 0
        config.controlSide[0] = 1 if self.agent_side == 0 else 0
        config.controlSide[1] = 1 if self.agent_side == 1 else 0
        config.verbose = 1 if self.verbose else 0
        self._lib.battle_configure(config)

        # Random teams
        self._lib.battle_set_team_random(0, rng_seed)
        self._lib.battle_set_team_random(1, rng_seed + 1)

        # Start battle
        self._lib.battle_start()

        # Step until first decision
        rc = self._lib.battle_step()
        if rc == BATTLE_STEP_DECIDE:
            self._lib.battle_get_action_request(self._request)

        self._prev_hp_frac = self._compute_hp_fractions()

        obs = self._get_obs()
        info = {"action_mask": obs["action_mask"]}
        return obs, info

    def step(
        self, action: int
    ) -> tuple[dict[str, np.ndarray], float, bool, bool, dict[str, Any]]:
        # Convert discrete action to BattleAction
        act = BattleAction()
        if action < BATTLE_MAX_MOVES:
            act.type = BATTLE_ACTION_FIGHT
            act.moveSlot = action
            # Default target: first opponent
            act.target = 1 if self.agent_side == 0 else 0
        else:
            act.type = BATTLE_ACTION_SWITCH
            act.switchSlot = action - BATTLE_MAX_MOVES

        self._lib.battle_submit_action(act)

        # Advance to next decision or end
        rc = self._lib.battle_step()

        if rc == BATTLE_STEP_DECIDE:
            self._lib.battle_get_action_request(self._request)

        self._lib.battle_get_state(self._state)
        obs = self._get_obs()

        # Reward
        outcome = self._state.outcome
        terminated = outcome != BATTLE_OUTCOME_NONE
        truncated = self._state.turn >= self.max_turns

        reward = 0.0
        if terminated:
            if outcome == BATTLE_OUTCOME_WON:
                reward = 1.0 if self.agent_side == 0 else -1.0
            elif outcome == BATTLE_OUTCOME_LOST:
                reward = -1.0 if self.agent_side == 0 else 1.0
            # DREW → 0.0
        elif self.reward_shaping:
            cur_hp = self._compute_hp_fractions()
            agent = self.agent_side
            opponent = 1 - self.agent_side
            # Reward = opponent HP lost - agent HP lost
            reward = (self._prev_hp_frac[opponent] - cur_hp[opponent]) - (
                self._prev_hp_frac[agent] - cur_hp[agent]
            )
            self._prev_hp_frac = cur_hp

        info: dict[str, Any] = {"action_mask": obs["action_mask"]}
        if terminated:
            info["outcome"] = outcome

        return obs, reward, terminated, truncated, info

    def render(self) -> str | None:
        if self.render_mode == "ansi":
            self._lib.battle_get_state(self._state)
            lines = [f"Turn {self._state.turn} | Weather: {self._state.weather}"]
            for i in range(self._state.battlerCount):
                a = self._state.active[i]
                lines.append(
                    f"  Battler {i}: species={a.species} hp={a.hp}/{a.maxHp} "
                    f"status1=0x{a.status1:X} status2=0x{a.status2:X}"
                )
            return "\n".join(lines)
        return None

    def close(self) -> None:
        self._lib.battle_free()
