"""Quick smoke test for the featurized wrapper."""
from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper, TOTAL_OBS_SIZE
import numpy as np

env = FeaturizedObsWrapper(PokemonBattleEnv())
obs, info = env.reset(seed=42)
print(f"Feature vector shape: {obs.shape} (expected {TOTAL_OBS_SIZE})")
print(f"Range: [{obs.min():.3f}, {obs.max():.3f}]")
print(f"Action mask: {info['action_mask']}")
print(f"First 30 values: {obs[:30]}")

# Full episode test
obs2, r, term, trunc, info2 = env.step(0)
print(f"Step OK: shape={obs2.shape}, r={r:.3f}")

# Run 10 full episodes
wins = 0
for ep in range(10):
    obs, info = env.reset(seed=ep * 31)
    done = False
    steps = 0
    while not done:
        mask = info["action_mask"]
        valid = np.flatnonzero(mask)
        action = int(np.random.choice(valid))
        obs, r, term, trunc, info = env.step(action)
        done = term or trunc
        steps += 1
    if info.get("outcome") == 1:
        wins += 1
    print(f"  ep {ep}: steps={steps}, outcome={info.get('outcome')}")

env.close()
print(f"Featurized wrapper OK! ({wins}/10 wins)")
