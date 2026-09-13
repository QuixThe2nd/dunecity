#!/usr/bin/env python3
"""Run real campaign AI with a fixed seed, isolated profile and no human commands.

Uses the existing macOS Ninja build. Access-control relaxation is restricted to
this diagnostic executable; no test hooks are compiled into the shipped game.
--attack-percent sets the legacy Easy fraction (zero disables dispatch); campaign
Easy/Medium use the half-army policy plus alliance caps. Results
and structured AI decision logs remain in --output-dir. This is a simulation
comparison, not a substitute for the browser playtest or human playtesting.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=root / 'build')
parser.add_argument('--output-dir', type=Path, required=True)
parser.add_argument('--level', type=int, choices=range(1,10), default=4)
parser.add_argument('--house', choices=('harkonnen','atreides','ordos'), default='harkonnen')
parser.add_argument('--harvester-limit', type=int, choices=range(-1,101), default=-1)
parser.add_argument('--partner-difficulty', choices=('easy','medium','hard','brutal'), default='easy')
parser.add_argument('--enemy-difficulty', choices=('easy','medium','hard','brutal'), default='easy')
parser.add_argument('--starport-probe', action='store_true', help='Exercise reserved cash with above-normal Starport prices')
parser.add_argument('--helper-economy-probe', action='store_true', help='Verify advanced campaign helper worker investment and paid imports')
parser.add_argument('--stats-probe', action='store_true', help='Verify campaign results with a shared human/AI house')
parser.add_argument('--pressure-probe', action='store_true', help='Verify campaign assault slots, recovery and save state')
parser.add_argument('--defence-probe', action='store_true', help='Verify retaliation and base/harvester reinforcements')
parser.add_argument('--repair-probe', action='store_true', help='Verify experienced campaign bots replace missing repair yards')
parser.add_argument('--pacing-probe', action='store_true', help='Verify enemy worker caps and small-wave readiness')
parser.add_argument('--seed', type=int, default=486409243)
parser.add_argument('--minutes', type=int, default=20)
parser.add_argument('--attack-percent', type=int, choices=range(101), default=25)
args = parser.parse_args()
if not 1 <= args.minutes <= 60 or not 0 <= args.seed <= 0xffffffff:
    parser.error('Use 1–60 minutes and a 32-bit unsigned seed.')
build, out = args.build_dir.resolve(), args.output_dir.resolve()
out.mkdir(parents=True, exist_ok=False)
subprocess.run(['python3',str(root/'scripts/check-build-deps.py'),str(build)],check=True,cwd=root)
target = 'bin/dunecity.app/Contents/MacOS/dunecity'
commands = subprocess.check_output(['ninja','-C',str(build),'-t','commands',target],text=True).splitlines()
main = (root/'src/main.cpp').read_text()
needle = 'int menuResult = MainMenu().showMenu();'
if main.count(needle) != 1:
    raise RuntimeError('Main-menu injection point changed.')
main = main.replace(needle,'int menuResult = runCampaignBalanceProbe();')
main = main.replace('if(shouldPlayIntro && (bFirstInit==true))','if(false && shouldPlayIntro && (bFirstInit==true))')
position = main.index('int main(')
main = main[:position] + '#include "'+str(root/'tests/ai/campaign-balance-probe.inc')+'"\n' + main[position:]
source, obj = out/'balance-main.cpp', out/'balance-main.o'
source.write_text(main)
compile_command = shlex.split(next(line for line in commands if ' -c ' in line and '/src/main.cpp' in line))
for option in ('-include','-MT','-MF'):
    if option in compile_command:
        position = compile_command.index(option)
        del compile_command[position:position+2]
for option in ('-MD','-MMD'):
    if option in compile_command: compile_command.remove(option)
compile_command[compile_command.index('-o')+1] = str(obj)
compile_command[compile_command.index('-c')+1] = str(source)
compile_command.append('-fno-access-control')
app = out/'balance-probe.app/Contents'
(app/'MacOS').mkdir(parents=True)
(app/'Resources').symlink_to(build/'bin/dunecity.app/Contents/Resources')
binary = app/'MacOS/balance-probe'
link = shlex.split(next(line for line in commands if ' -o '+target+' ' in line))
link = link[link.index('&&')+1:]
link = link[:link.index('&&')]
link[link.index('-o')+1] = str(binary)
link = [str(obj) if arg.endswith('/main.cpp.o') else arg for arg in link]
with (out/'build.log').open('w') as log:
    subprocess.run(compile_command,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
    subprocess.run(link,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
env = dict(os.environ,DUNECITY_USERDIR=str(out/'profile'),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',
           BALANCE_LEVEL=str(args.level),BALANCE_PARTNER=args.partner_difficulty,BALANCE_SEED=str(args.seed),BALANCE_MINUTES=str(args.minutes),
           BALANCE_ATTACK_PERCENT=str(args.attack_percent),BALANCE_ENEMY=args.enemy_difficulty,
           BALANCE_HOUSE=str(('harkonnen','atreides','ordos').index(args.house)),BALANCE_HARVESTER_LIMIT=str(args.harvester_limit))
if args.starport_probe: env['BALANCE_STARPORT_PROBE'] = '1'
if args.helper_economy_probe: env['BALANCE_HELPER_ECONOMY_PROBE'] = '1'
if args.stats_probe: env['BALANCE_STATS_PROBE'] = '1'
if args.pressure_probe: env['BALANCE_PRESSURE_PROBE'] = '1'
if args.defence_probe: env['BALANCE_DEFENCE_PROBE'] = '1'
if args.pacing_probe: env['BALANCE_PACING_PROBE'] = '1'
if args.repair_probe: env['BALANCE_REPAIR_PROBE'] = '1'
with (out/'run.log').open('w') as log:
    subprocess.run([str(binary),'--window','--showlog'],cwd=out,env=env,
                   stdout=log,stderr=subprocess.STDOUT,check=True,timeout=600)
results = [line for line in (out/'run.log').read_text().splitlines() if 'CAMPAIGN_BALANCE_RESULT:' in line]
if len(results) != 1: raise RuntimeError('Missing campaign result.')
events = next((out/'profile').rglob('events.jsonl'))
rows = [json.loads(line) for line in events.read_text().splitlines()]
summary = {'result':results[0],'sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
           'workingTreeModified':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True).strip()),
           'metadata':rows[0]['data'],'attacks':[r for r in rows if r['event']=='ground_hunt'],
           'final':[r for r in rows if r['event']=='game_summary']}
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(results[0])
print('Telemetry:',events)
