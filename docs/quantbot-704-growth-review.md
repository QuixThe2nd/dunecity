# QuantBot 1.0.704: funded openings and ongoing city growth

## Verified failures

Native 703 capture `1789556870788357-0` is All against Atreides, seed1293696382,
vanilla, concrete/degradation enabled, default harvester option. Atreides runs
QuantBot Brutal; four opponents run **AI Player Hard**, not QuantBot Hard.
At4.08min it has97,273 credits, four refineries and no heavy factory. The opening
orders a Starport at1.76min despite this map having no CHOAM catalogue. Its first
heavy order is3.47min. At15.23min it has12 heavy factories and18 Carryalls.

Engine700 (`37e5772`, detached `/tmp/dunecity-700-before-planner`) wins the same
seed/options in34.70min with the former default60-worker ceiling. At15.23min it
has17 heavy factories and34 Carryalls. The current harness links that old engine;
the harness's source-revision metadata names the harness checkout, so it must not
be used as the old engine's revision. This is evidence of a regression, not proof
that a single tuning variable explains the outcome.

Native city capture `1789558312233110-0`, DuneCity seed118157932, is four Brutal
houses: Fremen/team1, Neutral/team2, Atreides/team3, Sardaukar/team4, explicit
harvester100, concrete enabled and degradation disabled. Fremen stalls at roughly
61R/20C/29I, four construction yards, positive R/I demand and1100 free base-rock
tiles. One pass has218 spendable credits, an available100-credit residential lot
(forecast62, score484), but selects a450-credit launcher (score751). The global
reservation repeatedly prevents the cheap lot from using an idle yard.

Loaded harvesters returning to occupied refineries were counted as pressure only
within six tiles. This misses the reason many of them are walking: Carryalls
cannot deliver them to an occupied bay. The travel forecast also ignored existing
Carryalls, understating the unloading capacity a transported fleet needs.

## Changes

- Fund the missing first-heavy prerequisites and four minutes of production
  before choosing a rich custom opening; no map-name or100k-credit special case.
- Check enabled CHOAM catalogue membership in both Starport build paths. A sold-out
  entry can restock; an absent entry cannot. Owning a factory is not an exception.
- Remove map-size/default engine worker ceilings. Respect positive Game Options
  overrides; default/-1 and explicit0 are unlimited. The bot still evaluates
  remaining spice, throughput and its campaign policy when choosing a fleet.
- Protect one demanded, useful R/C/I purchase when routine military saving would
  win the score comparison. Independent factories use the remaining real cash.
  Current power/service/transport and genuine unloading priorities remain.
- Include distant loaded returners in sustained bay pressure, subtract only
  free unbooked bays, and avoid duplicate expansion when another bay is pending.
- Account for the share of workers supported by existing Carryalls in trip
  estimates. Use airspeed plus pickup/landing allowance, never undelivered planes.
  Enable the real backlog build path in vanilla as well as DuneCity.
- Log decisions and their inputs in telemetry14 and SQLite. No save-format change.

## Final-engine validation

All game tests use isolated profiles and never operate the user's native match.

| Check | Result / evidence |
| --- | --- |
| CTest, native Release build | 7/7 groups; `/tmp/ctest-704-final.log` |
| Dependency records, version, signature | Pre/post Ninja check; all metadata1.0.704; codesign verifies |
| Opening/market/limits/field queue | `/tmp/dunecity-704-checked-opening`, passed |
| City shared budget and growth | `/tmp/dunecity-704-growth-spending-city`, passed |
| Vanilla shared budget | `/tmp/dunecity-704-verified-spending-vanilla`, passed |
| Campaign pacing/house targets | `/tmp/dunecity-704-verified-pacing`, passed |
| SQLite decision fields | `/tmp/dunecity-704-validation.sqlite`: protected city purchases at218 and1000 spendable |

The city fixture uses low-value land so its lot scores430 against military500.
It verifies a real zone order at218 credits and both construction and military
production/upgrade at1000, rather than merely checking the scoring function.
Existing checks cover simultaneous yards, MCV saving and additional transport.

| All against Atreides, same seed1293696382 | Result | First heavy order | At15.23min: CY / heavy / bays / Carryalls / workers |
| --- | --- | --- | --- |
| Engine700, former default60 | Win34.70min | 3.47min | 7 /17 /20 /34 /60 |
| Native703, former default60 | Losing/army destroyed by31min | 3.47min | 8 /12 /20 /18 /60 |
| Final704, default unlimited | Win20.17min | 1.44min | 8 /20 /40 /34 /120 |
| Final704, configured60 | Win21.65min | 1.44min | 19 /15 /20 /23 /61 |

Final captures: `/tmp/dunecity-704-match-default` and
`/tmp/dunecity-704-match-60`; old engine `/tmp/dunecity-704-baseline700-live-seed`.
The configured60 run briefly has61 workers: existing concurrent delivery/finished
production semantics can overshoot a configured ceiling. This change does not
rework those semantics. The cap still stops further ordinary production; the
default run is no longer bound by the previous60-worker ceiling.

The city reproduction `/tmp/dunecity-704-match-city` uses the native seed, roster,
teams and options. Between minutes30–35, Fremen orders **61 R/C/I lots and89
combat units**; native703 orders **zero lots and109 combat units**. At29.95min,
Fremen has79R/37C/37I, four CYs,15 refineries,27 workers and25 Carryalls, versus
703's61R/21C/29I, four CYs, seven refineries,20 workers and24 Carryalls.
It loses at38.78min after its base is destroyed. All four bots use the new policy,
so this verifies ongoing construction alongside army production; it is not a
claim that Fremen now wins the FFA or that combat balance is optimal.

### Spending and yard utilisation correction

Counting orders alone does **not** establish a balanced credit allocation.
The same704 Fremen30–35min window commits6,100 credits to R/C/I against31,450
to combat units, plus3,750 to rocket turrets,2,500 to police and1,600 to Carryalls.
Direct R/C/I is only13.4% of those production commitments. These are quoted,
accepted order costs, not measured cash deductions in precisely the same window.

Time-weighted planning samples show the original703 yards were99.5% idle:
all four were idle for roughly294 of the300 seconds. This establishes the
original cause as resource misallocation, not a shortage of yards. In704, the
same four-yard capacity is about88.1% busy,11.1% idle and0.8% upgrading, with
no sample interval where all four are idle. Across20 available yard-minutes,
R/C/I occupies roughly597 seconds and turrets/police461 seconds. Adding yards
would not have fixed703's original failure. Further704 tuning should measure
income, cash allocation and services' share of construction time before changing
yard targets. No further yard-target change was made in this patch.

All three final match captures finish without truncation and pass
`tests/ai/report-spending.py --check`: no missing order links, accounting mismatch,
ordinary overspend or city orders in vanilla. Full reports sit beside each run.

The local app is `build-692/bin/dunecity.app`. No public rollout or replacement of
a running application was performed during this task.
