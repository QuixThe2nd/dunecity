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

Final 672 Emscripten build and visible mission validation are pending at this checkpoint. Earlier 669 browser observations do not validate the final 672 changes.
