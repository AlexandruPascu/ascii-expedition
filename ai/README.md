# Learning to play ASCII Expedition

Two trained agents are included: a small **cross-entropy (CEM) weight policy** and a **PPO neural policy**. Both run against the real C++ game and choose which objective to pursue next. Shared, hand-written pathfinding moves the character and aims the blaster.

This first experiment covers **one generated expedition on Normal difficulty**, starting after the tutorials. It learns bonus collection and objective ordering. It does not learn movement, patrol evasion, or aiming from scratch, and it has not been evaluated on later stages or other difficulties.

## Watch them play

Build the game using the [main README](../README.md), then run from the repository root:

```sh
# CEM needs only Python's standard library (Python 3.8+).
python3 ai/watch.py --agent cem --seed 42
python3 ai/watch.py --agent nearest --seed 42
```

Use a terminal at least **80 columns by 33 rows**. Playback defaults to 2× speed; `--speed 1` uses real time. **Ctrl+C** stops playback and restores the terminal. The AI viewer has its own display and does not write to human high scores. Choose another `--seed` to generate a different map. `--headless` prints just the result.

For PPO, install the optional Python dependencies first. The recorded experiment used **Python 3.12**, CPU PyTorch 2.14.1, Gymnasium 1.3.0, and Stable Baselines3 2.9.0:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install torch==2.14.1 --index-url https://download.pytorch.org/whl/cpu
python -m pip install -r ai/requirements.txt
python ai/watch.py --agent ppo --seed 42
```

On Ubuntu, `python3-venv` may be needed to create an environment. On Windows, use `py -3.12 -m venv .venv`, then `.venv\Scripts\Activate.ps1`; subsequent commands use `python`. On macOS, install `torch==2.14.1` from the default pip index instead of the CPU index.

The Python binding finds the native shared library in `build/`, `build/Release/`, or `build/Debug/`. For a custom build directory, set `EXPEDITION_AI_LIBRARY` to the full path of `libexpedition_ai.so`, `libexpedition_ai.dylib`, or `expedition_ai.dll`.

## Results on unseen maps

Both models were frozen before evaluation on **128 test seeds, 30000–30127**. All agents use identical movement, observations, available skills, and game rules.

| Agent | Mean score | Change vs nearest star | Maps cleared | Mean game time | Crates broken |
| --- | ---: | ---: | ---: | ---: | ---: |
| Nearest star, then exit | 2,601 | — | 128/128 | 13.55 s | 0.00 |
| CEM weights | **3,699** | **+42.2%** | 128/128 | **18.58 s** | 5.11 |
| PPO | 3,626 | +39.4% | 128/128 | 27.05 s | 5.16 |

CEM wins this first comparison. Both learned to take profitable detours, but PPO used about eight more simulated seconds per map than CEM. CEM averaged 73 more points than PPO; a paired bootstrap across these maps gives a 95% interval of approximately 60–85 points in CEM's favor. This describes variation across maps for **one training run per method**, not variation across independent training runs or proof that CEM always outperforms PPO.

Every agent took zero hits. The generator guarantees safe routes, and the shared pathfinder excludes full patrol sweep and sentry pulse areas. The 100% completion rate is therefore largely supplied by the movement helper. The learned improvement is in scoring, not survival.

The [benchmark JSON](reports/benchmark.json) records model hashes, seed lists, means, score dispersion, and paired comparisons. The [per-map CSV](reports/benchmark.csv) contains all 384 episodes. [Installed package versions](reports/environment.txt) record the training environment. Results were obtained with GCC 13.3/libstdc++ on Linux; seeded layouts may differ between C++ standard libraries, and retraining PPO may differ with library versions or hardware.

## What the agents learn

At each decision, the native simulator offers nine skills:

1. Go to the nearest uncollected star.
2. Go to a star in a nearby cluster.
3. Go to the farthest uncollected star.
4. Approach and shoot the nearest reachable crate.
5. Collect the nearest extra life.
6. Collect the nearest time bonus.
7. Collect the nearest speed boost.
8. Collect the nearest shield.
9. Reach the unlocked exit.

If a skill has no available target, it becomes “nearest star,” or “exit” after all stars are collected. The observation describes that **actual fallback target**, so unavailable skills never become free waits. A skill runs until its target is collected/broken, the player is hit, or the game ends. Incidental pickups and exit contacts follow the normal game rules.

The helper uses breadth-first search with the player's complete 3×3 collision footprint. It knows walls, live crates, objectives, and patrol sweep geometry. It avoids patrol zones even while shielded. It uses double steps on straight routes when boosted, precise steps at turns, and ordinary blocked movement to aim at an adjacent crate. Each input advances **100 ms**, with the game's normal 20 ms collision substeps, timers, patrol motion, cooldowns, and pickup effects. Reaching the exit ends the game immediately, as in human play. Training and animated replay share this exact code.

Each of the nine targets has 12 features, giving a flat **108-value observation**:

| Feature | Definition |
| --- | --- |
| `distance` | Safe route length / 100 |
| `star`, `crate` | Target-type indicators |
| `heart` | 1 for a life pickup below maximum lives; 0.4 at maximum |
| `time` | Time-pickup indicator |
| `speed` | 1 for a speed pickup, reduced to 0.2 while already boosted |
| `shield`, `exit` | Target-type indicators |
| `nearby_stars` | Uncollected stars within Manhattan distance 16 of the target / 6 |
| `exit_distance` | Manhattan distance from the target to the exit / 100 |
| `deadline_fraction` | Route length / (10 × remaining seconds) |
| `remaining_stars` | Number of remaining stars / 6 |

The policy receives no map seed, RNG state, hidden crate drops, or hidden crate reward amount. It sees engineered objective features, not pixels or raw keypresses. `remaining_stars` is the same for every candidate, so its coefficient cancels in CEM's linear ranking; PPO can use it as context.

**CEM** scores each candidate with `sum(weight × feature)` and chooses the highest score. It samples 32 weight vectors per generation, evaluates each on the same 48 training maps for that generation, keeps the best six, and refits a smoothed diagonal Gaussian. Twelve generations were run with optimizer seed 7. A 64-map validation set chooses the saved generation winner. The entire search took about 49 seconds on the development machine; this is a local timing, not a performance guarantee.

The actual weights and generation history are in **[models/cem.json](models/cem.json)**. `features[i]` names `weights[i]`; there are no hidden overrides. To print them:

```sh
python -c "import json; d=json.load(open('ai/models/cem.json')); print(dict(zip(d['features'], d['weights'])))"
```

**PPO** uses [Stable Baselines3's PPO implementation](https://stable-baselines3.readthedocs.io/en/master/modules/ppo.html) and a [Gymnasium environment](https://gymnasium.farama.org/api/env/). Separate actor and critic networks each have two 64-unit hidden layers. It learned from scratch for 131,072 skill decisions across four environments; it did not imitate CEM or load its weights. The reward is **change in actual game score / 100**, with no bonus for firing, waiting, or repeated stuns. `gamma=1` preserves the undiscounted score objective despite variable skill durations. A 60-decision external cap is a truncation; death, in-game timeout, and completion are terminal states.

The saved **[PPO checkpoint](models/ppo.zip)** was selected by validation score. [Its metadata](models/ppo.json) records training settings, seed splits, timing, and validation history. Training took about 59 seconds locally. The last update's checkpoint is saved separately as `ppo-last.zip` and ignored by Git; the best validation checkpoint is used for evaluation and replay.

## Reproduce training and evaluation

Training seeds are **1000–1255**, validation seeds **20000–20063**, and test seeds **30000–30127**. CEM samples 48 training seeds per generation; PPO samples from the same 256-map pool. Only validation scores select checkpoints. The test set is reserved for the final comparison.

From the repository root, with the native library built and optional environment activated:

```sh
python ai/train_cem.py --generations 12 --population 32 --train-maps 48 --workers 4 --seed 7
python ai/train_ppo.py --steps 131072 --seed 7
python ai/evaluate.py
```

These commands replace the corresponding model files and benchmark report. To keep the included models while experimenting, use `--output ai/runs/cem.json` or `--output ai/runs/ppo.zip` during training, then `--model ai/runs/ppo.zip` when watching. `ai/evaluate.py` compares the included paths in `ai/models/`; it supports `--agents nearest cem` and `--split validation` for the initial CEM comparison.

## Checks and implementation

- `My first Game/AI/Agent.*` implements native skills, pathfinding, observations, fixed-time simulation, and replay frames. It calls the existing `Game` methods without rewriting the rules.
- `My first Game/AI/Bridge.cpp` exports a versioned C interface, loaded through Python's standard-library `ctypes`.
- `native.py` provides native episodes and CEM policy loading; `environment.py` adapts episodes to Gymnasium.
- `train_cem.py`, `train_ppo.py`, `evaluate.py`, and `watch.py` train, compare, and play back the agents.
- `ctest` covers 64 timed AI expeditions, native/animated parity, actual crate shots, boosts, repeatable seeds, invalid actions, Python bindings, and disjoint data splits. Native AI tests also run under ASan/UBSan.
- `python ai/smoke_test.py` checks Gymnasium compliance, runs small CEM/PPO training jobs, reloads the resulting models, replays both committed models, compares rendered and headless episodes with PyTorch already loaded, and checks terminal restoration after animated playback for **both CEM and PPO** on POSIX.

The standard game and CEM have no PyTorch dependency. The optional learning CI job installs CPU dependencies and runs the smoke check; full training is not repeated in CI.

### Linux compiler-runtime troubleshooting

If headless PPO succeeds but playback crashes while rendering the first frame, check the native library with `ldd build/libexpedition_ai.so`. This project encountered that failure with an extracted GCC installation whose `libstdc++.so` symlink was broken: the linker embedded a static C++ runtime while PyTorch loaded the shared runtime. Repairing the compiler's shared-runtime link and rebuilding resolved the conflict. A normal shared-runtime build lists `libstdc++.so.6` in `ldd` output.

After repairing that compiler installation, rebuild with `cmake --build build --clean-first --parallel 2` and run `python ai/smoke_test.py` using the PPO environment. The smoke check includes actual rendering and terminal playback; a `--headless` run alone cannot validate that path.
