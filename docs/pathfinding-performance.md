# Pathfinding cost reduction, 1.0.719

The synchronous A* search now caches each tile's passability for the duration of
one search, skips closed neighbours before checking occupancy or calculating
costs, and calculates the current node's parent direction once rather than for
each neighbour. The cache is reset by the existing tile-buffer initialization;
it is never shared between units or searches. No saved state changes.

The node budget, expansion threshold, queue schedule, neighbour order, heap
tie-breaking, heuristic and fixed-point costs are unchanged. This reduces the
cost per search; it does not fix the existing whole-search budget overshoot.
Passability caching relies on synchronous execution: the unit and map must not
mutate during the search. Revisit that assumption before making A* resumable or
concurrent.

## Verification

`tests/pathfinding/run-pathfinding-probe.py` compiles the current search and the
original implementation at `12f5d46616d6736866668a73a7414d99119da220` into the same
real-engine diagnostic executable. The probe loads a supplied save in an
isolated profile without advancing the live match or overwriting the save.
It alternates which implementation runs first and compares every path coordinate
and the exact expanded-node count. An obstacle is introduced between rounds and
then removed to check that the scratch pool does not retain stale passability.

Example on this Mac:

```sh
python3 tests/pathfinding/run-pathfinding-probe.py \
  --build-dir build \
  --output-dir /tmp/dunecity-pathfinding-check \
  --save '/Users/stefan/Library/Application Support/Dune City/save/cities 3.dls'
```

The output directory must not already exist. The macOS Ninja build must be
current and have the usual game resources staged. The reference can be changed
with `--reference-ref`; its resolved commit is recorded with the output.

On 19 September 2026, `cities 3.dls` supplied 3,815 queries across 11 ground-unit
types. All three rounds matched: 11,445 paired searches, including 392 searches
at/above the expansion threshold and 809 empty routes in the first round.
The first round expanded 7,776,360 nodes in both implementations.

| Round | Original | Optimized |
|---|---:|---:|
| 1 | 2,274.367 ms | 1,657.989 ms |
| 2, added obstacle | 2,248.773 ms | 1,614.282 ms |
| 3, restored terrain | 2,102.731 ms | 1,531.969 ms |
| Total | 6,625.871 ms | 4,804.240 ms |

This is **27.5% less search time**, or **1.38× pathfinding throughput**, for this
query set. It is not a measured full-game FPS improvement. A prior run on the
same save measured 26.6% less time. Timings include obtaining/destroying the
returned path; implementations use separate scratch pools in one process.

Clean native Release build, dependency checks, version checks and all seven
CTest suites passed. The changed A* translation unit also compiled with
Emscripten. The complete browser build was subsequently rebuilt from the same
660d913 source as native 1.0.719, and its home screen/version were visually
verified. No browser runtime performance claim is made.

Local evidence is in
`/Users/stefan/Documents/projects/outputs/game-performance-20260919/`, including
`pathfinding-final/run.log`, `ctest.log`, `native-build.log` and
`wasm-compile.log`. The initial live-game profiling report is `analysis.md` in
the same directory.
