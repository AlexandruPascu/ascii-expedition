"""Evaluate the embedded C++ policies across tutorials, progression and all difficulties."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import platform
from statistics import mean
from native import ROOT, Episode, native_action


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seeds', type=int, default=32)
    parser.add_argument('--first-seed', type=int, default=40000)
    parser.add_argument('--stages', type=int, default=14)
    parser.add_argument('--output', type=Path, default=ROOT / 'ai/reports/campaign.json')
    args = parser.parse_args()
    if not 1 <= args.seeds <= 10000 or not 1 <= args.stages <= 1000:
        parser.error('Use 1..10000 seeds and 1..1000 stages')
    if args.first_seed < 0 or args.first_seed + args.seeds > 0x100000000:
        parser.error('Seeds must fit in unsigned 32-bit integers')
    rows, summary = [], []
    for agent in ('cem', 'ppo'):
        for difficulty in ('relaxed', 'normal', 'hard'):
            group = []
            for seed in range(args.first_seed, args.first_seed + args.seeds):
                with Episode(seed, difficulty=difficulty, tutorial=True) as episode:
                    for _ in range(args.stages):
                        before = episode.info()
                        while not episode.info()['done']:
                            episode.step(native_action(episode.observe(), agent))
                        after = episode.info()
                        row = dict(agent=agent, difficulty=difficulty, seed=seed, stage=after['stage'],
                                   success=after['success'], lives=after['lives'], decisions=after['decisions'])
                        row.update({key: after[key] - before[key] for key in ('score', 'seconds', 'hits', 'crates', 'stars')})
                        rows.append(row)
                        group.append(row)
                        if not after['success']:
                            break
                        if after['stage'] + 1 < args.stages:
                            episode.next_level()
            result = dict(agent=agent, difficulty=difficulty, maps=len(group),
                          expected_maps=args.seeds * args.stages,
                          cleared=sum(r['success'] for r in group), hits=sum(r['hits'] for r in group),
                          mean_stage_score=mean(r['score'] for r in group),
                          mean_stage_seconds=mean(r['seconds'] for r in group),
                          max_decisions=max(r['decisions'] for r in group))
            summary.append(result)
            print(json.dumps(result), flush=True)
    report = dict(first_seed=args.first_seed, seed_count=args.seeds, stages=args.stages,
                  policy='embedded C++ inference, original frozen trained weights',
                  platform=platform.platform(),
                  export_sha256=hashlib.sha256((ROOT / 'ai/models/native/PolicyData.h').read_bytes()).hexdigest(),
                  summary=summary)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    with args.output.with_suffix('.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)


if __name__ == '__main__':
    main()
