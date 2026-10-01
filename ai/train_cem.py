"""Cross-entropy search for a fixed, readable vector of objective weights."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import random
from statistics import mean, pstdev
import time
from native import FEATURES, TRAIN_SEEDS, VALIDATION_SEEDS, linear_action, rollout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generations', type=int, default=12)
    parser.add_argument('--population', type=int, default=32)
    parser.add_argument('--train-maps', type=int, default=48)
    parser.add_argument('--workers', type=int, default=4)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--output', type=Path, default=Path('ai/models/cem.json'))
    args = parser.parse_args()
    if args.generations < 1 or args.population < 4 or not 1 <= args.train_maps <= len(TRAIN_SEEDS) or args.workers < 1:
        parser.error('Use positive generations/workers, population >= 4 and train-maps in 1..256')
    rng = random.Random(args.seed)
    # A nearest-star-style initial policy; learned values are not hand-selected afterward.
    center = [-1.0, 1.0, 0.0, 0.0, 0.0, 0.0, -1.0, 1.0, 0.0, 0.0, 0.0, 0.0]
    spread = [1.5] * len(FEATURES)
    best_weights, best_validation = center[:], float('-inf')
    history = []
    started = time.monotonic()
    elite_count = max(2, args.population // 5)
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        def evaluate(weights, seeds):
            results = list(pool.map(lambda seed: rollout(seed, lambda obs: linear_action(obs, weights)), seeds))
            return mean(r['score'] for r in results)
        for generation in range(args.generations):
            seeds = rng.sample(TRAIN_SEEDS, args.train_maps)
            population = [center[:], best_weights[:]] + [
                [rng.gauss(m, s) for m, s in zip(center, spread)]
                for _ in range(args.population - 2)]
            ranked = sorted(((evaluate(w, seeds), i, w) for i, w in enumerate(population)), reverse=True)
            elites = [row[2] for row in ranked[:elite_count]]
            for i in range(len(FEATURES)):
                center[i] = 0.3 * center[i] + 0.7 * mean(w[i] for w in elites)
                spread[i] = max(0.08, 0.3 * spread[i] + 0.7 * pstdev(w[i] for w in elites))
            # Validation only chooses between the generation winner and running champion.
            candidate = ranked[0][2]
            validation = evaluate(candidate, VALIDATION_SEEDS)
            if validation > best_validation:
                best_weights, best_validation = candidate[:], validation
            row = dict(generation=generation + 1, training_score=ranked[0][0],
                       validation_score=validation, best_validation_score=best_validation,
                       training_seeds=seeds, weights=candidate, center=center[:], spread=spread[:])
            history.append(row)
            print(f"CEM {generation + 1:02d}: train {ranked[0][0]:.1f}, validation {validation:.1f}, best {best_validation:.1f}", flush=True)
    artifact = dict(schema=1, algorithm='cross-entropy', features=FEATURES, weights=best_weights,
                    config={k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()},
                    training_seeds=TRAIN_SEEDS, validation_seeds=VALIDATION_SEEDS,
                    validation_score=best_validation, elapsed_seconds=time.monotonic() - started,
                    history=history)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, indent=2) + '\n')
    print(f'Saved {args.output}', flush=True)


if __name__ == '__main__':
    main()
