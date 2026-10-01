"""Load policies lazily: CEM and the baseline do not need ML dependencies."""
from native import ROOT, linear_action, load_weights


def policy(name, path=None):
    if name == 'nearest':
        return lambda obs: 0
    if name == 'cem':
        weights = load_weights(path or ROOT / 'ai/models/cem.json')
        return lambda obs: linear_action(obs, weights)
    if name == 'ppo':
        import numpy as np
        import torch
        from stable_baselines3 import PPO
        torch.set_num_threads(1)
        model = PPO.load(path or ROOT / 'ai/models/ppo.zip', device='cpu')
        return lambda obs: int(model.predict(np.asarray(obs, dtype=np.float32), deterministic=True)[0])
    raise ValueError('Choose nearest, cem or ppo')
