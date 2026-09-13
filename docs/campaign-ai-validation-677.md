# Native campaign balance and resistance — 1.0.677

13 September 2026. Local game-source commit `0861fba`, diagnostic source snapshot `a6c54ef`. This report supersedes the fixed-threshold limitation in the 1.0.672 validation; it does not represent a public deployment.

## Method

59 real native-engine matches, using dummy SDL video/audio and no frame pacing. Actual victory/defeat or a 60-game-minute cutoff; no human commands, mission skipping or forced wins. Vanilla, isolated profiles, fog disabled, default harvester ceiling. Two native workers. Each runner compiles a diagnostic main against the same game objects; hooks do not ship. The selected level chooses the normal initial campaign branch, so random seeds vary play rather than exhaust all mission branches.

Equal-tier matches cover Harkonnen levels 4–9. A fixed Hard partner against all four enemy tiers at each level separates enemy changes from helper changes. Additional cases cover Brutal/Hard on levels 6–9, second seeds on selected cases and Easy/Easy Atreides/Ordos on levels 4 and 9. This is a targeted sample, not a statistical human win-rate estimate.

Outcomes from the player's side: **49 wins, 10 defeats, 0 time limits**. Median wall time per runner (compile/link plus simulation) was 7.7 seconds; maximum 14.9 seconds. Every live match checked shared enemy assault house/count/value caps every 250 game cycles.

## Changes validated

- Campaign attacks no longer require an existing repair yard. Missing repairs cannot disable fighting.
- Hard and Brutal prioritize one missing repair yard and missing buildable prerequisites, for both enemy and helper roles. Existing/queued structures count, and normal technology, cash, placement and working-economy requirements apply. This also replaces destroyed yards.
- After minute 15, once map spice is exhausted, campaign controllers stop waiting for a larger army. Dispatch still respects defensive reserves, shared enemy wave caps, opening, recovery and no-top-up rules. A remaining cash balance or cheap survivors cannot keep a fixed army goal alive.
- Idle dispatched campaign attackers scout unexplored terrain instead of repeatedly acquiring distant aircraft. Full helpers can explicitly reacquire visible bases. Home defenders, human orders, enemy wave limits and recovery remain protected.
- All campaign tiers recheck an unmet army threshold within 15 game seconds. No new serialized AI state or free resources were introduced.

All six CTest targets and the native dependency audit passed. Real-engine fixtures passed for Hard/Brutal repair prerequisites and duplicate prevention in both roles, defense/retaliation/remote harvester rescue, shared pressure/save state, scouting/manual-order protection, and the Easy level-4 eight-tank boundary (2,399 waits; 2,400 sends four and keeps four).

## Balance assessment

The frozen-match regressions are resolved in this sample, and upper-level enemy pressure is more differentiated. All 13 Easy/Easy samples won, including levels 4 and 9 for Atreides/Ordos and Harkonnen levels 4–9. With a fixed Hard helper, level 9 progressed from two enemy sorties on Easy to three on Medium, 25 on Hard and 33 on Brutal; the Brutal enemy won. This supports real resistance and a useful late-game difficulty gradient, not a claim that Easy is easy for an unaided human.

The curve is not fully smooth. Levels 4, 6 and 7 still overlap in outcomes and damage ratios, while level 5 Brutal is a pronounced scenario-dependent spike. A Brutal helper also lost both level-9 Hard-enemy seeds while the Hard helper won one and lost one. The helper's larger legacy readiness goal (16,000 versus 8,000 while spice remains) is a plausible contributor, not an isolated causal result. Do not call Brutal a consistently stronger campaign helper from these runs.

Two individual enemy no-damage spells remain below. They are tactical effectiveness concerns, despite completed matches; they have not been relabelled as successful attacks. Human playtests with limited or no helper and additional mission branches remain needed before declaring overall campaign balance finished.

## Controlled enemy-difficulty comparison

Fixed **Hard player-side helper**, Harkonnen, seed 42. W/L and minutes refer to the player. Sorties are nonempty offensive dispatches, not shots or defensive responses. E/P combines all enemy houses; campaign starting armies and bases are asymmetric.

