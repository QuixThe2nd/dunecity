# QuantBot 1.0.703 regression review

## Evidence from the reported game

Native session `1789545923897200-0`, version 1.0.702, Dune City,
`4P - 192x192 - DuneCity`, seed `1105042893`, harvester override 100.
The white house was Fremen (house 3, team 1), with QuantBot Brutal. Other houses
were Harkonnen/team 4, Ordos/team 3 and Rebels/team 2. Counts below come from the
native decision capture, not screenshots.

The 703 comparison uses the same seed, house/team lineup and options, with all
QuantBots updated and no human commands. It is a new simulation, not a replay
of human input. Times are nearest periodic snapshots.

| Fremen metric | Reported 702, 15.25 min | Final 703, 15.25 min | Reported 702, 19.81 min | Final 703, 19.81 min |
| --- | ---: | ---: | ---: | ---: |
| Construction yards | 1 | 4 | 1 | 4 |
| R / C / I zones | 2 / 0 / 0 | 9 / 3 / 0 | 4 / 0 / 0 | 29 / 10 / 4 |
| Light factories | 4 | 1 | 4 | 1 |
| Heavy factories | 2 | 1 | 4 | 3 |
| Harvesters | 25 | 23 | 43 | 44 |
| Refineries | 4 | 5 | 6 | 7 |
| Carryalls | 1 | 5 | 1 | 12 |
| Repair yards | 0 | 2 | 1 | 2 |
| Rocket turrets | 1 | 1 | 2 | 3 |
| Police stations | 0 | 3 | 0 | 5 |
| Last city tax payment | 22 | 976 | 73 | 4,018 |
| Military value | 13,880 | 7,150 | 28,180 | 16,200 |

This changes early spending toward city/transport/defence investment and away
from excessive light production. The final run is a twenty-minute observation,
not a completed match or proof that Fremen wins. An earlier full four-Brutal
run with the runway/ratio fixes still lost at48.88min; details below distinguish
that result from the last supplier/priority changes. Later light expansion is
still permitted when the funded composition needs it.

## What changed from the older implementation

Baseline is 1.0.700 (`37e5772`), before the common spending planner in 1.0.701
(`c54a344`). Version 1.0.702 (`73741bd`) only established first-Carryall priority.

1. The immediate military purchase shortcut ran before factory upgrades. The
   reported Fremen house never upgraded its heavy or light factories and never
   generated an MCV candidate. Restored progression before ordinary allocation;
   funded vanilla MCV unlocks also precede repeated worker orders.
2. Military deficits used the full military ceiling instead of the funded army
   plan. This inflated light-vehicle demand. Use the funded basis and require
   a real light-unit deficit before another light factory.
3. Factory scores beat city investment, while MCV eligibility required no idle
   construction yard and funding beyond every factory's hypothetical output.
   Demand and usable rock now justify construction capacity; save one useful
   MCV's actual price. Duplicate city factories yield to demanded city capacity.
4. Only the first Carryall was protected. Both Starport and High Tech now supply
   scalable transport targets; queued, paid and in-flight units count. The first
   Carryall precedes even discounted campaign worker imports.
5. Services needed leftover cash. Crime and uncovered-base candidates protect
   their price using existing placement/coverage searches. Unlock one defensive
   construction yard before repeat zoning delays rocket technology.
6. Custom Starports applied a second fixed half-cash ceiling after the shared
   reservation. Removed it; real funds, stock, useful worker capacity and other
   protected purchases still bound imports.
7. Factory expansion now considers cash runway across both unit lines and
   construction. A large opening grant can fund parallel growth despite falling
   cash. Existing unpaid queues are charged once. MCVs are occasional purchases,
   not a continuous military production mix; assuming every heavy factory builds
   MCVs continuously falsely exhausted the forecast runway in an intermediate test.

See [the spending policy](quantbot-spending.md) for decision order, scores,
limits, runway accounting and telemetry. These changes preserve 2x2 zones,
normal prices, deterministic decisions and the save format.

## Support capacity

Initial tuning baselines, not measured optimal ratios:

| Support | Baseline | Queue response |
| --- | --- | --- |
| Carryalls | One per five committed harvesters, plus one per twenty combat vehicles; repair allowance capped at two per operating repair yard | Add one when all are booked and at least two pickups have no assigned carrier |
| Repair yards | One per twenty-five combat vehicles; at least one for a working harvester fleet | Add one when all bays are repairing and at least two damaged vehicles wait |
| Refineries | Worker delivery rate divided by a bay's unloading rate, often roughly one per 4–6 workers | Existing persistent ten-second loaded-worker queue can justify another bay |

Pending capacity prevents duplicate queue-driven purchases. Finished repairs
and unloaded harvesters waiting for return flights count as transport pressure,
not busy repair/unloading work. Infantry and aircraft do not inflate repair
transport needs. Medium vanilla retains its existing restriction on adding new
repair yards; Dune City allows them at every difficulty.

Ratios anticipate demand, while queues react to actual travel/repair load.
Targets can fall after combat losses; existing support units are retained.

## Full-match comparisons before the last transport-supplier fix

Custom opponents are the legacy **AI Player Hard**, not QuantBot Hard. Seed 701.
All against Atreides uses the authored enemy teams and a 100-harvester override.
DuneCity is a four-house free-for-all. Campaign 9 is vanilla Atreides with a
QuantBot Brutal helper versus QuantBot Medium enemies.

