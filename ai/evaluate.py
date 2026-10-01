"""Evaluate frozen policies on held-out maps, saving every episode and paired differences."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import platform
import random
from statistics import mean, stdev
from native import ROOT, TEST_SEEDS, VALIDATION_SEEDS, rollout
from policies import policy


def paired_interval(differences):
    rng = random.Random(123)
    means = sorted(mean(rng.choices(differences, k=len(differences))) for _ in range(2000))
    return [means[49], means[1949]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--agents', nargs='+', choices=('nearest', 'cem', 'ppo'), default=['nearest', 'cem', 'ppo'])
    parser.add_argument('--split', choices=('validation', 'test'), default='test')
    parser.add_argument('--output', type=Path, default=Path('ai/reports/benchmark.json'))
    args = parser.parse_args()
    seeds = TEST_SEEDS if args.split == 'test' else VALIDATION_SEEDS
    rows, summary, by_agent = [], {}, {}
    for name in args.agents:
        select = policy(name)
        results = [dict(agent=name, **rollout(seed, select)) for seed in seeds]
        rows.extend(results)
        by_agent[name] = results
        summary[name] = {key: mean(r[key] for r in results)
                         for key in ('score', 'success', 'seconds', 'hits', 'crates', 'stars', 'truncated')}
        summary[name]['score_stddev'] = stdev(r['score'] for r in results)
        print(f'{name}: {json.dumps(summary[name])}', flush=True)
    comparisons = {}
    for first, second in (('cem', 'nearest'), ('ppo', 'nearest'), ('ppo', 'cem')):
        if first in by_agent and second in by_agent:
            differences = [a['score'] - b['score'] for a, b in zip(by_agent[first], by_agent[second])]
            comparisons[f'{first}_minus_{second}'] = dict(mean=mean(differences),
                bootstrap_95_percent=paired_interval(differences),
                wins=sum(d > 0 for d in differences), ties=sum(d == 0 for d in differences))
    hashes = {}
    for name, filename in (('cem', 'cem.json'), ('ppo', 'ppo.zip')):
        if name in args.agents:
            hashes[name] = hashlib.sha256((ROOT / 'ai/models' / filename).read_bytes()).hexdigest()
    report = dict(split=args.split, seeds=list(seeds), platform=platform.platform(),
                  python=platform.python_version(), model_sha256=hashes,
                  summary=summary, paired_comparisons=comparisons)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    with args.output.with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


if __name__ == '__main__':
    main()
