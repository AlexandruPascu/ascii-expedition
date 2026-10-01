"""Exercise Gym compliance, both trainers, checkpoint loading and terminal replay."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from stable_baselines3.common.env_checker import check_env
from environment import ExpeditionEnv
from native import ROOT, Episode
from policies import policy


def run(*args):
    return subprocess.check_output([sys.executable, *map(str, args)], cwd=ROOT, text=True)


def check_rendering():
    # Load PyTorch before the native library, as watch.py --agent ppo does.
    # Headless PPO and CEM-only animation missed a mixed C++ runtime crash here.
    for agent in ('ppo', 'cem'):
        select = policy(agent)
        with Episode(42) as bulk, Episode(42) as animated:
            while not bulk.info()['done']:
                action = select(bulk.observe())
                bulk.step(action)
                frames = list(animated.frames(action))
                assert frames and all(frame.startswith('#' * 80) for frame in frames)
                assert bulk.info() == animated.info(), (agent, bulk.info(), animated.info())
                assert bulk.observe() == animated.observe()
            assert animated.info()['success'] == 1
        print(f'PASS {agent.upper()} rendered replay matches headless play', flush=True)


def check_terminal(agent):
    import pty
    import select
    import time
    master, slave = pty.openpty()
    process = None
    chunks = []
    try:
        process = subprocess.Popen([sys.executable, '-X', 'faulthandler', 'ai/watch.py',
                                    '--agent', agent, '--speed', '100', '--seed', '42'],
                                   cwd=ROOT, stdout=slave, stderr=slave)
        os.close(slave)
        slave = None
        deadline = time.monotonic() + 30
        while True:
            if time.monotonic() > deadline:
                raise TimeoutError(f'{agent} terminal replay did not finish in 30 seconds')
            if not select.select([master], [], [], 1)[0]:
                continue
            try:
                chunk = os.read(master, 65536)
            except OSError as error:
                import errno
                if error.errno != errno.EIO:
                    raise
                break
            if not chunk:
                break
            chunks.append(chunk)
        output = b''.join(chunks)
        assert process.wait(timeout=5) == 0, (agent, output[-2000:])
        assert b'\x1b[?1049h' in output and b'\x1b[?1049l' in output
        assert b'\x1b[?25h' in output and b'"success": 1.0' in output
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        os.close(master)
        if slave is not None:
            os.close(slave)
    print(f'PASS {agent.upper()} animated terminal playback and restoration', flush=True)


def main():
    check_rendering()
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
        for agent in ('cem', 'ppo'):
            check_terminal(agent)
    print('PASS Gym contract, CEM/PPO training, saved-model replay and animated terminal playback')


if __name__ == '__main__':
    main()