| Scenario | 1.0.700 | 1.0.702 | 1.0.703 |
| --- | --- | --- | --- |
| All against Atreides | Won, 32.67 min | Lost, 20.72 min | Won, 31.21 min |
| DuneCity vs AI Player Hard | Won, 34.71 min | Won, 30.79 min | Won, 33.76 min |
| Campaign 9 | Not compared | Not compared | Won, 20.55 min |
| Reported four-Brutal setup | Not compared | Human-played defeat | Automated defeat, 48.88 min |

The following snapshot also belongs to the earlier full-match comparison, before
the last supplier/priority changes. At 15.23 minutes:

| Version | Yards | R / C / I | Light factories | Carryalls | Police / rockets | Last tax payment |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1.0.700 | 1 | 8 / 2 / 0 | 1 | 2 | 0 / 0 | 905 |
| 1.0.702 | 1 | 2 / 0 / 0 | 3 | 1 | 0 / 0 | 35 |
| 1.0.703 | 4 | 16 / 6 / 1 | 1 | 3 | 0 / 2 | 2,374 |

By 29.92 minutes, 703 has 150 zones, 18 police stations, 20 rocket turrets and
10,119 tax per payment. Win time alone concealed the city-building regression.
The support targets are not instant guarantees: at that point this house has
seven Carryalls against a target of nineteen and twenty-eight waiting pickups.
The detailed decision says Carryalls were sold out and no High Tech Factory existed. This exposed a further supplier-unlock gap; the final fix makes an unmet transport target fund that first factory when imports are unavailable. A real-engine regression covers it. Supplier availability, losses and competing cash demand still matter.

## Final twenty-minute checks

After the sold-out-transport supplier fix and higher priority for demanded city
MCVs (5,000, below the first Carryall at5,500), both cases were rerun for twenty
simulated minutes. Both completed with clean spending audits. These are bounded
runs and have no victory/defeat result.

| House/setup at nearest 20-minute snapshot | Yards | R / C / I | Harvesters / refineries | Carryalls / target | Police / rockets | Last tax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Reported Fremen/four Brutal | 4 | 29 / 10 / 4 | 44 / 7 | 12 / 12 | 5 / 3 | 4,018 |
| Atreides vs AI Player Hard | 4 | 11 / 4 / 0 | 40 / 6 | 12 / 12 | 0 / 3 | 1,184 |

The second case now builds High Tech at10.59min and meets transport demand,
but city growth is slower than in the earlier run that under-supplied transport.
It had only3R at15.23min and15zones at19.79min. This is a remaining tuning tradeoff,
not evidence that all economy timing is settled. The reported Fremen case has
43zones by19.81min, compared with4 in the original game. Neither final test
returns to the original one-CY/one-Carryall/no-city-growth failure.

Final evidence: `/tmp/dunecity-703-balanced-{city,current,shared}`. The shared
engine fixture verifies that a sold-out Starport funds a first High Tech Factory,
and that a funded needed MCV takes priority over that additional supplier. Both
simulations log remaining pickup queues even when the baseline ratio is met;
idle/unbooked suppliers do not by themselves justify buying still more.

## Validation and reproduction

Native build `build-692/bin/dunecity.app` is 1.0.703. Seven CTest groups pass;
pre/post Ninja dependency checks and version consistency pass. The real-engine
shared-spending and Starport probes pass in both mods, including exact worker
savings, simultaneous yards, depleted spice, factory upgrades, MCV savings,
additional transports, defensive-yard upgrades, bulk imports and real quotes.
The mixed-investment assertion checks workers plus troops and exact cash, not
an obsolete fixed half-budget split.

Telemetry 13 (`city-capacity-recovery-v69`), capital-plan schema 2, exposes active
burn, sustained unit/construction costs, projected cash, runway, supplier queues,
support targets and crime. SQLite capital-plan views expose these inputs.
Spending audits check links, ordinary overspending, accounting reconciliation,
vanilla zoning and truncated capture. Final completed captures pass the audit.

Final simulation/probe evidence:
- `/tmp/dunecity-703-release-check/{all-100,dunecity,current-seed}`
- `/tmp/dunecity-703-final-carryall-campaign9`
- `/tmp/dunecity-703-verified/starport-{vanilla,dunecity}`
- `/tmp/dunecity-703-final-check-shared-{vanilla,dunecity}`

The full-match table above covers the corrected runway/ratio implementation, before the final sold-out-transport supplier fix. Custom matches also preceded the first-Carryall import ordering correction; that correction affects the campaign bargain-worker branch only. Campaign and Starport probes were rerun after the ordering fix. The final supplier and construction-priority changes are checked separately below; do not treat the earlier full-match outcomes as reruns of that last change. Earlier exploratory 703 runs are excluded. Large city battles were slow; a sample attributed the
hot path to existing unit target searches, not the new build planner.

```sh
python3 tests/ai/run-campaign-balance.py --build-dir build-692 \
  --output-dir /tmp/new-city-reproduction --mod dunecity --house fremen \
  --custom-map 'data/maps/multiplayer/4P - 192x192 - DuneCity.ini' \
  --roster 'harkonnen:4,ordos:3,rebels:2,fremen:1' \
  --partner-difficulty brutal --enemy-ai quantbot --enemy-difficulty brutal \
  --harvester-limit 100 --seed 1105042893 --minutes 60
```

One deterministic seed is evidence of behaviour, not proof of overall balance.
The ratios and four-minute forecasts need further playtesting across layouts,
army compositions, stock availability and travel distances. Future continuous
repair spending and Carryall routing are not fully forecast.
