#!/usr/bin/env python3
"""Run a reproducible native campaign comparison matrix without frame pacing.

Keeps full per-match evidence. Equal-tier matches test survival; fixed Hard
partners isolate changes in enemy difficulty. Selected second seeds and other
houses check whether a result is specific to one scenario. No player commands
or forced victories. A time limit is unfinished, not automatically balanced.
"""
import argparse
import concurrent.futures
import json
from pathlib import Path
import subprocess
import time


def matrix():
    cases = []
    def add(house, level, partner, enemy, seed):
        case = (house, level, partner, enemy, seed)
        if case not in cases:
            cases.append(case)
    for level, partner, enemy, seed in [(8, 'brutal', 'hard', 42),
            (9, 'hard', 'hard', 42), (9, 'brutal', 'hard', 1),
            (9, 'brutal', 'hard', 42), (9, 'brutal', 'brutal', 42)]:
        add('harkonnen', level, partner, enemy, seed)
    for level in range(4, 10):
        for tier in ('easy', 'medium', 'hard', 'brutal'):
            add('harkonnen', level, tier, tier, 42)
            add('harkonnen', level, 'hard', tier, 42)
    for level in range(6, 10):
        for seed in (1, 42):
            add('harkonnen', level, 'brutal', 'hard', seed)
    for house in ('atreides', 'ordos'):
        for level in (4, 9):
            add(house, level, 'easy', 'easy', 42)
    for level in (4, 5, 9):
        add('harkonnen', level, 'easy', 'easy', 1)
    for tier in ('hard', 'brutal'):
        add('harkonnen', 9, tier, tier, 1)
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--workers', type=int, choices=range(1, 5), default=2)
    args = parser.parse_args()
    root = args.output_dir.resolve()
    root.mkdir(parents=True, exist_ok=False)
    repo = Path(__file__).resolve().parents[2]
    cases = matrix()
    (root/'plan.json').write_text(json.dumps(cases, indent=2)+'\n')
    def run(case):
        house, level, partner, enemy, seed = case
        name = f'{house}-l{level}-{partner}-{enemy}-s{seed}'
        start = time.monotonic()
        command = ['python3', str(repo/'tests/ai/run-campaign-balance.py'),
            '--output-dir', str(root/name), '--house', house, '--level', str(level),
            '--partner-difficulty', partner, '--enemy-difficulty', enemy,
            '--seed', str(seed), '--minutes', '60']
        with (root/(name+'.driver.log')).open('w') as log:
            result = subprocess.run(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT)
        summary = root/name/'summary.json'
        outcome = json.loads(summary.read_text())['result'] if summary.exists() else 'FAILED'
        return dict(case=name, returncode=result.returncode,
                    wall_seconds=round(time.monotonic()-start, 1), result=outcome)
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        for future in concurrent.futures.as_completed([pool.submit(run, case) for case in cases]):
            result = future.result()
            results.append(result)
            (root/'results.json').write_text(json.dumps(results, indent=2)+'\n')
            print(f"{len(results)}/{len(cases)} {result['case']} {result['result']} "
                  f"wall={result['wall_seconds']}s", flush=True)
    raise SystemExit(any(result['returncode'] for result in results))


if __name__ == '__main__':
    main()
