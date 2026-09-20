# Periodic pause investigation — 1.0.720

## Live evidence

Stefan's running 1.0.719 match was sampled without restarting or pausing it.
A 50.26-second telemetry window averaged 33.23 rendered FPS. The largest frame
was 260.13 ms: AI 235.04 ms, pathfinding 13.58 ms and city work 3.64 ms. Other
large frames likewise coincided with AI construction planning. Pathfinding
still accounts for substantial average time but did not explain those spikes.
AI construction planning runs every 100 cycles per AI. City effects/growth run
on adjacent cycles every 78 cycles and sometimes land in the same rendered
frame. Tax calculation was walking the entire 192x192 map every cycle.

## Changes

- Ground access checks use a 64-bit local occupancy graph for ordinary building
  sizes. This removes grid/queue allocations while preserving cardinal/diagonal
  connectivity, frontage rules and passable-tile reads. Larger footprints keep
  the original implementation.
- Zone proximity scoring uses a per-search spatial index of owned zones and the
  original rounded octile distance. The industrial score only distinguishes
  distances below 6, through 16 and beyond 16, so one bounded query suffices.
- Nearest city-role origins and pollution-footprint separation use exact
  Chebyshev distance fields. They are created lazily after a candidate reaches
  scoring, include the same buildings/reservations, and die with the search.
  Candidate order, scoring, tie-breaks and placement-result caching are unchanged.
- Tax calculation visits actual structure origins through the structure list,
  verifies each against its map tile, and preserves row-major house payout
  order. Payment cadence, fixed-point sums, growth and effects schedules stay
  unchanged.

Pathfinding source, node budget, expansion limit and AI planning cadence are
unchanged. All earlier menu, sidebar, live-preview and A* fixes remain included.

## Verification and measurements

The real-engine probe loads `cities 3.dls` in a private profile, advances exactly
2,000 cycles (329538–331538), emits state digests every 100 cycles, and saves the
result. Three runs of each implementation produced byte-identical final saves
and matching checkpoints. Cross-version comparison excludes only the release
label; the save-format version, mod checksum and all gameplay bytes are checked.

| Headless simulation wall time | 1.0.719 median | Optimized median |
| --- | ---: | ---: |
| Total for 2,000 cycles | 23,935 ms | 15,733 ms |
| 99th percentile cycle | 124.49 ms | 66.18 ms |
| Worst cycle per run | 591.97 ms | 386.73 ms |

The live game remained running, so scheduling/load varied. Total times ranged
17.82–27.70 seconds before and 14.61–24.41 seconds after. These are simulation
benchmarks, not a rendered FPS claim or proof that all pauses disappeared.
AI planning bursts and periodic growth/effects remain possible. The original
single comparison measured AI-build work 5.95→4.50 seconds and tax work
1.97→0.56 seconds across those 2,000 cycles.

Regression coverage compares all 65,536 occupancy patterns around a 2x2 lot,
with/without required exits and diagonal movement, against the frozen original
access algorithm. Additional fixtures cover dimensions through 8x8, including
64-cell boundaries and the larger-footprint fallback. Distance fields are
compared with direct rectangle-distance scans for empty, multiple, overlapping
and edge sources and candidate footprints. All seven CTest suites pass.

Reproduce with a built native Ninja tree:

```sh
python3 tests/performance/run-simulation-probe.py --save /absolute/city.dls --output-dir /absolute/before
# Build the changed engine, then:
python3 tests/performance/run-simulation-probe.py --save /absolute/city.dls --output-dir /absolute/after --compare-dir /absolute/before
```

Evidence is under the parent workspace's `outputs/game-pauses-719/`, including
live logs/sample, baseline, candidate-v3, alternating repeats and
`simulation-comparison.json`. An eager distance-field trial and a broader
placement-cache trial are excluded from the final implementation and benchmark.
