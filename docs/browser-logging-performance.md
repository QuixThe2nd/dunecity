# Browser diagnostic logging benchmark — 1.0.722

## Result — 2026-09-19

Disabling diagnostics gave a small observed speed increase, not a solution to
the recurring stalls. The median of the two runs per mode was 13.26 FPS on
versus 13.54 FPS off (+2.1%). Median p99 frame time improved 225.35→217.05 ms
(3.7%). There were exactly 217 frames over 100 ms per two-minute run on average
in each mode. With only two runs per mode and uncontrolled background activity,
the small speed difference should not be treated as a precise guaranteed gain.

| Run | Logging | FPS | p99 frame ms | Longest frame ms | Frames >100 ms | Frames >250 ms | Diagnostic MB |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | On | 13.43 | 226.4 | 314.0 | 218 | 10 | 26.67 |
| 2 | Off | 13.58 | 218.9 | 338.1 | 219 | 10 | 0 |
| 3 | Off | 13.50 | 215.2 | 303.3 | 215 | 6 | 0 |
| 4 | On | 13.09 | 224.3 | 321.1 | 216 | 11 | 26.16 |

Each run captured four periodic storage flushes, with no errors. No measured
frame exceeded 500 ms. Thus these runs reproduce shorter stalls but not Stefan's
estimated half-to-one-second freezes. Diagnostic sizes include the 10-second
warmup and use decimal MB. Logging off created no AI trace/performance files.

In the two logging-on runs' internal timing windows, pathfinding accounted for
52.4% of accumulated game-frame time, rendering 12.1%, AI 12.2%, and city updates
8.1%. AI reached 253 ms within one frame; city updates reached 125 ms. These
windows include warmup and end at the last completed periodic window, so their
totals are not the exact 120-second comparison interval. They identify further
investigation targets, not measured gains from unimplemented changes. Nested
timing scopes must not be added together. Keep pathfinding budgets unchanged.

Raw samples, validation output and artifact/save hashes are in the parent
workspace's `outputs/browser-logging-performance-722/` (`run-1-on.json`,
`run-2-off.json`, `run-3-off.json`, `run-4-on.json`, `comparison.json`,
`fixture-manifest.json`). Files prefixed `calibration-` are excluded.
The save SHA-256 is
`b6e742b9503170039449eda1bba5f83bb4e77095fe4819ea2f9937b6487ca095`.
Production source/build stayed at 1.0.722; only benchmark tooling and this report
were added. Browser default remains diagnostics off.

## Method

Measured the actual shipped browser JS/Wasm/data, without recompiling or changing
game code. Loaded Stefan's `cities 3.dls` through Continue with copied display
settings and mod overrides. Canvas was 1440×900, game tick 4 ms, same initial
camera, no gameplay input. Browser identified as Chromium 152 on macOS.

Four sequential runs use on/off/off/on logging order, each with 10 seconds of
gameplay warmup excluded and 120 seconds measured. Each has a fresh isolated
localhost origin and IDBFS profile, preventing old diagnostic files from affecting
storage flushes. The real save and user profile are read-only fixtures. Intro and
fullscreen are disabled; the browser display migration is marked complete so the
copied resolution is retained. Each test tab is closed before the next opens.

The test-only shell probe stores frame, long-task and storage-sync timings in
memory, then writes results after measurement. It starts only after explicitly
arming at the main menu and opening the fixture save. Menu save-header reads do
not start the timer. Background/hidden visibility invalidates a run.

SDL 2.32.8 yields through Asyncify inside `Emscripten_GLES_SwapWindow`; the game's
loop then yields again. The probe samples every other normal-state yield to
measure complete presentation-to-presentation intervals. The first eight call
stacks, collected during warmup, must alternate between these two sites. Counting
every yield would incorrectly double reported FPS. Setup/calibration runs that
used that method are excluded. These are game presentation intervals, including
browser scheduling delays, not a direct display-compositor paint count.

The user’s running desktop game and background applications were left alone;
no compiler or game test suite ran during measurement. Two repetitions per mode
give limited precision. Equal wall time can advance different numbers of game
cycles, so this is an observed play-speed comparison, not a fixed-cycle cost
measurement. Initial menu residence and periodic storage-flush phase can differ.
Fresh profiles also do not reproduce months of accumulated browser log history.

Storage-sync callback latency includes asynchronous work and waiting for the main
thread; it must not be described as a single blocking pause. The separate
`invocation_ms` field measures the initial synchronous call only.

## Reproduce

Run from the repository root, choosing four unused/fresh origin ports:

```sh
python3 tests/performance/serve-browser-logging-benchmark.py \
  --port 18741 \
  --profile "$HOME/Library/Application Support/Dune City" \
  --save "$HOME/Library/Application Support/Dune City/save/cities 3.dls" \
  --output-dir ../outputs/browser-logging-performance-722
```

Open each printed URL sequentially. Wait for the home menu, press **Arm
benchmark**, then **Continue**. After the title reports completion, close that
tab before opening the next. The server saves raw samples and a fixture manifest
with save and shipped artifact hashes. Reused profiles deliberately fail rather
than silently carrying old files forward. Summarize with:

```sh
python3 tests/performance/summarize-browser-logging-benchmark.py \
  ../outputs/browser-logging-performance-722
```

The benchmark probe and copied profiles are only served by the private test
server. They are not included in the normal browser build.
