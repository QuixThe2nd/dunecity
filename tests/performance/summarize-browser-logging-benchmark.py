#!/usr/bin/env python3
"""Summarize paired real-browser diagnostic logging runs; keep raw samples."""
import argparse
import json
from pathlib import Path
import statistics
import re

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory',type=Path)
args=parser.parse_args()
manifest=json.loads((args.directory/'fixture-manifest.json').read_text())
runs=[]
for expected in manifest['runs']:
    path=args.directory/f'run-{expected["run"]}-{expected["logging"]}.json'
    row=json.loads(path.read_text())
    assert row['run']==expected['run'] and row['logging']==expected['logging']
    assert row['save_sha256']==manifest['save_sha256']
    assert row['summary']['frames']==len(row['frames'])>0
    assert all(f['duration']>0 for f in row['frames'])
    measured_ms=sum(f['duration'] for f in row['frames'])
    assert abs(measured_ms-row['summary']['elapsed_ms'])<0.01
    assert abs(measured_ms-manifest.get('seconds',120)*1000)<row['summary']['max_ms']*2
    assert row['initialVisibility']=='visible'
    assert all(x['state']=='visible' for x in row['visibility']), 'Background-throttled run'
    callsites=[re.search(r'wasm-function\[(\d+)\]',s).group(1) for s in row['yieldStacks']]
    assert len(callsites)==8 and len(set(callsites[::2]))==1 and len(set(callsites[1::2]))==1
    assert callsites[0]!=callsites[1], 'Presentation and loop yields were not distinguished'
    start,end=row['measurement_start'],row['measurement_end']
    measured_syncs=[x for x in row['syncs'] if start<=x['start']<end and not x['populate']]
    assert len(measured_syncs)>=3, 'Too few periodic storage syncs'
    assert all(x.get('error') is None for x in measured_syncs)
    traces=[f for f in row['files'] if '/ai-decisions/' in f['path'] or f['path'].endswith('/DuneCity-Performance.log')]
    assert bool(traces)==(row['logging']=='on'), 'Logging state does not match recorded files'
    runs.append({'run':row['run'],'logging':row['logging'],**row['summary'],
        'diagnostic_bytes':sum(f['bytes'] for f in traces),'syncs':len(measured_syncs),
        'max_sync_invocation_ms':max(s['invocation_ms'] for s in measured_syncs),
        'max_sync_elapsed_ms':max(s['end']-s['start'] for s in measured_syncs),
        'long_task_count':len(row['longTasks']),
        'max_long_task_ms':max((x['duration'] for x in row['longTasks']),default=0),
        'canvas':row['canvas']})
assert len({json.dumps(r['canvas'],sort_keys=True) for r in runs})==1
medians={}
for mode in ('on','off'):
    selected=[r for r in runs if r['logging']==mode]
    medians[mode]={k:statistics.median(r[k] for r in selected) for k in
        ('fps','median_ms','p95_ms','p99_ms','max_ms','over_100ms','over_250ms','over_500ms','over_1000ms',
         'diagnostic_bytes','max_sync_invocation_ms','max_sync_elapsed_ms')}
improvement={'fps_percent':100*(medians['off']['fps']/medians['on']['fps']-1),
             'p99_reduction_percent':100*(1-medians['off']['p99_ms']/medians['on']['p99_ms']),
             'over_100ms_reduction_percent':100*(1-medians['off']['over_100ms']/medians['on']['over_100ms']) if medians['on']['over_100ms'] else None}
order='/'.join(r['logging'] for r in runs)
report={'runs':runs,'medians':medians,'observed_change':improvement,
        'method':f'Sequential {order}, same shipped JS/Wasm, save and settings; first {manifest.get("warmup",10)} seconds excluded, {manifest.get("seconds",120)} seconds measured. Frame intervals include browser scheduling and game work. Equal wall time may advance different simulation cycles.'}
(args.directory/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
