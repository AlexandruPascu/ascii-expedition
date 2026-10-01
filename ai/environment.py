"""Gymnasium objective-selection environment on the native C++ rules."""
import gymnasium as gym
import numpy as np
from native import ACTIONS, FEATURES, Episode, TRAIN_SEEDS


class ExpeditionEnv(gym.Env):
    metadata = {'render_modes': ['ansi'], 'render_fps': 10}

    def __init__(self, render_mode=None):
        super().__init__()
        if render_mode not in (None, 'ansi'):
            raise ValueError('Supported render mode: ansi')
        self.render_mode = render_mode
        self.action_space = gym.spaces.Discrete(len(ACTIONS))
        self.observation_space = gym.spaces.Box(0, np.inf, (len(ACTIONS) * len(FEATURES),), np.float32)
        self.episode = None
        self.previous_score = 0

    def reset(self, *, seed=None, options=None):
        super().reset(seed=seed)
        self.close()
        # Gym seed seeds the training sampler; explicit map_seed is for evaluation/replay.
        map_seed = (options or {}).get('map_seed')
        if map_seed is None:
            map_seed = int(self.np_random.choice(TRAIN_SEEDS))
        self.episode = Episode(map_seed)
        self.previous_score = 0
        return np.asarray(self.episode.observe(), dtype=np.float32), self.episode.info()

    def step(self, action):
        if self.episode is None:
            raise RuntimeError('Call reset before step')
        info = self.episode.step(int(action))
        reward = (info['score'] - self.previous_score) / 100.0
        self.previous_score = info['score']
        truncated = bool(info['truncated'])
        terminated = bool(info['done']) and not truncated
        return np.asarray(self.episode.observe(), dtype=np.float32), reward, terminated, truncated, info

    def render(self):
        return self.episode.render() if self.episode else ''

    def close(self):
        if self.episode is not None:
            self.episode.close()
            self.episode = None
