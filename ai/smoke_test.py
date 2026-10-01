"""Exercise Gym compliance, both trainers, checkpoint loading and terminal replay."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from stable_baselines3.common.env_checker import check_env
from environment import ExpeditionEnv
from native import ROOT


def run(*args):
    return subprocess.check_output([sys.executable, *map(str, args)], cwd=ROOT, text=True)


def main():
    env = ExpeditionEnv()
    try:
        check_env(env)
        a, _ = env.reset(seed=17)
        a, reward, terminated, truncated, info = env.step(0)
        assert reward == info['score'] / 100
        first = (a.tolist(), reward, terminated, truncated, info)
        env.reset(seed=17)
        b, reward, terminated, truncated, info = env.step(0)
        assert first == (b.tolist(), reward, terminated, truncated, info)
    finally:
        env.close()
    with tempfile.TemporaryDirectory() as temp:
        cem, ppo = Path(temp) / 'cem.json', Path(temp) / 'ppo.zip'
        print(run('ai/train_cem.py', '--generations', 1, '--population', 4,
                  '--train-maps', 2, '--workers', 1, '--output', cem))
        print(run('ai/train_ppo.py', '--steps', 1024, '--output', ppo))
        for agent, path in (('cem', cem), ('ppo', ppo)):
            first = json.loads(run('ai/watch.py', '--headless', '--agent', agent, '--model', path, '--seed', 17))
            second = json.loads(run('ai/watch.py', '--headless', '--agent', agent, '--model', path, '--seed', 17))
            assert first == second and first['success'] == 1
    # Both committed artifacts must also load and complete a fresh expedition.
    for agent in ('cem', 'ppo'):
        result = json.loads(run('ai/watch.py', '--headless', '--agent', agent, '--seed', 43))
        assert result['success'] == 1 and result['truncated'] == 0
    if os.name == 'posix':
        import pty
        master, slave = pty.openpty()
        try:
            process = subprocess.Popen([sys.executable, 'ai/watch.py', '--speed', '100', '--seed', '17'],
                                       cwd=ROOT, stdout=slave, stderr=slave)
            os.close(slave)
            slave = None
            chunks = []
            while True:
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    break
                if not chunk:
                    break
                chunks.append(chunk)
            assert process.wait(timeout=30) == 0
            output = b''.join(chunks)
            assert b'\x1b[?1049h' in output and b'\x1b[?1049l' in output
            assert b'\x1b[?25h' in output and b'"success": 1.0' in output
        finally:
            os.close(master)
            if slave is not None:
                os.close(slave)
    print('PASS Gym contract, CEM/PPO training, saved-model replay and animated terminal playback')


if __name__ == '__main__':
    main()
