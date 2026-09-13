# Campaign AI validation — 1.0.672

13 September 2026. Game-source commit `27f14ffa0517e2fd77f6059d9f5e12eb6d5e4588`, clean tree during native tests. Local candidate only.

## Method

The real game engine ran with dummy SDL video/audio and no frame pacing. Each match ended at actual victory, actual defeat, or 225,000 cycles (60 game minutes). No mission skipping, artificial victories, or player commands. The human house used the named full QuantBot partner. Shared enemy assault house/count/value budgets were asserted every 250 cycles. All 22 runners exited successfully; a time limit is an unfinished match, not evidence of a healthy stalemate.

The source-built defense, pressure and pacing fixtures also passed, covering all four defense tiers, first-hit retaliation, remote harvester rescue, Area Guard, bounded pursuit, retained repair retreats, shared assault caps, opening/recovery, save/load, Windtrap orders, worker ceilings, and the eight-tank 2,400-value readiness case. All six CTest targets, native dependency audit and app signature verification passed.

## Completed native matches

Times below are game minutes. Evidence: `/tmp/dunecity-campaign-balance/672-validation/{case}/summary.json` and `profile/ai-decisions/*/events.jsonl`; condensed machine-readable analysis is `analysis.json` in that root.

| House | Level | Partner | Enemy | Seed | Outcome | Minutes | Player sorties | Enemy sorties |
| --- | ---: | --- | --- | ---: | --- | ---: | ---: | ---: |
| Atreides | 4 | Medium | Easy | 1237721204 | won | 15.94 | 2 | 1 |
| Harkonnen | 6 | Brutal | Hard | 1 | won | 19.51 | 3 | 3 |
| Harkonnen | 6 | Brutal | Hard | 42 | won | 19.27 | 3 | 3 |
| Harkonnen | 7 | Brutal | Hard | 1 | won | 18.01 | 2 | 3 |
| Harkonnen | 7 | Brutal | Hard | 42 | won | 18.14 | 2 | 3 |
| Harkonnen | 8 | Brutal | Hard | 1 | won | 21.75 | 5 | 4 |
| Harkonnen | 8 | Brutal | Hard | 42 | time limit | 60.0 | 0 | 7 |
| Harkonnen | 9 | Brutal | Hard | 1 | time limit | 60.0 | 0 | 35 |
| Harkonnen | 9 | Brutal | Hard | 42 | time limit | 60.0 | 1 | 28 |
| Harkonnen | 4 | Easy | Easy | 1 | won | 16.48 | 3 | 1 |
| Harkonnen | 4 | Easy | Easy | 42 | won | 16.41 | 3 | 1 |
| Harkonnen | 5 | Easy | Easy | 1 | won | 17.33 | 4 | 1 |
| Harkonnen | 5 | Easy | Easy | 42 | won | 18.26 | 5 | 1 |
| Harkonnen | 9 | Easy | Easy | 1 | won | 27.19 | 10 | 0 |
| Harkonnen | 9 | Easy | Easy | 42 | won | 33.87 | 15 | 3 |
| Harkonnen | 8 | Hard | Easy | 1 | won | 19.37 | 5 | 1 |
| Harkonnen | 8 | Hard | Easy | 42 | won | 19.67 | 5 | 1 |
| Harkonnen | 9 | Hard | Easy | 1 | won | 20.5 | 5 | 1 |
| Harkonnen | 9 | Hard | Easy | 42 | won | 21.87 | 6 | 2 |
| Harkonnen | 9 | Medium | Medium | 42 | won | 26.53 | 9 | 1 |
| Harkonnen | 9 | Hard | Hard | 42 | time limit | 60.0 | 11 | 20 |
| Harkonnen | 9 | Brutal | Brutal | 42 | lost | 47.56 | 0 | 23 |

17 wins, four time limits, one defeat. Across these matches telemetry recorded 2,245 defense responses and 3,209 campaign retaliations; these count orders/events, not unique battles or proof of tactical quality. Easy level 9 seed 1 had no offensive sortie, despite active defense; a win in that sample does not measure exposure to every intended wave.

