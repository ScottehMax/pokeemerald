# pokeemerald-battle

Python bindings and RL environments for the `battle_desktop` C API extracted
from the pokeemerald Gen III battle engine.

The package exposes three layers:

- Low-level `ctypes` bindings for the C API in `battle_api.h`.
- A single-agent Gymnasium environment where your agent controls one side and
  the built-in battle AI controls the opponent.
- A PettingZoo AEC environment for two externally controlled sides, plus helper
  wrappers for self-play and MaskablePPO training.

## Requirements

- Python 3.12.6 or newer.
- `uv` for dependency management.
- A built `battle_desktop` shared library:
  - Windows: `battle_desktop.dll`
  - Linux/macOS: `libbattle_desktop.so`

From the parent `battle_desktop` directory, build the C shared library with:

```powershell
cd ..
make lib
```

The Python loader searches for the library in these locations:

1. The parent `battle_desktop` directory.
2. `battle_desktop/battle_desktop/`.
3. The directory named by `BATTLE_LIB_PATH`.

If the library is elsewhere, set `BATTLE_LIB_PATH` to the directory containing
the shared library:

```powershell
$env:BATTLE_LIB_PATH = "D:\path\to\library"
```

## Install

Install the package and runtime dependencies from this directory:

```powershell
uv sync
```

For training and evaluation scripts that use Stable-Baselines3 and
`sb3-contrib`, install the optional training dependency group:

```powershell
uv sync --group train
```

## Quick Start: Gymnasium

`PokemonBattleEnv` is the simplest interface. The agent controls one side
(`agent_side=0` by default), while the other side is controlled by the engine's
built-in AI.

```python
import numpy as np

from pokeemerald_battle import PokemonBattleEnv

env = PokemonBattleEnv(render_mode="ansi", reward_shaping=True)
rng = np.random.default_rng(42)

obs, info = env.reset(seed=12345)
done = False

while not done:
    mask = obs["action_mask"]
    valid_actions = np.flatnonzero(mask)
    action = int(rng.choice(valid_actions))

    obs, reward, terminated, truncated, info = env.step(action)
    done = terminated or truncated

print(env.render())
print("outcome:", info.get("outcome"))
env.close()
```

Run the included example:

```powershell
uv run examples/random_gymnasium.py
```

### Gymnasium Constructor

```python
PokemonBattleEnv(
    render_mode=None,
    agent_side=0,
    doubles=False,
    reward_shaping=False,
    max_turns=200,
    verbose=False,
    team_files=None,
    team_mix_rate=0.5,
)
```

- `agent_side`: side controlled by the Gymnasium agent, `0` or `1`.
- `doubles`: enables double battles when true.
- `reward_shaping`: adds non-terminal HP/KO shaping rewards. Terminal rewards
  are `+1` for a win, `-1` for a loss, and `0` for a draw.
- `max_turns`: truncates episodes at this turn count.
- `verbose`: forwards battle logs from the C engine.
- `team_files`: optional list of Pokemon Showdown-format team text files.
- `team_mix_rate`: probability of choosing a preset team instead of a random
  generated team when `team_files` is provided.

## Actions and Masks

The Gymnasium and PettingZoo environments use `spaces.Discrete(10)`:

- `0` to `3`: use move slot 0 to 3.
- `4` to `9`: switch to party slot 0 to 5.

Not every action is legal at every decision point. Use `action_mask` to select
only valid actions:

```python
mask = obs["action_mask"]          # Gymnasium raw observation
valid = np.flatnonzero(mask)
```

After wrappers that strip the mask from observations, read it from `info` or
call `env.action_masks()`:

```python
mask = info["action_mask"]
mask = env.action_masks()
```

## Observations

The raw environments return a dictionary of NumPy arrays with battle state:

- Active battlers: species, HP, stats, stat stages, statuses, moves, PP, types,
  ability, item, level, alive flag.
- Both parties: species, HP, moves, PP, types, ability, status, item, level,
  alive flag.
- Field state: weather, side conditions, spikes.
- Metadata: turn number.
- `action_mask`: valid action mask for the current decision.

The raw IDs are engine/game IDs. For neural network training, prefer
`FeaturizedObsWrapper`, which converts the raw dictionary into a flat
`float32` feature vector.

```python
from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper

env = FeaturizedObsWrapper(PokemonBattleEnv(reward_shaping=True))
obs, info = env.reset(seed=42)

print(obs.shape)              # (275,)
print(info["action_mask"])    # valid actions
```

Smoke-test the featurizer:

```powershell
uv run tools/test_featurize.py
```

## MaskablePPO Wrappers

`sb3-contrib`'s `MaskablePPO` expects observations without an embedded
`action_mask` and an `action_masks()` method on the environment.

Use:

- `MaskableObsWrapper` to keep raw dictionary observations but expose masks via
  `action_masks()`.
- `FeaturizedObsWrapper` to use a 275-element feature vector and expose masks
  via `action_masks()`.

Example:

```python
from sb3_contrib import MaskablePPO

from pokeemerald_battle import PokemonBattleEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper

env = FeaturizedObsWrapper(PokemonBattleEnv(reward_shaping=True))
model = MaskablePPO("MlpPolicy", env, verbose=1)
model.learn(total_timesteps=100_000)
```