| Level | Easy enemy | Medium enemy | Hard enemy | Brutal enemy |
| --- | --- | --- | --- | --- |
| 4 | W 16.7m; 1 attack | W 15.2m; 1 attack | W 16.2m; 1 attack | W 15.9m; 1 attack |
| 5 | W 17.3m; 1 attack | W 17.6m; 2 attacks | W 23.3m; 7 attacks | L 26.6m; 6 attacks |
| 6 | W 17.9m; 1 attack | W 20.0m; 2 attacks | W 19.3m; 2 attacks | W 21.8m; 3 attacks |
| 7 | W 19.3m; 1 attack | W 18.5m; 2 attacks | W 19.1m; 4 attacks | W 20.6m; 4 attacks |
| 8 | W 17.6m; 1 attack | W 24.5m; 2 attacks | W 42.3m; 16 attacks | L 46.5m; 17 attacks |
| 9 | W 21.8m; 2 attacks | W 26.3m; 3 attacks | W 32.4m; 25 attacks | L 41.4m; 33 attacks |

Final spice and damage ratios for the same fixed-helper comparisons. Damage is actual hostile HP removed on priced targets, excluding overkill; cost-weighted damage is also available in the full table. Neither HP ratios nor losses alone are a measure of fairness.

| Level | Enemy | Spice E/P | HP damage E/P | Cost-weighted damage E/P | Loss value E/P |
| --- | --- | ---: | ---: | ---: | ---: |
| 4 | easy | 0.19 | 0.31 | 0.34 | 1.75 |
| 4 | medium | 0.28 | 0.22 | 0.24 | 2.17 |
| 4 | hard | 0.34 | 0.32 | 0.35 | 1.75 |
| 4 | brutal | 0.44 | 0.23 | 0.24 | 2.81 |
| 5 | easy | 0.25 | 0.12 | 0.14 | 3.58 |
| 5 | medium | 0.46 | 0.29 | 0.35 | 3.30 |
| 5 | hard | 0.54 | 0.53 | 0.56 | 1.56 |
| 5 | brutal | 2.88 | 1.10 | 0.76 | 0.43 |
| 6 | easy | 0.21 | 0.26 | 0.31 | 3.40 |
| 6 | medium | 0.35 | 0.23 | 0.26 | 4.49 |
| 6 | hard | 0.37 | 0.24 | 0.29 | 3.60 |
| 6 | brutal | 0.57 | 0.30 | 0.34 | 4.27 |
| 7 | easy | 0.16 | 0.32 | 0.36 | 3.17 |
| 7 | medium | 0.34 | 0.32 | 0.34 | 3.12 |
| 7 | hard | 0.38 | 0.37 | 0.34 | 3.38 |
| 7 | brutal | 0.74 | 0.46 | 0.39 | 3.39 |
| 8 | easy | 0.46 | 0.18 | 0.19 | 7.03 |
| 8 | medium | 0.74 | 0.31 | 0.37 | 3.29 |
| 8 | hard | 0.97 | 0.47 | 0.52 | 3.04 |
| 8 | brutal | 1.53 | 0.68 | 0.66 | 1.64 |
| 9 | easy | 0.42 | 0.20 | 0.23 | 5.39 |
| 9 | medium | 0.64 | 0.26 | 0.29 | 4.23 |
| 9 | hard | 1.24 | 0.33 | 0.36 | 4.66 |
| 9 | brutal | 2.40 | 0.68 | 0.54 | 1.92 |

## Matched-time economics and combat on level 9

Fixed Hard partner, seed 42. Each row uses one observer snapshot containing all houses, avoiding duplicated shared human/AI ledgers. P/E values are cumulative player/enemy totals. Sample timestamps are shown rather than pretending snapshots occur exactly on the requested minute.

| Enemy | Sample minute | Refined spice P/E | HP damage P/E | Army value P/E | Harvesters P/E | Sorties P/E |
| --- | ---: | --- | --- | --- | --- | --- |
| easy | 9.63 | 10506/10038 | 0/0 | 4400/8850 | 9/4 | 0/0 |
| easy | 14.69 | 26941/15405 | 4338/3612 | 9960/11010 | 12/4 | 2/1 |
| easy | 19.76 | 46177/22178 | 37546/8558 | 19700/100 | 13/5 | 5/2 |
| medium | 9.63 | 8697/12708 | 0/550 | 3000/9900 | 8/8 | 0/0 |
| medium | 14.69 | 23796/22567 | 1805/2981 | 7370/14350 | 11/8 | 0/1 |
| medium | 19.76 | 41090/36977 | 15027/9090 | 12150/9920 | 11/8 | 3/3 |
| hard | 9.63 | 8084/23723 | 0/0 | 4200/16750 | 11/12 | 0/0 |
| hard | 14.69 | 22322/40508 | 4332/2664 | 7650/22800 | 12/12 | 0/4 |
| hard | 19.76 | 40381/55981 | 15809/8428 | 16500/19800 | 12/9 | 3/12 |
| hard | 29.89 | 50635/62985 | 64872/23363 | 17550/0 | 10/6 | 10/25 |
| brutal | 9.63 | 8776/32569 | 0/0 | 4400/17110 | 8/28 | 0/0 |
| brutal | 14.69 | 20159/74832 | 5148/3329 | 7670/27000 | 8/27 | 0/5 |
| brutal | 19.76 | 30881/76015 | 14961/9194 | 11020/25450 | 8/24 | 4/12 |
| brutal | 29.89 | 31640/76037 | 28474/16572 | 7600/13460 | 8/24 | 10/22 |

