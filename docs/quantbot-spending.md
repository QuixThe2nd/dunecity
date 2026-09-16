# QuantBot shared spending — 1.0.701

A deterministic, four-simulated-minute forecast compares the next economy,
combat-unit and additional-production investments. It is a bounded scoring
model, not a full mathematical optimizer or a prediction of combat outcomes.
No new saved AI state or save-format change is required.

## Cash and independent queues

Spendable cash is current credits less unpaid production and upgrades already
queued. Paid Starport cargo counts toward unit totals without reserving its
price twice. Every newly accepted order, foundation, upgrade and road batch
charges this same planning budget exactly once. Starport budgeting uses the
port's displayed offer, which is the price its queue actually charges; market
quotes can refresh before the build list does.

Protect the price of the highest-scoring next purchase (plus needed concrete).
Other yards/factories can use the remaining credits immediately. Construction
therefore runs alongside factory harvesters and military production when funds
permit. Saving for a worthwhile worker does not reserve the cost of an entire
hypothetical future fleet. Existing human-allied campaign full-fleet rules are
an explicit exception; custom allies and opponents use normal shared spending.

## Comparison

| Investment | Forecast and priority |
| --- | --- |
| Harvester | Additional receipts after production/delivery, accounting for the existing fleet, unloading capacity, travel and remaining spice. Must recover its purchase cost within the horizon. |
| Refinery | Additional unloading capacity and the included worker; excludes existing income. Actual loaded-worker queues can justify a bay even as unharvested spice declines. |
| Dune City R/C/I | Demand/site-supported growth after construction and growth delay, net of power upkeep, with allocated generation/foundation cost. Existing unfinished plots reduce confidence. No fixed tax-to-spice ratio gate. |
| Combat unit | Military value per purchase credit, weighted by the fraction of the army target still missing. Active attacks on the base/workers raise defence priority. Existing composition and difficulty limits remain. |
| Extra factory | Only if existing lines are busy and forecast funding/army shortfall exceed their production capacity plus the new building's cost. Idle factories do not justify more factories. |
| Extra construction capacity | Existing rock/map/yard rules, busy construction or rock shortage, and enough forecast funding while preserving working cash. |

Economy score is forecast net receipts divided by total capital cost (scaled
by 1,000, capped at 4,000). Normal military score is value/price multiplied by
the army shortfall in thousandths; active defence adds 4,000 before applying
value/price. Additional-factory score uses only the extra military production
that can actually be funded beyond existing capacity. Stable ordering resolves
ties; forecast income is never treated as cash available to spend now.

Spice receipts compare the current fleet with the expanded fleet over the same
remaining resource pool, including harvesting during delivery delay. This
reduces the value of extra harvesters as the existing fleet can exhaust the
remaining fields. The travel estimate is a bounded local sample, not a new
pathfinding pass, and does not explicitly model carryall routing. Forecasts
remain estimates; use delivered-spice and tax telemetry to assess their error.

Dune City includes R/C/I, tax and municipal/power expenses. Vanilla has no zone
candidates or tax forecast. Its configured power rules still apply. First
income/technology prerequisites, demanded civic buildings, replacement of lost
infrastructure, power recovery and campaign-authored restrictions retain their
existing rules. Emergency generator orders can commit more than current cash;
ordinary purchases cannot. Running repair bills are not reserved in full.

## Decision capture and SQLite

Telemetry version 12, policy `shared-capital-spending-v68`, records:

- `capital_plan`: cash/commitments, horizon, resource and income estimates,
  military target/current value, producer queues, all common spending
  candidates, unit-mix deficits, prices, scores, eligibility and chosen option.
- `city_economy_comparison` and `zone_evaluation`: detailed tax/refinery
  forecasts, demand, growth confidence and placement rejection reasons.
- `production_order`: accepted/rejected queue order, actual quote, rule,
  builder, state and `capital_plan` sequence reference.
- `capital_upgrade`, `capital_road_batch`, `capital_order_blocked` and
  `capital_outcome`: auxiliary commitments, blocked purchases, selected-order
  fulfilment and remaining budget.

All are written to the existing native/browser session JSONL and imported into
SQLite by the existing importer. Join references with both session and sequence.
New views: `capital_plans`, `capital_candidates`, `capital_orders`,
`capital_outcomes`. `capital_orders` includes successful production, upgrades and
road batches. Detailed rejection/state payloads remain in `events.data`.

```sh
python3 scripts/ai-decisions.py --db /tmp/game.sqlite import /path/events.jsonl
python3 scripts/ai-decisions.py --db /tmp/game.sqlite query "
 SELECT p.house,c.kind,c.item,c.score,c.reason,p.spendable,p.spice_share
 FROM capital_plans p JOIN capital_candidates c USING(session,plan)
 WHERE c.selected=1 ORDER BY p.cycle"
python3 tests/ai/report-spending.py /path/events.jsonl --output /tmp/spending.json --check
```

The audit checks plan/order links, ordinary overspending, accepted-cost
reconciliation, and accidental vanilla zone orders. It reports capture
completeness and refuses a clean audit on a capture-limit marker. Session
capture remains bounded (normally 256 MiB); reaching the routine-event allowance
now emits an explicit marker while retaining room for terminal summaries.

## Reproducible validation

The engine probe exercises exact worker savings, simultaneous construction,
depleted spice, cash-starved factories and funded production expansion in both
mods. The Starport probe also covers stale market/display quotes and legacy
campaign/custom bulk-import rules. Tests run against an isolated profile.

```sh
python3 tests/ai/run-campaign-balance.py --build-dir build-692 \
  --output-dir /tmp/new-spending-test --level 9 --mod dunecity \
  --house harkonnen --partner-difficulty brutal --enemy-difficulty hard \
  --seed 701 --minutes 1 --shared-spending-probe
```

Repeat with `--mod vanilla`; use `--starport-probe` for imports. The runner also
accepts `--custom-map PATH` for two-house custom simulations. A time-limited
simulation is behavioural evidence, not proof of balance across all maps.
