# Advanced campaign helper economy — 1.0.679

Stefan reported that his Brutal level-9 helper would not buy enough harvesters,
requiring manual Starport orders. He closed the game before inspection; its
exact settings/state were not recovered. The following is a native reproduction,
not a claim to have observed his closed game.

## Findings and changes

Baseline 678 Harkonnen/Vanilla level 9, Brutal helper/Hard enemy, seed 42:
the helper had an engine/AI ceiling of 15 workers, but dividing remaining spice
among four houses reduced its target to 9 at 10.13 minutes and 6 at 12.16 minutes
(41,474 spice still present). It had 11 workers at both checkpoints.

Game commit `83f6264`:

- Hard/Brutal full human-side campaign helpers in the vanilla economy budget
  against remaining map spice without an equal allocation to enemy houses.
  They aim for their permitted harvester capacity while sufficient spice remains
  (1,500 remaining spice per worker); normal engine/AI limits and depletion apply.
- Needed Starport imports are planned before optional producers/construction,
  then heavy factories. Factory worker priority no longer waits for army-capital
  ratios or an extra 1,000 credits of spare cash for these helpers.
- Existing above-normal-price buying and first-carryall support remain.
- Queued Starport cargo is already paid in `StarPort::doProduceItem`; remove its
  cost from unpaid production reservations while still counting incoming units.
  This accounting correction applies to every QuantBot role.
- Easy/Medium helper investment and enemy harvester policies are unchanged.
  High-difficulty enemy assault commitment from 678 remains enabled.

## Validation

Release build, pre/post Ninja dependency audits, signature, version consistency
and all six CTest targets passed. Final native helper-economy fixtures pass on
both Hard and Brutal: eight current workers buy five at 900 credits each, then a
second port buys the remaining two with exactly 1,800 fresh credits while the
first five are still in transit. Total committed workers reaches 15, prices are
above normal, and paid cargo neither consumes cash twice nor gets duplicated.
Existing first-carryall/above-market Starport and Easy reserve/pacing fixtures pass.
Fixture evidence: `/tmp/dunecity-campaign-balance/679-probes` (use `*-final` for
helper fixtures). Earlier fixture attempts failed their unrelated factory-order
expectation; final fixture `097bc34` directly verifies the paid-cargo behavior
with a second Starport. No game-code changes followed `83f6264`.

Six real native level-9 Harkonnen matches, no player orders/skips, normal initial
campaign branch, dummy SDL/no frame pacing. All reached 15 actual harvesters.
First-15 times are observer snapshots, not exact production-completion cycles.

| Helper | Enemy | Seed | First observed 15 workers (min) | Player result / min |
| --- | --- | ---: | ---: | --- |
| Hard | Hard | 1 | 11.15 | Defeat / 13.88 |
| Hard | Hard | 42 | 11.65 | Defeat / 16.59 |
| Brutal | Hard | 1 | 11.15 | Defeat / 13.88 |
| Brutal | Hard | 42 | 11.65 | Defeat / 16.59 |
| Brutal | Easy | 42 | 10.64 | Victory / 22.32 |
| Brutal | Brutal | 42 | 11.65 | Defeat / 13.30 |

Direct Brutal/Hard seed-42 comparison: 8.11 minutes, 7 -> 9 workers;
10.13 minutes, 11 -> 12; 12.16 minutes, 11 -> 15. Target at 12.16 rises from
6 to 15. The stronger economy alone does not solve survival against the new
large enemy attacks: five defeats, one victory, no timeouts. None of the five
losing helper runs launched a ground assault before defeat. Helper readiness
and combat reliability remain separate open issues.

Evidence: `/tmp/dunecity-campaign-balance/678-helper-baseline` and `679-final`,
with per-run `summary.json` and `profile/ai-decisions/*/events.jsonl`.
Aggregate damage/spice analysis: `679-resistance.json`/`.md` in that parent.
Diagnostic fixture revisions happened during the batch; all matches use the
same built 679 gameplay objects. Per-run metadata preserves the source state.

Local branch `fix/campaign-ai-attack-limits`; native app rebuilt at
`build/bin/dunecity.app`. No push, merge or browser/public deployment this turn.