## PettingZoo Self-Play

`PokemonBattlePettingZooEnv` exposes both sides as external agents using the
PettingZoo AEC API:

- `player_0`: side 0
- `player_1`: side 1

```python
import numpy as np

from pokeemerald_battle import PokemonBattlePettingZooEnv

env = PokemonBattlePettingZooEnv(render_mode="ansi")
rng = np.random.default_rng(99)

env.reset(seed=54321)

for agent in env.agent_iter():
    obs, reward, termination, truncation, info = env.last()

    if termination or truncation:
        action = None
    else:
        valid = np.flatnonzero(info["action_mask"])
        action = int(rng.choice(valid))

    env.step(action)
```

Run the included example:

```powershell
uv run examples/random_pettingzoo.py
```

For Gymnasium-style training against a random or checkpointed opponent, use
`SelfPlayEnv`. It exposes `player_0` as the learning agent and automatically
plays `player_1` until control returns to the learner.

```python
from pokeemerald_battle import SelfPlayEnv
from pokeemerald_battle.featurize import FeaturizedObsWrapper

env = FeaturizedObsWrapper(
    SelfPlayEnv(
        opponent_type="random",
        reward_shaping=True,
        max_turns=200,
    )
)
```

To use a frozen MaskablePPO checkpoint as the opponent:

```python
env = SelfPlayEnv(
    opponent_type="model",
    opponent_path="checkpoints/best/model",
    opponent_update_interval=10,
)
```

## Low-Level C API

The `_ffi` module mirrors the public C structs and functions from
`battle_api.h`. Use this layer when you need direct control over the battle
lifecycle.

```python
from pokeemerald_battle._ffi import (
    BATTLE_ACTION_FIGHT,
    BATTLE_STEP_DECIDE,
    BATTLE_STEP_DONE,
    BattleAction,
    BattleActionRequest,
    BattleConfig,
    BattleState,
    get_lib,
)

lib = get_lib()
lib.battle_init(42)

config = BattleConfig()
config.doubles = 0
config.controlSide[0] = 1
config.controlSide[1] = 0
config.verbose = 0
lib.battle_configure(config)

lib.battle_set_team_random(0, 42)
lib.battle_set_team_random(1, 43)
lib.battle_start()

state = BattleState()
request = BattleActionRequest()

while True:
    rc = lib.battle_step()
    if rc == BATTLE_STEP_DONE:
        break
    if rc == BATTLE_STEP_DECIDE:
        lib.battle_get_action_request(request)
        lib.battle_get_state(state)

        action = BattleAction()
        action.type = BATTLE_ACTION_FIGHT
        action.moveSlot = 0
        action.target = 1
        lib.battle_submit_action(action)

print("outcome:", lib.battle_get_outcome())
lib.battle_free()
```

Run the full low-level example:

```powershell
uv run examples/low_level_api.py
```

## Training and Evaluation

Train a MaskablePPO agent against the built-in AI:

```powershell
uv sync --group train
uv run --group train examples/train.py --n-envs 8 --total-steps 200000
```

The training script writes:

- Final model: `checkpoints/final.zip`
- Best model during evaluation: `checkpoints/best/model.zip`
- TensorBoard logs: `logs/`

Evaluate a trained checkpoint:

```powershell
uv run --group train examples/evaluate.py --model checkpoints/final --battles 100
```

Run TensorBoard:

```powershell
uv run --with tensorboard tensorboard --logdir logs/
```

Benchmark environment throughput:

```powershell
uv run examples/benchmark.py
uv run examples/benchmark.py --battles 500 --env gymnasium
uv run examples/benchmark.py --battles 500 --env pettingzoo
```

## Team Files

`PokemonBattleEnv` can mix random teams with preset Pokemon Showdown-format
team files:

```python
env = PokemonBattleEnv(
    team_files=[
        "../teams/team_a.txt",
        "../teams/team_b.txt",
    ],
    team_mix_rate=0.75,
)
```

With `team_mix_rate=0.75`, each side has a 75% chance of receiving a randomly
chosen preset team on reset; otherwise it receives a generated random team.

The training script can discover all `.txt` teams in a directory:

```powershell
uv run --group train examples/train.py --teams-dir ..\teams --team-mix-rate 0.75
```

## Troubleshooting

### `Cannot find battle_desktop.dll` or `Cannot find libbattle_desktop.so`

Build the C shared library from the parent directory:

```powershell
cd ..
make lib
```

If the library is not in a searched location, set `BATTLE_LIB_PATH` to the
directory containing it.

### Invalid actions or stuck policies

Always sample or predict using the current action mask. Legal actions change
with request type, PP, disabled moves, forced switches, and switch availability.

### Training imports fail

Install the training dependency group:

```powershell
uv sync --group train
```

### Multiprocessing training is slow to start

`examples/train.py` uses `SubprocVecEnv`. On Windows this can take a moment
because each worker imports the package and loads the C library independently.

## Public API

Top-level imports:

```python
from pokeemerald_battle import (
    BattleAction,
    BattleActionRequest,
    BattleConfig,
    BattleState,
    FeaturizedObsWrapper,
    MaskableObsWrapper,
    PokemonBattleEnv,
    PokemonBattlePettingZooEnv,
    SelfPlayEnv,
    get_lib,
)
```