## Reported harvester/no-attack regression

The closed-match telemetry identified Atreides level 4, seed 1237721204, Medium partner/Easy Harkonnen enemy, with a game harvester ceiling of 100. Reproduction used those settings in ordinary campaign mode, not the original co-op transport. The old enemy bought seven workers and repeatedly waited below a 4,600-value army threshold. In 672 it bought no extra workers, retained its initial worker, launched a four-unit/500-value sortie at 13.00 game minutes, and lost normally at 15.94 minutes. This complements the exact eight-tank fixture: 2,399 waits; 2,400 sends four tanks and leaves four.

## Remaining readiness limitation

Brutal partner/Hard enemy level 8 seed 42 reached 60 minutes with 15,080 player army value, zero offensive sorties, and zero enemy combat army remaining. The partner still required 16,000 army value. Level 9 seed 1 also never launched (7,830 remaining versus 16,000 required); seed 42 launched once then stalled at 9,810 versus 16,000. Hard/Hard level 9 stalled with 7,950 versus an 8,000 threshold while enemy combat armies were exhausted. Spice was exhausted in these samples. These are fixed-threshold deadlocks or attrition stalls, not proof of evenly matched opponents. The current enemy-wave correction does not change the full human partner readiness policy.

Brutal/Brutal level 9 seed 42 lost at 47.56 minutes without a player offensive sortie. Broader human difficulty, other houses and more seeds remain unvalidated. Do not describe the campaign as balanced based on AI self-play wins.

## Browser validation

The served 672 browser build uses the same game source, with Release C++ objects
and a local link override:
`CMAKE_EXE_LINKER_FLAGS=-O2 -sBINARYEN_EXTRA_PASSES=--no-stack-ir`.
Packaging source is `63e166f` (documentation-only changes after game commit
`27f14ff`). All seven served artifact hashes match `play/build.json`.

The stock `-O3` final StackIR optimization was stopped after over 30 wall minutes;
a sampled stack showed `StackIROptimizer::local2Stack` / `LazyLocalGraph`
destruction during binary writing. Two concurrent builds on this 16 GB Mac
also drove swap usage to about 20 GB. An experimental `-O1 --no-stack-ir` link
compiled but crashed both embedded Chromium and a fresh Chrome renderer during
startup. Its 31 MB wasm validated and compiled separately in Node. The `-O2`
link produced a 14 MB wasm and opened successfully in both browsers. The
underlying Chromium failure was not fully diagnosed; the failed `-O1` artifact
has been replaced. These local link overrides are not a release configuration
change or validation of the cancelled stock `-O3` build.

A fresh visible embedded-browser run completed normally: Harkonnen level 4,
SCENH008.INI, seed 1701707512, Vanilla, Easy full partner/Easy enemies,
normal campaign, default harvester ceiling, maximum speed (4 ms cycle delay).
No player combat/economy commands or Skip mission were used. The victory
briefing and completed score screen were observed: **427 points, Base Commander,
16 minutes displayed**, 57 enemy units and 21 enemy buildings destroyed versus
14 player units and zero player buildings destroyed. The score screen remains
open as the user-facing result (in-app tab 12).

Ordos launched one four-unit/550-value wave at cycle 48,798 (13.01 game minutes),
with 5,900 total army value at dispatch. Its telemetry records 24
`defence_response` and 44 `campaign_retaliation` events. It never exceeded one
harvester in the 32 enemy state snapshots. The partner launched three sorties.
The exact eight-tank/2,400-value boundary is covered by the separate native
pacing fixture; this browser wave is not that synthetic fixture.

Browser telemetry session: `1789299110607000-0`, under
`/home/web_user/.config/DuneCity/ai-decisions/` in the browser's persistent FS.
The last recorded cycle is 59,270 (15.805 game minutes); it is a telemetry
snapshot, not an asserted exact victory cycle. `game_summary` has not yet been
written because the score screen remains open before game teardown. Victory
was verified from the actual result UI, not inferred from remaining armies.