## Previous stalls, rerun with the same seed

| Match | 672 outcome | 677 outcome | Player sorties before/after | Longest armed no-damage spell before/after |
| --- | --- | --- | --- | --- |
| harkonnen-l8-brutal-hard-s42 | time_limit 60.0m | won 23.82m | 0/3 | 28.88/0.0m |
| harkonnen-l9-brutal-hard-s1 | time_limit 60.0m | lost 45.88m | 0/11 | 3.55/0.51m |
| harkonnen-l9-brutal-hard-s42 | time_limit 60.0m | lost 53.65m | 1/18 | 3.55/1.01m |
| harkonnen-l9-hard-hard-s42 | time_limit 60.0m | won 32.42m | 11/12 | 7.09/0.0m |
| harkonnen-l9-brutal-brutal-s42 | lost 47.56m | lost 37.92m | 0/11 | 2.03/0.0m |

## Inactivity and repair audit

An armed no-damage spell means at least 600 army value with unchanged cumulative HP damage between sampled snapshots after minute 15. Travel, defense without contact, and reserve troops can create such spells; they require inspection rather than being automatic failures. Death before the opening and active defense can also explain zero offensive waves.

Two five-minute inspection flags were examined:

- Hard helper/Brutal enemy, Harkonnen level 8 seed 42: Ordos had no HP damage from minute 27.40 to 38.55 (11.15 minutes), while sending two one-unit sorties. The player dealt another 4,726 HP damage and Atreides another 7,588 HP during this span; the player ultimately lost. Ordos ended the span with 1,800 army value.
- Brutal helper/Hard enemy, level 9 seed 42: Ordos had no HP damage from minute 44.63 to 53.24 (8.61 minutes), while sending four small sorties. The player dealt another 2,161 HP and Atreides 2,493 HP during the span; the player ultimately lost. Ordos ended with 850 army value.

These are individual ineffective attacks/lulls, not frozen matches. The exact tactical cause of these two residual spells is not established.

Every final match included at least one nonempty enemy offensive sortie. This records dispatch, not proof that each sortie reached or damaged its objective.


Representative completed repair construction/use on level 9, seed 42. P/E aggregates the helper house and all enemy houses. Built counts include replacements; busy samples only establish observed use.

| Partner/enemy | Yards built P/E | Busy-yard samples P/E |
| --- | --- | --- |
| brutal/brutal | 2/3 | 6/7 |
| hard/brutal | 2/3 | 3/5 |
| hard/hard | 2/3 | 7/2 |

## All final matches

Loss value includes unit and building losses at nominal prices, including environmental losses. It is not purely damage caused by the opponent. House-level repair counts include replacements; busy samples demonstrate observed use but do not measure repair throughput.

