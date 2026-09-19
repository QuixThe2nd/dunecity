#!/usr/bin/env python3
"""Exercise live joining over real local PHP and WebRTC, then compare resumed game state.

Requires the existing macOS Ninja Release build and bundled game assets. Uses an isolated
profile and dummy SDL drivers. Logs and the test executable are retained in --output-dir.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=root / 'build')
parser.add_argument('--output-dir', type=Path)
parser.add_argument('--browser',action='store_true',help='Use the browser game as Newcomer; connect using browser.json, then create browser-observed after inspection.')
parser.add_argument('--mode', choices=['replace','share_ai','share_human','abort','spectate','reject_spectate'],default='replace')
args = parser.parse_args()
build = args.build_dir.resolve()
out = args.output_dir.resolve() if args.output_dir else Path(tempfile.mkdtemp(prefix='dunecity-late-join-probe-'))
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['python3', str(root / 'scripts/check-build-deps.py'), str(build)], check=True, cwd=root)
target = 'bin/dunecity.app/Contents/MacOS/dunecity'
lines = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True).splitlines()
main = (root / 'src/main.cpp').read_text()
needle = 'int menuResult = MainMenu().showMenu();'
if main.count(needle) != 1:
    raise RuntimeError('Main menu entry changed; update the test injection point.')
main = main.replace(needle, 'int menuResult = runLateJoinProbe();')
main = main.replace('if(shouldPlayIntro && (bFirstInit==true))', 'if(false && shouldPlayIntro && (bFirstInit==true))')
pos = main.index('int main(')
include = root / 'tests/network/late-join-probe.inc'
main = main[:pos] + '#include "' + str(include) + '"\n' + main[pos:]
source = out / 'late-join-probe-main.cpp'
source.write_text(main)
cc = shlex.split(next(line for line in lines if ' -c ' in line and '/src/main.cpp' in line))
# Test compilation must not overwrite Ninja's production dependency records.
for option in ('-include', '-MT', '-MF'):
    if option in cc:
        i = cc.index(option)
        del cc[i:i+2]
for option in ('-MD', '-MMD'):
    if option in cc:
        cc.remove(option)
obj = out / 'late-join-probe-main.o'
cc[cc.index('-o') + 1] = str(obj)
cc[cc.index('-c') + 1] = str(source)
map_path = root / 'data/maps/multiplayer/2P - 51x31 - 1v1 - Habbanya-Autumn.ini'
cc.append('-fno-access-control')
cc.append('-DPROBE_MAP_PATH="' + str(map_path) + '"')
app = out / 'late-join-probe.app/Contents'
(app / 'MacOS').mkdir(parents=True, exist_ok=True)
resources = app / 'Resources'
if not resources.exists():
    resources.symlink_to(build / 'bin/dunecity.app/Contents/Resources')
binary = app / 'MacOS/late-join-probe'
link = shlex.split(next(line for line in lines if ' -o ' + target + ' ' in line))
link = link[link.index('&&')+1:]
link = link[:link.index('&&')]
link[link.index('-o')+1] = str(binary)
link = [str(obj) if arg.endswith('/main.cpp.o') else arg for arg in link]
with (out / 'build.log').open('w') as log:
    subprocess.run(cc, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run(link, cwd=build, stdout=log, stderr=subprocess.STDOUT, check=True)

import sys, time
sys.path.insert(0,str(root/'tools/p2p-signaling/test'))
from test_signaling import ServiceFixture
service=ServiceFixture()
processes=[]; logs=[]
if args.browser:
    import http.server, threading, json
    class BrowserFiles(http.server.SimpleHTTPRequestHandler):
        def __init__(self,*a,**kw): super().__init__(*a,directory=str(build/'emscripten/bin'),**kw)
        def do_POST(self):
            # Match production's same-origin service routing; keep the browser CSP intact.
            import http.client
            conn=http.client.HTTPConnection('127.0.0.1',service.port,timeout=30)
            body=self.rfile.read(int(self.headers.get('Content-Length','0')))
            headers={k:v for k,v in self.headers.items() if k.lower() in ('content-type','origin','x-dune-session')}
            conn.request('POST',self.path,body=body,headers=headers)
            response=conn.getresponse(); data=response.read()
            self.send_response(response.status)
            for k,v in response.getheaders():
                if k.lower() not in ('connection','transfer-encoding','server','date'):
                    self.send_header(k,v)
            self.end_headers(); self.wfile.write(data); conn.close()
        def log_message(self,*a): pass
        def end_headers(self):
            self.send_header('Cross-Origin-Opener-Policy','same-origin')
            self.send_header('Cross-Origin-Embedder-Policy','require-corp')
            super().end_headers()
    web=http.server.ThreadingHTTPServer(('127.0.0.1',0),BrowserFiles)
    origin='http://127.0.0.1:'+str(web.server_port)
    service.write_config(allowed_origins=[origin])
    threading.Thread(target=web.serve_forever,daemon=True).start()
    (out/'browser.json').write_text(json.dumps({'url':origin+'/dunecity.html?relay='+origin+'&relaydev=1'}))

try:
    for role in (('Host','Partner') if args.browser else ('Host','Partner','Newcomer')):
        log=(out/(role+'.log')).open('w'); logs.append(log)
        env=dict(os.environ,DUNECITY_USERDIR=str(out/('profile-'+role)),SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',JOIN_ROLE=role,JOIN_MODE=args.mode,JOIN_OUT=str(out),JOIN_ENDPOINT='http://127.0.0.1:'+str(service.port))
        if args.browser: env['JOIN_BROWSER']='1'
        processes.append(subprocess.Popen([str(binary),'--window','--showlog'],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT))
    deadline=time.monotonic()+(620 if args.browser else 170)
    while time.monotonic()<deadline:
        compared=('Host','Partner') if args.mode=='abort' or args.browser else ('Host','Partner','Newcomer')
        if all((out/(role+'-digest')).exists() for role in compared) and (args.mode!='abort' or (out/'Newcomer-cancelled').exists()):
            values=[(out/(role+'-digest')).read_text() for role in compared]
            if len(set(values))!=1: raise RuntimeError('State mismatch: '+str(values))
            if args.browser and not (out/'browser-observed').exists(): time.sleep(.2); continue
            if args.mode in ('spectate','reject_spectate') and not args.browser:
                after=[out/(role+'-after-leave') for role in ('Host','Partner')]
                if not all(p.exists() for p in after): time.sleep(.2); continue
                if len({p.read_text() for p in after})!=1: raise RuntimeError('State diverged after spectator left')
            (out/'done').write_text('yes'); break
        if any(p.poll() is not None and not (i==2 and ((args.mode=='abort' and (out/'Newcomer-cancelled').exists()) or (args.mode in ('spectate','reject_spectate') and (out/'Newcomer-left').exists()))) for i,p in enumerate(processes)): raise RuntimeError('Probe process ended early; inspect logs in '+str(out))
        time.sleep(.2)
    else: raise RuntimeError('Join probe timed out')
    for p in processes:
        if p.wait(timeout=15)!=0: raise RuntimeError('Probe process failed')
    print(str(len(values))+' real peers continued with matching state ('+args.mode+'): '+values[0])
finally:
    for p in processes:
        if p.poll() is None: p.terminate()
    for p in processes:
        try: p.wait(timeout=5)
        except subprocess.TimeoutExpired: p.kill()
    for log in logs: log.close()
    service.stop()
    if args.browser: web.shutdown()
