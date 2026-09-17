# QuantBot 1.0.706: restore funded production alongside city growth

## Evidence and cause

The native 705 capture `1789589460739178-0` is Dune City, All against
Atreides, seed 906213928. At about six minutes Atreides still had one heavy
factory and one repair yard with 84,862 credits. The second heavy factory was
ordered at 6:37. This was a construction-allocation regression, not a shortage
of credits or an engine unit cap. The earlier 704 rich-opening victory test
used vanilla; that did not cover this Dune City production path.

The shared planner explicitly blocked duplicate factories until the city-yard
target was met. Even after removing that gate, its per-yard score choice and
late factory cancellation displaced the established cash-funded production
order. The economic factory shortcut also ran before normal upgrades, MCVs
and army production. A further opening-worker gate could delay affordable MCVs
while unloading capacity prevented reaching that worker target.

## Change

- Factory expansion no longer requires reaching the construction-yard target.
- In city custom games, current cash and the four-minute forecast must cover
  another heavy factory, its operating cost and the working buffer. When they
  do, use the existing factory/repair expansion order and normal heavy-factory
  upgrade/MCV/worker/military production path. Do not subsequently cancel those
  funded factory orders through the constrained-spending score check.
- In that funded case, an affordable MCV need not wait for the opening worker
  target. Construction and factory capacity can grow together.
- Keep 705's dedicated R/C/I yard and idle-yard growth fallback. Add repair
  capacity using the existing fleet/queue target alongside factory expansion.
- Reserve a legal heavy-factory plot and a legal repair-yard plot before zoning,
  with a one-tile access margin. Recompute from actual land and commitments each
  pass. Only zoning is excluded after planning; infrastructure can use the plots.

No new difficulty ratios, military limits, harvester caps, tactics, map-name
exceptions or save fields. Vanilla retains its existing funded-production path.
Telemetry 16 / `funded-parallel-city-production-v72` records the funded decision,
required runway and reserved plots; SQLite exposes the new capital fields.

## Matched comparison

All against Atreides, Dune City mod, seed 906213928, QuantBot Brutal Atreides
against four legacy **AI Player Hard** opponents; concrete required, no concrete
degradation, explicit harvester option 100, no human commands, maximum 30 minutes.
The native player's run is diagnostic evidence, not a bit-identical replay.

| Engine | Cash at 15:14 | Army value including queued units | Delivered heavy factories | Delivered repair yards | Delivered R/C/I plots | Result |
|---|---:|---:|---:|---:|---:|---|
| 700, before shared spending | 26,431 | 65,600 | 11 | 4 | 21 | Survived through 30-minute limit |
| 705 baseline | 97,112 | 29,150 | 7 | 2 | 127 | Lost at 19:07 |
| 706 | 92,699 | 79,570 | 20 | 8 | 111 | Won at 21:41 |

At that same sample, fielded army values excluding queued units were 61,100,
26,000 and 74,870 respectively. Thus the 706 result is not just orders waiting
in factories. The city retained tax growth while adding production. One fixed
seed establishes this regression's recovery; it is not a general win-rate claim.

700 engine source is `37e5772`, built in
`/tmp/dunecity-700-before-planner/build-local`. Its comparison used the diagnostic
harness from the 705 source; harness metadata must not be mistaken for engine
source provenance. 705 source is `bf7135b`. The 706 tests used the final working
tree committed with this note, in a separate build from the running native game.

| Capture | Session |
|---|---|
| `/tmp/dunecity-706-old700-city` | `1789590148125667-0` |
| `/tmp/dunecity-706-baseline705-city` | `1789590386463338-0` |
| `/tmp/dunecity-706-final-match` | `1789592009292869-0` |

Reproduce the final comparison with a new output directory:

```sh
python3 tests/ai/run-campaign-balance.py --build-dir build-706 \
  --output-dir /tmp/new-706-production-comparison --mod dunecity \
  --custom-map 'data/maps/singleplayer/5P - 128x128 - All against Atreides.ini' \
  --house atreides --partner-difficulty brutal --enemy-ai ai-player \
  --enemy-difficulty hard --harvester-limit 100 \
  --no-structures-degrade-on-concrete --seed 906213928 --minutes 30
```

## Validation

- All seven CTest groups pass, plus pre/post dependency checks, version agreement
  and app signature verification.
- Real-engine level-9 shared-spending fixtures pass in both mods. Added city
  cases cover a funded second factory before the yard target, simultaneous
  dedicated zoning, preserved factory/repair footprints, and an MCV while the
  opening worker target is unmet. Existing low-cash, crime, power, no-land,
  upgrade, worker, transport and queued-cost checks remain active.
- The rich/poor-opening fixture passes, including the disabled Starport market
  on this map, explicit/default harvester options and refinery return queues.
- A 30-minute four-house Dune City free-for-all completed, all QuantBot Brutal,
  seed 705667278, normal map roster (Atreides/Harkonnen/Ordos/Sardaukar), same
  concrete/harvester options. All four retained four construction yards and
  reached 153-180 delivered R/C/I plots, with 3-6 heavy factories and 4-5 repair
  yards at the final sample. This is a separate smoke test, not the earlier
  705 native Harkonnen/Ordos/Neutral/Rebels roster comparison.
- Final match, free-for-all, both spending fixtures and opening fixture pass the
  spending audit: no broken plan links, overspending or city orders in vanilla.
  Captures end normally; none hit the logging limit. The final city fixtures and
  matches import into SQLite, including the new fields.

Final fixture sessions: city `1789592011560526-0`, vanilla
`1789592150614571-0`, opening `1789591687535775-0`. The opening fixture predates
only the final funded duplicate-factory cancellation bypass; it exercises the
first-factory path, which that bypass cannot affect. Free-for-all session:
`1789592010900394-0`.

Local app: `build-706/bin/dunecity.app`. The user's running app remains
`build-705/bin/dunecity.app`; it was not stopped or overwritten. No remote push
or public deployment for this patch.
