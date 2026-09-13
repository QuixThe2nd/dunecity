#!/usr/bin/env node
/*
 * DEBT: combined static+signaling server for local two-browser testing only.
 * Serves the Emscripten build directory on HTTP and runs the DuneCity
 * signaling WebSocket server on the same port (path /).
 * remove when: the signaling server itself serves the game's static files.
 */
import { createSignalingServer } from './server.js';

const PORT = Number(process.env.PORT || 8788);
const BIN = process.env.BIN_DIR || '/root/workspaces/dc-fix-emscript/build/emscripten/bin';

const MIME = {
  '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm',
  '.data': 'application/octet-stream', '.png': 'image/png', '.css': 'text/css', '.map': 'application/json',
};

const signaling = createSignalingServer({ host: '0.0.0.0', port: PORT });
const httpServer = signaling.httpServer;

httpServer.removeAllListeners('request');
httpServer.on('request', (req, res) => {
  const u = new URL(req.url, 'http://placeholder');
  const hostHeader = (req.headers.host || `127.0.0.1:${PORT}`).split(':')[0];
  if (u.pathname === '/health') {
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify(signaling.stats()));
    return;
  }
  if (u.pathname === '/cfg.js') {
    // Tell the in-game glue where the signaling server lives (same origin).
    res.writeHead(200, { 'content-type': 'text/javascript' });
    res.end(`window.DUNECITY_WEBRTC_CONFIG = { signaling: 'ws://${hostHeader}:${PORT}/' };`);
    return;
  }
  let p = u.pathname === '/' ? '/dunecity.html' : u.pathname;
  p = p.split('?')[0];
  if (p.includes('..')) { res.writeHead(403); res.end(); return; }
  const file = BIN + p;
  import('node:fs').then(fs => {
    fs.stat(file, (err, st) => {
      if (err || !st.isFile()) { res.writeHead(404); res.end('not found'); return; }
      res.writeHead(200, {
        'content-type': MIME[p.slice(p.lastIndexOf('.'))] || 'application/octet-stream',
        'content-length': st.size,
        'cross-origin-resource-policy': 'cross-origin',
      });
      fs.createReadStream(file).pipe(res);
    });
  });
});

httpServer.listen(PORT, '0.0.0.0', () => {
  console.log(`dunecity test server: http://0.0.0.0:${PORT}/  (signaling ws on same port)`);
});
