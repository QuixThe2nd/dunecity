#!/usr/bin/env python3
"""Audit the core campaign PAK used by vanilla and Dune City (not Tornie's loose INIs)."""
import argparse
import configparser
import json
from pathlib import Path
import re
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pak', type=Path, default=Path(__file__).resolve().parents[2] / 'data/SCENARIO.PAK')
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
data = args.pak.read_bytes()
entries, pos = [], 0
while True:
    offset, = struct.unpack_from('<I', data, pos)
    pos += 4
    if not offset:
        break
    end = data.index(b'\0', pos)
    name = data[pos:end].decode('ascii')
    pos = end + 1
    entries.append((name, offset))
rows = []
for index, (name, offset) in enumerate(entries):
    if not re.fullmatch(r'SCEN[AHO]\d{3}\.INI', name, re.I):
        continue
    end = entries[index + 1][1] if index + 1 < len(entries) else len(data)
    raw = data[offset:end].decode('latin1')
    # Some original PAK entries contain non-INI padding after their last key.
    text = '\n'.join(line for line in raw.splitlines()
                     if re.match(r'^\s*(\[[A-Za-z]+\]|[A-Za-z0-9_]+\s*=)', line))
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.read_string(text)
    human = next(section for section in ini.sections() if ini[section].get('Brain', '').lower() == 'human')
    reinforcements = next((section for section in ini.sections() if section.lower() == 'reinforcements'), None)
    attacks = []
    for key, value in ini[reinforcements].items() if reinforcements else []:
        parts = [part.strip() for part in value.split(',')]
        if len(parts) not in (4, 5):
            raise ValueError(f'{name}: invalid reinforcement {key}={value}')
        house, unit, location, when = parts[:4]
        if house.lower() == human.lower() or location.lower() == 'homebase' or unit.lower() in ('harvester', 'carryall', 'mcv', 'sandworm'):
            continue
        attacks.append(dict(house=house, unit=unit, location=location, minute=int(when.rstrip('+')), repeat='+' in value))
    hunts = [value for value in ini['UNITS'].values()
             if 'hunt' in value.lower() and not value.lower().startswith(human.lower() + ',')] if 'UNITS' in ini else []
    first=min((attack['minute'] for attack in attacks), default=None)
    early_signal=first is None and int(name[5:8])<=4
    rows.append(dict(map=name, human=human, first_attack=first,
                     opening_signal_minute=4 if early_signal else first,
                     opening_signal_type='existing_troops' if early_signal else 'reinforcement',
                     attacks=attacks, initial_hunts=len(hunts)))
rows.sort(key=lambda row: row['map'])
expected = {f'SCEN{house}{mission:03}.INI' for house in 'AHO' for mission in range(1, 23)}
if {row['map'].upper() for row in rows} != expected:
    raise ValueError('Core campaign audit must cover exactly all 66 maps')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(dict(source=str(args.pak), maps=rows), indent=2) + '\n')
missing = [row['map'] for row in rows if row['first_attack'] is None]
print(f'{len(rows)} maps checked; {len(rows)-len(missing)} have offensive reinforcements; {len(missing)} missing.')
for name in missing:
    print(name)
uncovered=[row['map'] for row in rows if row['opening_signal_minute'] is None]
if uncovered:
    raise ValueError(f'Campaign maps without an opening signal: {uncovered}')
print('All 66 maps have an opening signal, including the 12 early-map existing-troop signals.')
