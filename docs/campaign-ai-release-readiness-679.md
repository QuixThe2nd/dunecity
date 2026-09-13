# Native campaign release assessment — local AI 1.0.679

14 September 2026. Tested source: `552a59fcafef30910cd1a13ce63ff4101c97bc6e`;
game changes through `83f6264`. All 71 initial runs record this exact source
with an unmodified working tree. No game-code changes were made during this assessment.

## Decision

**Do not publish this exact branch/build yet.** Native stability and basic AI
activity are encouraging, but fix worker commitment accounting, integrate the
current main branch, assign a distinct version, and validate the resulting web
artifact first. No push, merge or deployment was performed.

The native tests do not establish unaided-human difficulty. Full AI helpers
control the player; maps have asymmetric starts; fog was disabled, with ordinary
exploration still used. This samples the initial campaign branch, not every
mission variant, mod, saved campaign or co-op configuration.

## Coverage and outcomes

- 59-case matrix: levels 4–9, all enemy tiers, equal-tier and fixed-Hard helpers,
  selected seeds 1/42 and all three houses.
- 12 additional level-9 Atreides/Ordos cases: Brutal versus Easy/Hard and Hard
  versus Hard, each with seeds 1/42.
- Initial 60-game-minute cap: 48 player victories, 22 defeats, one unfinished.
- The one unfinished case was rerun using the same compiled native executable,
  same inputs and a 120-minute cap: Ordos level 9, Brutal helper/Hard enemy,
  seed 1, ended in a natural defeat at cycle 320406 (85.44 game minutes).
  Thus 71 distinct scenarios ultimately resolved as 48 wins/23 defeats.
  There were 72 match executions including that replay, no crashes or harness errors.
- Every initial match had a nonempty enemy offensive sortie.
- All 13 Easy/Easy scenarios won; all six Medium/Medium scenarios won.
- All 21 level-9 advanced-helper cases reached at least 15 actual harvesters.
- Across the initial batch, no house had an observed armed/no-damage interval
  exceeding five minutes after minute 15 (metric requires at least 600 army value).
  This metric does not detect low-value stragglers or all forms of ineffective combat.

The extended match was active at the first cutoff: player HP damage rose from
61,980 at 45.13 minutes to 68,715 at 59.32, with enemy damage continuing too.
It was not an entirely frozen match. Original and extended evidence are kept
separately; the latter is a deterministic replay from the start, not a resumed save.

## Remaining worker accounting issue

Eight scenarios reached 16 actual harvesters against a reported engine cap of
15. These were human-helper houses: Harkonnen level-5 Medium/Medium seed 42;
Ordos level-9 Brutal/Easy seed 1, Brutal/Hard seed 42 and Hard/Hard seed 42;
Atreides level-9 Brutal/Hard and Hard/Hard, seeds 1 and 42.

A concrete Atreides Brutal/Hard seed-42 sequence:
Starport worker ordered at 10.61 minutes; refinery ordered at 10.69; factory
worker completed at 10.77; refinery completed at 11.04; Starport worker arrived
at 11.17, with 16 actual workers in the next snapshot.

Source inspection: pending refinery-supplied workers are added to QuantBot's
commitment count only in city simulation, not vanilla campaigns. A completed
refinery can supply a worker before already-paid imports arrive. Starport
delivery honors paid orders rather than rechecking the limit. Fix planning
commitments across these sources; do not delete workers or discard paid cargo.

## Difficulty observations

Fixed Hard helper, Harkonnen, seed 42; player results and game minutes:

| Level | Easy enemy | Medium enemy | Hard enemy | Brutal enemy |
| --- | --- | --- | --- | --- |
| 4 | Win 14.7 | Win 16.7 | Win 15.3 | Win 15.3 |
| 5 | Win 15.9 | Win 18.4 | Loss 12.9 | Loss 17.1 |
| 6 | Win 17.9 | Win 18.6 | Win 19.2 | Loss 19.3 |
| 7 | Win 19.0 | Win 18.8 | Win 19.8 | Loss 17.4 |
| 8 | Win 17.9 | Win 22.2 | Win 33.3 | Loss 13.4 |
| 9 | Win 21.5 | Win 23.0 | Loss 16.6 | Loss 13.3 |

Hard's level-5 spike and late level-9 losses persist, while the fixed helper wins
levels 6–8. Brutal's strong coordinated pressure follows Stefan's explicit
all-in request, so losses alone are not evidence of a defect. Atreides Hard/Hard
and Brutal/Hard level 9 win seed 1 and lose seed 42: house/start/seed sensitivity
matters. Several losing helpers never dispatched a ground assault; they still
defended and dealt damage. Helper readiness remains worth tuning separately.

## Checks and integration

All six CTest targets passed; native dependency audit, signature and version
consistency checks passed. Fresh real-engine pressure, defense and Hard/Brutal
repair fixtures passed in `679-release-fixtures`. Earlier final 679 helper
economy, Starport above-market/first-carryall and pacing fixtures remain passing
on unchanged game code.

Fetched `origin/main` at `49d58ed`: it includes menu navigation and browser mod
packaging, with its own release numbered **1.0.679**. That is a different code
state from this local AI candidate. The branch is 9 commits behind/27 ahead of
that main. Read-only `git merge-tree --write-tree HEAD origin/main` found a
HANDOVER.md content conflict; no source conflict was reported. The merged result
has not been built or tested. A new version and browser/cross-platform validation
are required before claiming a production-ready artifact.

## Evidence and reproduction

- `/tmp/dunecity-campaign-balance/679-release-matrix`: plan, results, per-run
  summaries, native logs, complete structured telemetry.
- `679-release-houses`: additional 12-case plan and equivalent evidence.
- `679-release-extended/ordos-l9-brutal-hard-s1`: 120-minute replay,
  `reproduction.json`, result and telemetry.
- `679-release-main.json/.md` and `679-release-houses.json/.md`: harvested-spice,
  actual HP-damage, loss-value, wave and activity comparisons.
- `679-release-fixtures/results.json`: current pressure/defense/repair checks.
- `/tmp/dunecity-679-merge-assessment.txt`: read-only integration assessment.

The main matrix is reproducible with:
`python3 tests/ai/run-campaign-matrix.py --output-dir NEW_DIRECTORY --workers 2`.
The other-house plan is stored alongside results and uses the same
`run-campaign-balance.py` runner. Each original match uses a 60-minute cutoff.
The extended replay invokes its already-compiled executable with the identical
BALANCE variables except BALANCE_MINUTES=120; no gameplay intervention or forced
victory occurs. The runner uses the real native engine with dummy SDL video/audio
and no frame pacing.
