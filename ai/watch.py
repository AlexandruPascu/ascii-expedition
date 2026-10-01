"""Watch a trained agent play, using the same native ticks as training."""
import argparse
import json
from pathlib import Path
import sys
import time
from native import ACTIONS, Episode
from policies import policy


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agent', choices=('nearest', 'cem', 'ppo'), default='cem')
    parser.add_argument('--model', type=Path)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--speed', type=float, default=2, help='Playback speed (1 = real time)')
    parser.add_argument('--headless', action='store_true', help='Print only the result; no terminal required')
    args = parser.parse_args()
    if args.speed <= 0 or args.speed > 100:
        parser.error('speed must be in (0, 100]')
    if not 0 <= args.seed <= 0xffffffff:
        parser.error('seed must be in 0..4294967295')
    if not args.headless and not sys.stdout.isatty():
        parser.error('Open an interactive terminal, or use --headless')
    select = policy(args.agent, args.model)
    try:
        with Episode(args.seed) as episode:
            if not args.headless:
                print('\x1b[?1049h\x1b[?25l', end='', flush=True)
            while not episode.info()['done']:
                action = select(episode.observe())
                if args.headless:
                    episode.step(action)
                else:
                    for frame in episode.frames(action):
                        print('\x1b[H' + frame + f'{args.agent.upper()} | skill: {ACTIONS[action]} | Ctrl+C to stop\x1b[K', end='', flush=True)
                        time.sleep(0.1 / args.speed)
            result = dict(agent=args.agent, seed=args.seed, **episode.info())
    except KeyboardInterrupt:
        result = dict(agent=args.agent, seed=args.seed, interrupted=True)
    finally:
        if not args.headless:
            print('\x1b[?25h\x1b[?1049l', end='', flush=True)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
