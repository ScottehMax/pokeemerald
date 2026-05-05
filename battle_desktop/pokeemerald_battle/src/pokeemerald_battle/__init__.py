"""pokeemerald-battle: Gen III battle engine bindings for ML/RL."""

from pokeemerald_battle._ffi import (
    BattleAction,
    BattleActionRequest,
    BattleConfig,
    BattleState,
    get_lib,
)
from pokeemerald_battle.env_gymnasium import PokemonBattleEnv
from pokeemerald_battle.env_pettingzoo import PokemonBattlePettingZooEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper
from pokeemerald_battle.self_play_env import SelfPlayEnv
from pokeemerald_battle.wrappers import MaskableObsWrapper

__all__ = [
    "BattleAction",
    "BattleActionRequest",
    "BattleConfig",
    "BattleState",
    "FeaturizedObsWrapper",
    "MaskableObsWrapper",
    "PokemonBattleEnv",
    "PokemonBattlePettingZooEnv",
    "SelfPlayEnv",
    "get_lib",
]
