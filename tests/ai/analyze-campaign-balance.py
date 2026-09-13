#!/usr/bin/env python3
"""Compare actual campaign economics, combat and inactivity, not just outcomes.

Usage: analyze-campaign-balance.py RUN_ROOT --output REPORT_PREFIX
Reads each runner's summary and its telemetry. House-level cumulative ledgers
are counted once, even when the human and AI share a house. Ratios aggregate all
opposing houses, and therefore are not per-bot or symmetric-start comparisons.
"""
import argparse
import json
import re
from collections import Counter
from pathlib import Path


def ledger(data):
    rewards = data.get('combat_rewards', {})
    return {
        'spice': data.get('economy_totals', {}).get('spice_refined', 0),
        'hp_damage': sum(v.get('hp_removed_milli', 0) for v in rewards.values()) / 1000,
        'damage_value': sum(v.get('damage_value_milli', 0) for v in rewards.values()) / 1000,
        'lost_value': sum(v.get('lost_value', 0) for v in rewards.values()),
    }


def analyze(path):
    summary = json.loads(path.read_text())
    match = re.search(r'level=(\d+).*partner=(\w+) enemy=(\w+) cycle=(\d+) outcome=(\w+)', summary['result'])
    if not match:
        return None  # fixtures are not matches
    final = next(e for e in summary['final'] if e['event'] == 'game_summary')
    human = final['data']['local_house']
    houses = final['data']['houses']
    team = houses[str(human)]['team']
    events_path = next(path.parent.glob('profile/ai-decisions/*/events.jsonl'))
    snapshots, attacks, counts, deferred = {}, {}, {}, {}
    for line in events_path.open():
        e = json.loads(line)
        h, kind = str(e['house']), e['event']
        counts.setdefault(h, Counter())[kind] += 1
        if kind == 'state_snapshot': snapshots.setdefault(h, []).append(e)
        if kind == 'ground_hunt' and e['data'].get('members', 0): attacks.setdefault(h, []).append(e)
        if kind == 'attack_deferred': deferred[h] = e['data']
    per_house = {}
    for h, data in houses.items():
        if not any(data.get(k) for k in ('actual', 'built', 'lost')):
            continue
        shots = attacks.get(h, [])
        times = [e['cycle'] for e in shots]
        quiet, longest = 0, 0
        previous = None
        for e in snapshots.get(h, []):
            d = e['data']; own = d.get('house_comparison', {}).get(h, {})
            if previous:
                pcycle, parm, pdmg = previous
                dt = max(0, e['cycle'] - max(pcycle, 15 * 3750))
                if dt and min(parm, d['state']['military']) >= 600 and ledger(own)['hp_damage'] == pdmg:
                    quiet += dt
                    longest = max(longest, quiet)
                else: quiet = 0
            previous = (e['cycle'], d['state']['military'], ledger(own)['hp_damage'])
        per_house[h] = dict(name=data['name'], team=data['team'], **ledger(data),
            sorties=len(shots), first_attack_min=round(times[0]/3750, 2) if times else None,
            mean_wave_units=round(sum(e['data']['members'] for e in shots)/len(shots), 2) if shots else 0,
            mean_wave_value=round(sum(e['data']['value'] for e in shots)/len(shots), 2) if shots else 0,
            max_harvesters=max((e['data'].get('actual', {}).get('31', 0) for e in snapshots.get(h, [])), default=0),
            repair_yards_built=data.get('built', {}).get('11', 0),
            max_repair_yards=max((e['data'].get('actual', {}).get('11', 0) for e in snapshots.get(h, [])), default=0),
            repair_busy_samples=sum(e['data'].get('repair_busy', 0) > 0 for e in snapshots.get(h, [])),
            defence_orders=counts.get(h, {}).get('defence_response', 0),
            retaliations=counts.get(h, {}).get('campaign_retaliation', 0),
            longest_armed_no_damage_min=round(longest/3750, 2),
            final_army=snapshots[h][-1]['data']['state']['military'] if snapshots.get(h) else 0,
            last_deferred=deferred.get(h))
    sides = {}
    for side, predicate in [('player', lambda d: d['team'] == team), ('enemy', lambda d: d['team'] != team)]:
        members = [d for d in per_house.values() if predicate(d)]
        sides[side] = {k:round(sum(d[k] for d in members), 2) for k in
            ('spice', 'hp_damage', 'damage_value', 'lost_value', 'sorties', 'defence_orders', 'retaliations')}
        sides[side]['first_attack_min'] = min((d['first_attack_min'] for d in members if d['first_attack_min'] is not None), default=None)
    # One observer snapshot contains all house ledgers at the same cycle.
    # Never sum duplicate ledgers emitted by separate controllers.
    checkpoints = []
    observer = snapshots.get(str(human), [])
    for minute in (5, 10, 15, 20, 30, 45, 60):
        eligible = [e for e in observer if e['cycle'] <= minute * 3750]
        if not eligible or minute * 3750 > int(match[4]):
            continue
        e = eligible[-1]
        checkpoint = {'minute': minute, 'sample_minute': round(e['cycle']/3750, 2), 'sides': {}}
        for side, own in [('player', True), ('enemy', False)]:
            members = [(h, d) for h, d in e['data']['house_comparison'].items()
                       if (d['team'] == team) == own]
            totals = {k: round(sum(ledger(d)[k] for h, d in members), 2)
                      for k in ('spice', 'hp_damage', 'damage_value', 'lost_value')}
            totals['army'] = sum(d.get('military', 0) for h, d in members)
            totals['harvesters'] = sum(d.get('harvesters', 0) for h, d in members)
            totals['sorties'] = sum(sum(a['cycle'] <= e['cycle'] for a in attacks.get(h, [])) for h, d in members)
            checkpoint['sides'][side] = totals
        checkpoints.append(checkpoint)
    ratios = {k:round(sides['enemy'][k]/sides['player'][k], 3) if sides['player'][k] else None
              for k in ('spice','hp_damage','damage_value','lost_value')}
    return dict(case=path.parent.name, level=int(match[1]), partner=match[2], enemy=match[3],
        cycles=int(match[4]), outcome=match[5], minutes=round(int(match[4])/3750,2),
        seed=summary['metadata']['seed'], source=summary['metadata']['source'],
        version=summary['metadata']['version'], sourceCommit=summary['sourceCommit'],
        workingTreeModified=summary['workingTreeModified'], player_team=team, local_house=human, sides=sides, enemy_player_ratios=ratios,
        houses=per_house, checkpoints=checkpoints, evidence=str(events_path))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    rows=[r for p in sorted(args.root.glob('*/summary.json')) if (r:=analyze(p))]
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_suffix('.json').write_text(json.dumps(rows,indent=2)+'\n')
    lines=['# Native campaign resistance comparison','','Enemy/player ratios combine all enemy houses; starts are asymmetric. HP damage excludes overkill; value damage weights it by unit/building cost. Sorties count real nonempty dispatches, not individual shots. Quiet periods mean at least 600 army value without dealing damage, sampled after minute 15; they flag inspection, not automatic failure.','', '| Case | Outcome/min | Spice E/P | HP damage E/P | Loss value E/P | Sorties P/E | First enemy attack | Longest armed quiet P/E (min) |', '| --- | --- | ---: | ---: | ---: | --- | ---: | --- |']
    for r in rows:
        p,e=r['sides']['player'],r['sides']['enemy']; hs=r['houses']
        player_team=r['player_team']
        quiet=[max((d['longest_armed_no_damage_min'] for d in hs.values() if (d['team']==player_team)==own),default=0) for own in (True,False)]
        f=lambda v:'—' if v is None else str(v)
        ratios=r['enemy_player_ratios']
        lines.append(f"| {r['case']} | {r['outcome']} / {r['minutes']} | {f(ratios['spice'])} | {f(ratios['hp_damage'])} | {f(ratios['lost_value'])} | {p['sorties']}/{e['sorties']} | {f(e['first_attack_min'])} | {quiet[0]}/{quiet[1]} |")
    args.output.with_suffix('.md').write_text('\n'.join(lines)+'\n')
    print(f'Analyzed {len(rows)} matches -> {args.output}.json/.md')

if __name__=='__main__': main()
