"""Check the embedded policies against the original Python/PyTorch models."""
import hashlib
import json
import numpy as np
import torch
from stable_baselines3 import PPO
from native import ROOT, Episode, linear_action, load_weights, native_scores


def main():
    torch.set_num_threads(1)
    manifest = json.loads((ROOT / 'ai/models/native/manifest.json').read_text())
    for name, expected in manifest['sources'].items():
        assert hashlib.sha256((ROOT / 'ai/models' / name).read_bytes()).hexdigest() == expected
    for name, expected in manifest['exports'].items():
        assert hashlib.sha256((ROOT / 'ai/models/native' / name).read_bytes()).hexdigest() == expected
    cem = load_weights(ROOT / 'ai/models/cem.json')
    ppo = PPO.load(ROOT / 'ai/models/ppo.zip', device='cpu')
    compared = 0
    for difficulty in ('relaxed', 'normal', 'hard'):
        for seed in (40000, 40001, 40002, 40003):
            for agent in ('cem', 'ppo'):
                with Episode(seed, difficulty=difficulty, tutorial=True) as episode:
                    for stage in range(14):
                        while not episode.info()['done']:
                            observation = episode.observe()
                            native_cem = native_scores(observation, 'cem')
                            assert int(np.argmax(native_cem)) == linear_action(observation, cem)
                            native_ppo = native_scores(observation, 'ppo')
                            with torch.no_grad():
                                tensor = torch.tensor([observation], dtype=torch.float32)
                                reference = ppo.policy.action_net(ppo.policy.mlp_extractor.forward_actor(tensor))[0].numpy()
                            np.testing.assert_allclose(native_ppo, reference, rtol=2e-5, atol=3e-5)
                            assert int(np.argmax(native_ppo)) == int(np.argmax(reference))
                            episode.step(int(np.argmax(native_cem if agent == 'cem' else native_ppo)))
                            compared += 1
                        assert episode.info()['success'] == 1, (difficulty, seed, agent, stage)
                        if stage < 13:
                            episode.next_level()
    print(f'PASS original and embedded policies agree on {compared} decisions across 336 stages')


if __name__ == '__main__':
    main()
