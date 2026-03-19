"""PettingZoo AEC environment wrapping the pokémerald Gen III battle engine.

Multi-agent: both sides are controlled by external agents (self-play).
Uses the AEC (Agent-Environment-Cycle) API since the engine yields decisions
one battler at a time.
"""

from __future__ import annotations

from typing import Any

import gymnasium as gym
import numpy as np
from gymnasium import spaces
from pettingzoo import AECEnv
from pettingzoo.utils import agent_selector

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

NUM_ACTIONS = BATTLE_MAX_MOVES + BATTLE_PARTY_SIZE


def _build_obs_space() -> spaces.Dict:
    return spaces.Dict(
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
            "weather": spaces.Box(0, 65535, shape=(1,), dtype=np.uint16),
            "side_status": spaces.Box(0, 65535, shape=(BATTLE_NUM_SIDES,), dtype=np.uint16),
            "spikes": spaces.Box(0, 3, shape=(BATTLE_NUM_SIDES,), dtype=np.uint8),
            "turn": spaces.Box(0, 255, shape=(1,), dtype=np.uint8),
            "action_mask": spaces.Box(0, 1, shape=(NUM_ACTIONS,), dtype=np.uint8),
        }
    )


class PokemonBattlePettingZooEnv(AECEnv):
    """PettingZoo AEC environment for Gen III Pokémon battles (self-play).

    Both sides are controlled by external agents. The engine yields one
    decision at a time; the ``agent_selection`` property indicates whose
    turn it is. Agents take turns submitting actions.

    Agents: ``"player_0"`` (side 0) and ``"player_1"`` (side 1).
    """

    metadata = {"render_modes": ["ansi"], "name": "PokemonBattle-v0", "is_parallelizable": False}

    def __init__(
        self,
        *,
        render_mode: str | None = None,
        doubles: bool = False,
        max_turns: int = 200,
        verbose: bool = False,
    ) -> None:
        super().__init__()
        self.render_mode = render_mode
        self.doubles = doubles
        self.max_turns = max_turns
        self.verbose = verbose

        self.possible_agents = ["player_0", "player_1"]
        self.agents = list(self.possible_agents)
        self._agent_to_side = {"player_0": 0, "player_1": 1}

        self._lib = get_lib()
        self._state = BattleState()
        self._request = BattleActionRequest()

        # Per-agent spaces
        obs_space = _build_obs_space()
        act_space = spaces.Discrete(NUM_ACTIONS)
        self.observation_spaces = {a: obs_space for a in self.possible_agents}
        self.action_spaces = {a: act_space for a in self.possible_agents}

        # AEC bookkeeping
        self.rewards: dict[str, float] = {a: 0.0 for a in self.possible_agents}
        self.terminations: dict[str, bool] = {a: False for a in self.possible_agents}
        self.truncations: dict[str, bool] = {a: False for a in self.possible_agents}
        self.infos: dict[str, dict[str, Any]] = {a: {} for a in self.possible_agents}
        self._cumulative_rewards: dict[str, float] = {a: 0.0 for a in self.possible_agents}

    def observation_space(self, agent: str) -> spaces.Space:
        return self.observation_spaces[agent]

    def action_space(self, agent: str) -> spaces.Space:
        return self.action_spaces[agent]

    # ---- internal helpers ----

    def _get_obs(self) -> dict[str, np.ndarray]:
        """Build observation dict from current engine state."""
        self._lib.battle_get_state(self._state)
        s = self._state
        obs: dict[str, np.ndarray] = {}

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

        obs["party_species"] = np.array([[s.party[side][j].species for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_hp"] = np.array([[s.party[side][j].hp for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_max_hp"] = np.array([[s.party[side][j].maxHp for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["party_alive"] = np.array([[s.party[side][j].isAlive for j in range(BATTLE_PARTY_SIZE)] for side in range(BATTLE_NUM_SIDES)], dtype=np.uint8)

        obs["weather"] = np.array([s.weather], dtype=np.uint16)
        obs["side_status"] = np.array([s.sides[i].sideStatus for i in range(BATTLE_NUM_SIDES)], dtype=np.uint16)
        obs["spikes"] = np.array([s.sides[i].spikesAmount for i in range(BATTLE_NUM_SIDES)], dtype=np.uint8)
        obs["turn"] = np.array([s.turn], dtype=np.uint8)

        obs["action_mask"] = self._get_action_mask()
        return obs

    def _get_action_mask(self) -> np.ndarray:
        mask = np.zeros(NUM_ACTIONS, dtype=np.uint8)
        req = self._request

        if req.type == BATTLE_REQUEST_ACTION:
            any_move = False
            for j in range(BATTLE_MAX_MOVES):
                if req.availableMoves[j] != 0 and req.movePp[j] > 0:
                    mask[j] = 1
                    any_move = True
            # When all moves are unusable the engine will Struggle;
            # enable slot 0 so the agent can still pick "fight".
            if not any_move and req.canFight:
                mask[0] = 1
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

        if mask.sum() == 0:
            mask[0] = 1

        return mask

    def _side_to_agent(self, side: int) -> str:
        return f"player_{side}"

    def _advance_engine(self) -> int:
        """Run battle_step until a decision or end."""
        rc = self._lib.battle_step()
        if rc == BATTLE_STEP_DECIDE:
            self._lib.battle_get_action_request(self._request)
        return rc

    def _update_agents_state(self) -> None:
        """Set terminations/truncations based on current engine state."""
        self._lib.battle_get_state(self._state)
        outcome = self._state.outcome
        turn = self._state.turn

        if outcome != BATTLE_OUTCOME_NONE:
            for agent in self.agents:
                self.terminations[agent] = True
                side = self._agent_to_side[agent]
                if outcome == BATTLE_OUTCOME_WON:
                    self.rewards[agent] = 1.0 if side == 0 else -1.0
                elif outcome == BATTLE_OUTCOME_LOST:
                    self.rewards[agent] = -1.0 if side == 0 else 1.0
                else:
                    self.rewards[agent] = 0.0
        elif turn >= self.max_turns:
            for agent in self.agents:
                self.truncations[agent] = True
                self.rewards[agent] = 0.0

    # ---- AEC API ----

    def reset(self, seed: int | None = None, options: dict[str, Any] | None = None) -> None:
        rng = np.random.default_rng(seed)
        rng_seed = int(rng.integers(0, 2**31))

        self.agents = list(self.possible_agents)
        self.rewards = {a: 0.0 for a in self.agents}
        self.terminations = {a: False for a in self.agents}
        self.truncations = {a: False for a in self.agents}
        self.infos = {a: {} for a in self.agents}
        self._cumulative_rewards = {a: 0.0 for a in self.agents}

        # Reset engine
        self._lib.battle_reset(rng_seed)

        config = BattleConfig()
        config.doubles = 1 if self.doubles else 0
        config.controlSide[0] = 1
        config.controlSide[1] = 1
        config.verbose = 1 if self.verbose else 0
        self._lib.battle_configure(config)

        self._lib.battle_set_team_random(0, rng_seed)
        self._lib.battle_set_team_random(1, rng_seed + 1)
        self._lib.battle_start()

        rc = self._advance_engine()
        self._update_agents_state()

        if rc == BATTLE_STEP_DECIDE:
            side = self._request.side
            self.agent_selection = self._side_to_agent(side)
        else:
            self.agent_selection = self.agents[0]

        obs = self._get_obs()
        mask = obs["action_mask"]
        for agent in self.agents:
            self.infos[agent] = {"action_mask": mask}

    def observe(self, agent: str) -> dict[str, np.ndarray]:
        obs = self._get_obs()
        return obs

    def step(self, action: int | None) -> None:
        if (
            self.terminations.get(self.agent_selection, False)
            or self.truncations.get(self.agent_selection, False)
        ):
            self._was_dead_step(action)
            return

        current_agent = self.agent_selection
        current_side = self._agent_to_side[current_agent]

        # Convert discrete action to BattleAction
        if action is not None:
            act = BattleAction()
            if action < BATTLE_MAX_MOVES:
                act.type = BATTLE_ACTION_FIGHT
                act.moveSlot = action
                act.target = 1 if current_side == 0 else 0
            else:
                act.type = BATTLE_ACTION_SWITCH
                act.switchSlot = action - BATTLE_MAX_MOVES

            self._lib.battle_submit_action(act)

        # Zero rewards before stepping
        self._cumulative_rewards = {a: 0.0 for a in self.agents}
        self.rewards = {a: 0.0 for a in self.agents}

        # Advance engine
        rc = self._advance_engine()
        self._update_agents_state()

        # Set next agent
        if rc == BATTLE_STEP_DECIDE and not all(self.terminations.values()):
            side = self._request.side
            self.agent_selection = self._side_to_agent(side)
        elif self.agents:
            # Battle done, cycle to first agent for dead-step handling
            self.agent_selection = self.agents[0]

        # Accumulate rewards
        self._accumulate_rewards()

        obs = self._get_obs()
        mask = obs["action_mask"]
        for agent in self.agents:
            self.infos[agent] = {"action_mask": mask}

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
        pass
