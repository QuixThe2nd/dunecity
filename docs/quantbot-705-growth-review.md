# QuantBot 1.0.705: dedicated city construction

## Report and cause

The native 704 DuneCity game (`1789562162651301-0`, seed `705667278`) had
Harkonnen, Ordos, Neutral and Rebels, all QuantBot Brutal on separate teams,
with an explicit 100-harvester limit. At about 33.77 minutes Harkonnen had four
construction yards, 98 residential / 40 commercial / 41 industrial zones,
1,439 free base-rock tiles and 18,424 spendable credits. It was expanding, but
other construction repeatedly took the yards away from city growth.

Over the preceding five minutes, planning samples weighted by elapsed time
showed approximately 41.4% zoning, 48.4% other construction and 10.2% idle.
The concrete allocation bug was that protecting a plot's cash did not bind the
builder to that plot: its later choice could spend the budget on crime protection
or air defence. The old idle fallback also depended on skip flags and more than
200 credits, and called a selector that could return a refinery instead of R/I/C.

## Change

- With multiple usable construction yards, dedicate the oldest to demanded R/I/C.
  Finish its existing order first; incidental zoning by another yard does not
  divert it into services. Recompute the assignment from actual builders.
- Bind the protected purchase to the actual yard decision. Other yards remain
  available for services, power, factories, refineries and more zones.
- If an otherwise idle yard cannot afford or place its preferred project, try a
  legal demanded R/I/C plot using its actual price, including exactly 100 credits.
- Keep demand, available cash, placement and 24 spare power checks. Do not spend
  forecast income or unpaid commitments. A lone opening yard keeps its essential
  opening progression and gains the idle fallback.

No new saved state, random spending personality, construction-yard target or
difficulty setting was added. Vanilla has no city allocation.

Telemetry version 15, policy `dedicated-city-growth-v71`, records
`city_growth_dedicated_yard`, `city_growth_yards_busy` and `city_growth_builder`.
Orders identify `dedicated_city_growth` and `idle_city_growth`; SQLite exposes
the assignment fields in `capital_plans`.

## Validation

The shipped build passed all seven CTest groups, pre/post Ninja dependency
checks, version agreement and codesign verification. Real-engine spending
fixtures passed in DuneCity and vanilla on level 9 / seed 701. The new city cases
exercise simultaneous zoning and crime protection, a second yard already zoning,
exactly 100 credits for another plot, insufficient cash, negative demand,
blackout recovery and no legal land. Existing worker, military, MCV, factory
upgrade and transport checks remain active. Fixtures use isolated profiles and
do not add hooks to the shipped executable.

The city fixture prepares fully upgraded yards so crime construction is available;
otherwise a yard correctly chooses its prerequisite upgrade. The full probe
requires level 9, as documented: level 4 cannot unlock its later rocket test.

Compare both engines for 30 simulated minutes using the native map, seed, roster,
harvester limit and no concrete degradation:

```sh
python3 tests/ai/run-campaign-balance.py --build-dir build-705 \
  --output-dir /tmp/new-705-city-comparison --mod dunecity \
  --custom-map 'data/maps/multiplayer/4P - 192x192 - DuneCity.ini' \
  --roster harkonnen:1,ordos:2,neutral:3,rebels:4 \
  --partner-difficulty brutal --enemy-difficulty brutal --harvester-limit 100 \
  --no-structures-degrade-on-concrete --seed 705667278 --minutes 30
```

The baseline used the preserved 704 engine objects in `build-692`; session
metadata confirms the actual engine versions. Both ended at cycle 112500 with
`time_limit`, complete captures and zero spending audit violations.

| House | 704 R/I/C orders, 0-30 min | 705 R/I/C orders, 0-30 min | 704 orders, 25-30 min | 705 orders, 25-30 min |
|---|---:|---:|---:|---:|
| Harkonnen | 150 | 165 | 52 | 62 |
| Ordos | 104 | 212 | 0 | 66 |
| Neutral | 156 | 198 | 56 | 61 |
| Rebels | 148 | 227 | 60 | 67 |

These are accepted zone orders, not surviving completed zones or population.
Ordos was defeated in the baseline; its difference is confounded by combat.
All four bots changed, so one seeded comparison does not establish better combat
balance. For Harkonnen, zoning's share of sampled yard time in minutes 25-30 rose
from 39.7% to 49.8%, while idle time was 11.5% versus 11.1%. The improvement mainly
comes from assigning more construction time to city growth.

Local evidence:

- Frozen current-game capture: `/tmp/dunecity-705-native704-city.jsonl`.
- Baseline: `/tmp/dunecity-705-baseline704-city/profile/ai-decisions/1789564329223373-0/events.jsonl`.
- Final 705: `/tmp/dunecity-705-city-final/profile/ai-decisions/1789564659623619-0/events.jsonl`.
- Per-house comparison: `/tmp/dunecity-705-city-comparison.json`.
- Real-engine probes: `/tmp/dunecity-705-growth-city` and `/tmp/dunecity-705-growth-vanilla`.
- Imported final match: `/tmp/dunecity-705-validation.sqlite` (4,497 capital plans).
- CTest: `/tmp/ctest-705-final.log`.

The local app is `build-705/bin/dunecity.app`. The ongoing 704 game in `build-692`
was not stopped or replaced. No public deployment was performed.
