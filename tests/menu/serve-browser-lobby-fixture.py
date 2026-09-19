#!/usr/bin/env python3
"""Serve unchanged browser assets with a local chat fixture; no public messages.

Open the printed URL in a fresh browser profile, set the name in Settings, then
Play Online. Requests are printed for verifying automatic entry and polling.
Room creation and message sending are deliberately unsupported in this fixture.
"""
import argparse
import json
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=Path(__file__).resolve().parents[2]/'build-714/emscripten/bin')
parser.add_argument('--port', type=int, default=18823)
args = parser.parse_args()

class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(args.build_dir.resolve()), **kw)

    def log_message(self, *a):
        pass

    def do_POST(self):
        fields = parse_qs(self.rfile.read(int(self.headers['Content-Length'])).decode())
        print(json.dumps({'path': self.path, 'name': fields.get('name'),
                          'runtime': fields.get('runtime'), 'allMods': fields.get('allMods'),
                          'presence': fields.get('presence')}), flush=True)
        body = 'status=ok\nprotocol=1\n'
        if self.path == '/v1/admission/list':
            body += 'next=0\n'
            if fields.get('allMods') == ['1']:
                for code, name, mod in [('H4PQ-7T2M-9XKB', 'Alice', 'vanilla'),
                                        ('J5QR-8V3N-2YKC', 'Bob', 'dunecity')]:
                    body += f'game={code}|1|4|custom|{name.encode().hex()}|abcdef|{mod.encode().hex()}\n'

        elif self.path == '/v1/lobby/enter':
            body += 'session=' + 'a'*64 + '\ncursor=0\n'
        elif self.path == '/v1/lobby/poll':
            body += 'cursor=1\ngap=0\nchat=1|' + 'Local fixture'.encode().hex()
            body += '|' + 'Browser chat connected automatically.'.encode().hex() + '\n'
            if fields.get('presence') == ['1']:
                body += 'online=2\nwaiting=416c696365\nwaiting=426f62\n'
        else:
            self.send_error(400, 'Unsupported fixture action')
            return
        encoded = body.encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

url = f'http://127.0.0.1:{args.port}'
print(f'{url}/dunecity.html?relay={url}&relaydev=1', flush=True)
ThreadingHTTPServer(('127.0.0.1', args.port), Handler).serve_forever()
