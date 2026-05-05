"""Evaluate final and best checkpoints vs AI and random."""
import numpy as np
from sb3_contrib import MaskablePPO
from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper
from pokeemerald_battle.self_play_env import SelfPlayEnv

N = 200

def run_eval(model, env_fn, n):
    wins, losses, draws, trunc = 0, 0, 0, 0
    for i in range(n):
        env = env_fn()
        obs, info = env.reset(seed=i * 7919)
        done = False
        steps = 0
        while not done:
            mask = info["action_mask"]
            action, _ = model.predict(obs, deterministic=True, action_masks=mask)
            obs, _, terminated, truncated, info = env.step(int(action))
            steps += 1
            done = terminated or truncated
            if steps > 500:
                break
        outcome = info.get("outcome", 0)
        if outcome == 1: wins += 1
        elif outcome == 2: losses += 1
        elif outcome == 3: draws += 1
        else: trunc += 1
        env.close()
    return wins, losses, draws, trunc

def ai_env():
    return FeaturizedObsWrapper(PokemonBattleEnv(reward_shaping=False, max_turns=200))

def random_env():
    return FeaturizedObsWrapper(SelfPlayEnv(opponent_type="random", reward_shaping=False, max_turns=200))

for ckpt in ["checkpoints/final", "checkpoints/best/model"]:
    print(f"\n=== {ckpt} ===")
    model = MaskablePPO.load(ckpt, device="cpu")
    
    w, l, d, t = run_eval(model, ai_env, N)
    print(f"  vs AI:     {w}W / {l}L / {d}D / {t}T = {w/N:.1%}")
    
    w, l, d, t = run_eval(model, random_env, N)
    print(f"  vs Random: {w}W / {l}L / {d}D / {t}T = {w/N:.1%}")

print("\nDone.")
