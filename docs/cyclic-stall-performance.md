# Cyclic stall investigation — 1.0.721

## Same-game reproduction

Stefan reported the whole picture freezing while the native mouse pointer still
moved, estimating pauses up to about one second. The running 1.0.720 process was
sampled without restarting it. Its telemetry showed frames up to 331 ms, mainly
AI construction planning. Five-second window totals accounted for elapsed time;
we did not capture a one-second freeze in that sample.

The graphical reproduction loads the actual `cities 3.dls` from Stefan's profile
at cycle 329538 (192×192 map, seed 1162923400). It uses the ordinary Game main loop,
renderer, input handling and pacing, with copied 1440×900 windowed display/audio
settings, city mod overrides and 4 ms game speed. Only the menu entry is replaced
to load the save directly and a timer exits this separate process after 60 seconds.
The original profile and save are untouched. The rendered city was visually checked.

The baseline reproduced a 298.46 ms frame at cycle 331600: 274.54 ms in AI, with
271.85 ms attributed to house 5. The live game had paused at the same cycle.
Repeated searches for the same building sites across idle construction yards
were the dominant unnecessary work. The native OS cursor moves independently
of the game loop, so pointer movement does not rule out a main-thread stall.

## Change and invariants

Placement results, including failed searches, are reused across equivalent
builders within one synchronous AI build pass. A builder with its own reservation
has a distinct exclusion key. Map/reservation changes still invalidate results,
and campaign power/repair reservations and palace unit summons explicitly clear
the placement cache. Changes to city production-plot mode still clear it.
Nothing is cached across build passes. Candidate order, scoring, commands, AI
cadence, pathfinding node budgets and simulation timing are unchanged.

Telemetry now splits AI construction into evaluation and order phases and counts
placement searches/cache hits. Every game-loop frame of at least 100 ms gets a
`frame_stall` JSON event, including smaller consecutive stalls. It includes the
session wall-clock timestamp, cycle, phase timings, input/command time, menu,
pause and window-focus state. Gaps outside the measured frame are timed separately
as `frame.gap` and also produce a stall event at 100 ms. These use the existing
bounded telemetry session, under the profile's `ai-decisions/<session>/events.jsonl`.
Five-second aggregate metrics remain available. The text spike log no longer
rate-limits adjacent frames of at least 100 ms after its normal warm-up.

## Measurements

Sequential 60-second graphical runs, same starting save and settings:

| Metric | 1.0.720 baseline | Final candidate |
| --- | ---: | ---: |
| Longest rendered frame | 298.460 ms | 157.497 ms |
| Frames over 100 ms | 51 / 1877 | 32 / 2000 |
| Longest AI construction pass | 269.447 ms | 120.678 ms |
| Total AI construction time | 11.030 s / 372 passes | 7.223 s / 398 passes |
| End simulation cycle | 338835 | 339453 |

The live user match remained running, so scheduling/load varies, and equal wall
time covers different numbers of simulation cycles. These are observed runs,
not a guarantee of a particular frame rate or elimination of all stalls. There
are still 100–157 ms frames. All 32 candidate frames above 100 ms had individual
stall records. The longest between-frame gap was 0.588 ms; telemetry write and
text-flush maxima were 2.224 ms and 0.378 ms, respectively.

The deterministic 2,000-cycle candidate comparison matched all 21 checkpoints.
The packaged 1.0.721 engine also matched the 1.0.720 baseline across 4,000 cycles
(329538–333538), all 41 checkpoints and the complete saved gameplay state. Only the release label is ignored by the
cross-version comparator; save format, mod checksum, RNG and gameplay bytes are
included. All seven CTest suites passed, including a new consecutive-stall test.
The final 4,000-cycle run overlapped browser compilation; its timings are excluded
from the graphical comparison above.

## Reproduction

With the native Ninja build up to date:

```sh
python3 tests/performance/run-simulation-probe.py \
  --save '/absolute/profile/save/cities 3.dls' \
  --profile-from /absolute/profile --render-seconds 60 \
  --output-dir /absolute/new-graphical-run
```

Use the same command before/after an engine change, with distinct output paths.
For exact deterministic comparisons use `--cycles 4000` instead of
`--render-seconds`, and pass `--compare-dir /absolute/baseline` to the candidate.
Graphical runs have no fixed final cycle and cannot use the save comparator.

Evidence is in the parent workspace's `outputs/game-cyclic-lag-720/`:
`rendered-baseline`, `rendered-candidate-v2`, `rendered-comparison.json`, live log
snapshots/sample, and fixed-cycle comparisons. The intermediate graphical
candidate is retained separately, not used as the final comparison above.
