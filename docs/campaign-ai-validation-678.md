# Campaign army commitment — 1.0.678

Stefan requested all-in Brutal enemy attacks and most-in Hard enemy attacks.
Game commit `063a7f5`; native simulation harness snapshot `4d77832`.
This is local only; browser/public deployment was not changed.

## Behavior

- Campaign enemies use 80% combat-army value on Hard and 100% on Brutal,
  without the former shared count/value ceiling or per-house subdivision.
- Hard permits two simultaneous attacking houses; Brutal permits all houses.
- Easy/Medium keep half-army commitment, small count/value caps and one house.
- The alliance's easiest living tier determines its policy for mixed tiers.
- Explicit zero configured attack ratio still disables dispatch. Older nonzero
  saved ratios use the new campaign policy. Other game modes and human-side
  helpers retain their existing behavior, including helper reserves/readiness.
- Available attackers exclude units already fighting, repairing, badly damaged
  or under manual control. Workers/transport units are not assault candidates.
- Opening timing, recovery, sortie expiry, no top-ups and defensive response
  remain in force. Readiness retains the existing mission-scaled values;
  removing wave caps does not introduce an unlimited army readiness threshold.
- Telemetry reports `-1` for absent count/value ceilings. Profile `units/value`
  on Hard/Brutal remain readiness reference values, not active assault limits.

## Verification

Native Release build, pre/post dependency audits, signature and six CTest targets
passed. Real-engine pressure, defense and pacing fixtures passed under
`/tmp/dunecity-campaign-balance/678-probes`.

The pressure fixture gives each of four enemies 40 actual tanks. Hard sends 32
from each of two houses, leaving the other two waiting. Brutal sends all 40 from
all four houses. Repeat dispatch cannot top up; a separate Brutal check excludes
manual/repairing/damaged troops and sends the other 37. Existing opening,
recovery, save/load, beginner reserve and defense checks remain covered.

The first six full-match attempts were aborted by the diagnostic harness's old
unconditional count/value assertion. `4d77832` updates it to enforce those caps
only where enabled, still checking house concurrency for all tiers. Those runs
are incomplete and are not counted. The final clean-source batch is below.

## Complete matches

Harkonnen, Vanilla, fixed Hard full helper, seed 42, initial campaign branch;
native real engine with dummy SDL and no frame pacing, no player orders or skips.
Player-side outcomes, game minutes; combined enemy/player ratios. HP damage is
actual hostile HP removed on priced targets, excluding overkill.

| Level | Enemy | Player outcome | Minutes | Enemy/player spice | Enemy/player HP damage | Enemy sorties | Largest wave units/value |
| --- | --- | --- | ---: | ---: | ---: | ---: | --- |
| 5 | Hard | Defeat | 16.39 | 1.356 | 8.846 | 2 | 51 / 13,500 |
| 5 | Brutal | Defeat | 12.81 | 1.923 | 7.193 | 1 | 50 / 12,500 |
| 8 | Hard | Victory | 23.94 | 0.889 | 0.417 | 6 | 43 / 11,800 |
| 8 | Brutal | Defeat | 19.02 | 1.587 | 1.538 | 4 | 36 / 13,050 |
| 9 | Hard | Defeat | 16.68 | 2.822 | 1.462 | 4 | 27 / 8,050 |
| 9 | Brutal | Defeat | 13.47 | 3.924 | 2.821 | 3 | 37 / 10,800 |

All six completed naturally; five defeats, one victory, no timeouts. Native
source and settings are recorded per run. Evidence:
`/tmp/dunecity-campaign-balance/678-final/*/{summary.json,profile/ai-decisions/*/events.jsonl}`;
analysis `/tmp/dunecity-campaign-balance/678-resistance.{json,md}`.

Compared with the same 677 cases, Hard's level-5 and level-9 player victories
became defeats; Brutal defeats occurred substantially earlier. This implements
the requested commitment and demonstrates strong resistance, not finished human
balance. The helper did not launch a ground sortie before four of these defeats.
No individual enemy armed/no-damage interval exceeded 1.01 minutes after minute
15 in these runs, but changed battles do not prove the previously identified
ineffective-route problem is fixed. Several games ended before minute 15.

Reproduce each case with `tests/ai/run-campaign-balance.py --level LEVEL
--partner-difficulty hard --enemy-difficulty TIER --seed 42 --minutes 60
--output-dir NEW_DIRECTORY`. Run the analyzer on the parent directory. Broader
seed/house tests, helper readiness and attack-progress recovery remain separate
work; no changes to those were bundled into this commitment adjustment.