| Case | Result/min | Spice E/P | HP damage E/P | Value damage E/P | Loss value E/P | Sorties P/E |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| atreides-l4-easy-easy-s42 | won 15.29 | 0.15 | 0.37 | 0.41 | 1.46 | 3/1 |
| atreides-l9-easy-easy-s42 | won 29.22 | 0.39 | 0.21 | 0.31 | 4.17 | 12/2 |
| harkonnen-l4-brutal-brutal-s42 | won 17.58 | 0.45 | 0.25 | 0.26 | 2.86 | 2/2 |
| harkonnen-l4-easy-easy-s1 | won 16.82 | 0.17 | 0.22 | 0.27 | 1.60 | 3/1 |
| harkonnen-l4-easy-easy-s42 | won 16.61 | 0.16 | 0.20 | 0.23 | 1.90 | 3/1 |
| harkonnen-l4-hard-brutal-s42 | won 15.93 | 0.44 | 0.23 | 0.24 | 2.81 | 3/1 |
| harkonnen-l4-hard-easy-s42 | won 16.67 | 0.19 | 0.31 | 0.34 | 1.75 | 2/1 |
| harkonnen-l4-hard-hard-s42 | won 16.2 | 0.34 | 0.32 | 0.35 | 1.75 | 3/1 |
| harkonnen-l4-hard-medium-s42 | won 15.19 | 0.28 | 0.22 | 0.24 | 2.17 | 2/1 |
| harkonnen-l4-medium-medium-s42 | won 17.99 | 0.30 | 0.38 | 0.43 | 1.41 | 4/2 |
| harkonnen-l5-brutal-brutal-s42 | lost 26.6 | 2.88 | 1.10 | 0.76 | 0.43 | 0/6 |
| harkonnen-l5-easy-easy-s1 | won 17.78 | 0.21 | 0.24 | 0.35 | 2.32 | 4/1 |
| harkonnen-l5-easy-easy-s42 | won 17.61 | 0.29 | 0.29 | 0.39 | 2.36 | 4/1 |
| harkonnen-l5-hard-brutal-s42 | lost 26.6 | 2.88 | 1.10 | 0.76 | 0.43 | 0/6 |
| harkonnen-l5-hard-easy-s42 | won 17.28 | 0.25 | 0.12 | 0.14 | 3.58 | 2/1 |
| harkonnen-l5-hard-hard-s42 | won 23.34 | 0.54 | 0.53 | 0.56 | 1.56 | 3/7 |
| harkonnen-l5-hard-medium-s42 | won 17.64 | 0.46 | 0.29 | 0.35 | 3.30 | 2/2 |
| harkonnen-l5-medium-medium-s42 | won 20.74 | 0.48 | 0.35 | 0.43 | 2.34 | 5/3 |
| harkonnen-l6-brutal-brutal-s42 | won 20.23 | 0.57 | 0.21 | 0.23 | 6.00 | 4/3 |
| harkonnen-l6-brutal-hard-s1 | won 19.06 | 0.35 | 0.26 | 0.29 | 3.60 | 3/3 |
| harkonnen-l6-brutal-hard-s42 | won 19.95 | 0.36 | 0.28 | 0.33 | 3.22 | 2/3 |
| harkonnen-l6-easy-easy-s42 | won 22.03 | 0.20 | 0.42 | 0.54 | 1.96 | 8/2 |
| harkonnen-l6-hard-brutal-s42 | won 21.77 | 0.57 | 0.30 | 0.34 | 4.27 | 6/3 |
| harkonnen-l6-hard-easy-s42 | won 17.89 | 0.21 | 0.26 | 0.31 | 3.40 | 3/1 |
| harkonnen-l6-hard-hard-s42 | won 19.31 | 0.37 | 0.24 | 0.29 | 3.60 | 5/2 |
| harkonnen-l6-hard-medium-s42 | won 19.97 | 0.35 | 0.23 | 0.26 | 4.49 | 5/2 |
| harkonnen-l6-medium-medium-s42 | won 19.95 | 0.35 | 0.23 | 0.26 | 3.72 | 5/2 |
| harkonnen-l7-brutal-brutal-s42 | won 25.51 | 0.84 | 0.51 | 0.42 | 2.37 | 3/6 |
| harkonnen-l7-brutal-hard-s1 | won 18.05 | 0.36 | 0.27 | 0.24 | 4.75 | 2/3 |
| harkonnen-l7-brutal-hard-s42 | won 21.1 | 0.34 | 0.36 | 0.31 | 4.04 | 2/4 |
| harkonnen-l7-easy-easy-s42 | won 21.83 | 0.19 | 0.40 | 0.47 | 2.56 | 7/2 |
| harkonnen-l7-hard-brutal-s42 | won 20.64 | 0.74 | 0.46 | 0.39 | 3.39 | 6/4 |
| harkonnen-l7-hard-easy-s42 | won 19.34 | 0.16 | 0.32 | 0.36 | 3.17 | 5/1 |
| harkonnen-l7-hard-hard-s42 | won 19.09 | 0.38 | 0.37 | 0.34 | 3.38 | 4/4 |
| harkonnen-l7-hard-medium-s42 | won 18.46 | 0.34 | 0.32 | 0.34 | 3.12 | 5/2 |
| harkonnen-l7-medium-medium-s42 | won 22.14 | 0.34 | 0.51 | 0.59 | 2.14 | 7/3 |
| harkonnen-l8-brutal-brutal-s42 | lost 28.97 | 1.54 | 0.82 | 0.67 | 1.34 | 8/15 |
| harkonnen-l8-brutal-hard-s1 | won 30.31 | 0.81 | 0.42 | 0.44 | 2.97 | 8/9 |
| harkonnen-l8-brutal-hard-s42 | won 23.82 | 0.88 | 0.36 | 0.35 | 4.41 | 3/7 |
| harkonnen-l8-easy-easy-s42 | won 26.15 | 0.47 | 0.41 | 0.49 | 3.39 | 10/3 |
| harkonnen-l8-hard-brutal-s42 | lost 46.5 | 1.53 | 0.68 | 0.66 | 1.64 | 14/17 |
| harkonnen-l8-hard-easy-s42 | won 17.64 | 0.46 | 0.18 | 0.19 | 7.03 | 4/1 |
| harkonnen-l8-hard-hard-s42 | won 42.34 | 0.97 | 0.47 | 0.52 | 3.04 | 19/16 |
| harkonnen-l8-hard-medium-s42 | won 24.47 | 0.74 | 0.31 | 0.37 | 3.29 | 8/2 |
| harkonnen-l8-medium-medium-s42 | won 21.44 | 0.67 | 0.29 | 0.34 | 4.91 | 7/3 |
| harkonnen-l9-brutal-brutal-s1 | lost 38.03 | 3.30 | 0.82 | 0.61 | 1.81 | 14/27 |
| harkonnen-l9-brutal-brutal-s42 | lost 37.92 | 2.35 | 0.77 | 0.61 | 2.04 | 11/28 |
| harkonnen-l9-brutal-hard-s1 | lost 45.88 | 1.66 | 0.64 | 0.55 | 1.81 | 11/52 |
| harkonnen-l9-brutal-hard-s42 | lost 53.65 | 1.47 | 0.64 | 0.56 | 2.07 | 18/48 |
| harkonnen-l9-easy-easy-s1 | won 27.51 | 0.33 | 0.23 | 0.33 | 3.71 | 11/2 |
| harkonnen-l9-easy-easy-s42 | won 29.41 | 0.35 | 0.29 | 0.37 | 3.25 | 13/2 |
| harkonnen-l9-hard-brutal-s42 | lost 41.41 | 2.40 | 0.68 | 0.54 | 1.92 | 15/33 |
| harkonnen-l9-hard-easy-s42 | won 21.83 | 0.42 | 0.20 | 0.23 | 5.39 | 6/2 |
| harkonnen-l9-hard-hard-s1 | lost 33.59 | 1.91 | 0.73 | 0.61 | 1.45 | 10/28 |
| harkonnen-l9-hard-hard-s42 | won 32.42 | 1.24 | 0.33 | 0.36 | 4.66 | 12/25 |
| harkonnen-l9-hard-medium-s42 | won 26.27 | 0.64 | 0.26 | 0.29 | 4.23 | 8/3 |
| harkonnen-l9-medium-medium-s42 | won 23.98 | 0.69 | 0.21 | 0.27 | 4.24 | 8/2 |
| ordos-l4-easy-easy-s42 | won 15.54 | 0.16 | 0.21 | 0.26 | 1.88 | 3/1 |
| ordos-l9-easy-easy-s42 | won 36.59 | 0.39 | 0.48 | 0.60 | 3.43 | 18/6 |

## Reproduction and evidence

Run `python3 tests/ai/run-campaign-matrix.py --output-dir /tmp/dune-new-comparison` after the audited native build. Analyze with `python3 tests/ai/analyze-campaign-balance.py /tmp/dune-new-comparison --output /tmp/dune-new-comparison/report`. Use a new output directory each time.

Evidence: `/tmp/dunecity-campaign-balance/677-final/{case}/summary.json` and `profile/ai-decisions/*/events.jsonl`. Aggregated house ledgers and matched-time checkpoints: `/tmp/dunecity-campaign-balance/677-resistance.json`. Fixtures: `/tmp/dunecity-campaign-balance/677-probes/`. Earlier diagnostic batches are excluded from these 59 final matches. In 676, Hard/Brutal level 8 seed 42 reached 60 minutes with six surviving launchers and 15.71 armed minutes without damage. End-state inspection found idle HUNT units repeatedly acquiring a distant carryall while bases remained undiscovered. The scouting change resolves that acquisition loop; the matched final result is in the full table.

The browser remains on the separately validated 672 build. These 677 changes are committed and built locally for native testing, not rebuilt or deployed to the public web game.
