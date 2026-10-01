"""Train PPO from scratch on the same observations, skills and score as CEM."""
import argparse
import json
from pathlib import Path
from statistics import mean
import time
import numpy as np
import torch
from stable_baselines3 import PPO
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.vec_env import DummyVecEnv
from stable_baselines3.common.monitor import Monitor
from environment import ExpeditionEnv
from native import FEATURES, TRAIN_SEEDS, VALIDATION_SEEDS, rollout


class Validation(BaseCallback):
    def __init__(self, output, interval):
        super().__init__()
        self.output, self.interval = output, interval
        self.next_eval = interval
        self.best = float('-inf')
        self.history = []

    def evaluate(self):
        def policy(obs):
            return int(self.model.predict(np.asarray(obs, dtype=np.float32), deterministic=True)[0])
        results = [rollout(seed, policy) for seed in VALIDATION_SEEDS]
        score = mean(row['score'] for row in results)
        if score > self.best:
            self.best = score
            self.model.save(self.output)
        self.history.append(dict(steps=self.num_timesteps, validation_score=score, best_validation_score=self.best))
        print(f'PPO {self.num_timesteps}: validation {score:.1f}, best {self.best:.1f}', flush=True)

    def _on_step(self):
        return True

    def _on_rollout_end(self):
        if self.num_timesteps >= self.next_eval:
            self.evaluate()
            self.next_eval += self.interval

    def _on_training_end(self):
        self.evaluate()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--steps', type=int, default=131072)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--output', type=Path, default=Path('ai/models/ppo.zip'))
    parser.add_argument('--eval-every', type=int, default=16384)
    args = parser.parse_args()
    if args.steps < 1 or args.eval_every < 1:
        parser.error('steps and eval-every must be positive')
    torch.set_num_threads(1)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    env = DummyVecEnv([lambda: Monitor(ExpeditionEnv()) for _ in range(4)])
    # A skill has variable duration. gamma=1 optimizes undiscounted game score,
    # rather than introducing an arbitrary per-skill time preference.
    model = PPO('MlpPolicy', env, seed=args.seed, device='cpu', n_steps=256,
                batch_size=128, n_epochs=10, gamma=1.0, gae_lambda=0.95,
                learning_rate=3e-4, ent_coef=0.01,
                policy_kwargs=dict(net_arch=dict(pi=[64, 64], vf=[64, 64])), verbose=0)
    callback = Validation(args.output, args.eval_every)
    started = time.monotonic()
    try:
        model.learn(total_timesteps=args.steps, callback=callback)
        model.save(args.output.with_name(args.output.stem + '-last.zip'))
        metadata = dict(schema=1, algorithm='PPO', features=FEATURES, seed=args.seed,
                        requested_steps=args.steps, actual_steps=model.num_timesteps,
                        training_seeds=TRAIN_SEEDS, validation_seeds=VALIDATION_SEEDS,
                        gamma=1.0, n_envs=4, n_steps=256, batch_size=128, n_epochs=10,
                        learning_rate=3e-4, ent_coef=0.01, net_arch=[64, 64],
                        validation_score=callback.best, history=callback.history,
                        elapsed_seconds=time.monotonic() - started)
        args.output.with_suffix('.json').write_text(json.dumps(metadata, indent=2) + '\n')
    finally:
        env.close()
    print(f'Saved best validation checkpoint to {args.output}', flush=True)


if __name__ == '__main__':
    main()
