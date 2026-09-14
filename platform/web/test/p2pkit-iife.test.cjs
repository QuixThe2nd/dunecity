// Verifies the committed IIFE bundle at platform/web/dist/p2pkit.iife.js: it
// must load in a bare script context (no Node/Emscripten globals beyond the
// standard web platform ones) and expose exactly the surface webrtc_glue.js
// resolves as globalThis.P2PKIT_IIFE.

'use strict';

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { webcrypto } = require('node:crypto');

const BUNDLE = path.join(__dirname, '..', 'dist', 'p2pkit.iife.js');

// The trimmed in-tree p2pkit copy (see ../p2pkit/UPSTREAM.md) has no negotiate
// or sdp modules, so those upstream helpers are intentionally not bundled.
const EXPECTED_KEYS = ['DEFAULT_ICE_SERVERS', 'Emitter', 'RTCTransport', 'randomId'];

function loadBundleInFreshContext() {
  const source = fs.readFileSync(BUNDLE, 'utf8');
  const sandbox = {
    TextEncoder,
    TextDecoder,
    performance,
    queueMicrotask,
    setInterval,
    clearInterval,
    setTimeout,
    clearTimeout,
    crypto: webcrypto,
    Event,
    console,
  };
  vm.runInNewContext(source, sandbox, { filename: 'p2pkit.iife.js' });
  return sandbox.P2PKIT_IIFE;
}

test('p2pkit IIFE loads in a bare vm context and exposes exactly the glue surface', () => {
  const kit = loadBundleInFreshContext();
  assert.ok(kit, 'bundle must expose globalThis.P2PKIT_IIFE');
  assert.deepEqual(Object.keys(kit).sort(), [...EXPECTED_KEYS].sort());
  assert.equal(typeof kit.RTCTransport, 'function');
  assert.equal(typeof kit.Emitter, 'function');
  assert.equal(typeof kit.randomId, 'function');
});

test('p2pkit IIFE DEFAULT_ICE_SERVERS are STUN-only', () => {
  const kit = loadBundleInFreshContext();
  assert.ok(Array.isArray(kit.DEFAULT_ICE_SERVERS));
  assert.ok(kit.DEFAULT_ICE_SERVERS.length > 0);
  for (const server of kit.DEFAULT_ICE_SERVERS) {
    assert.equal(typeof server.urls, 'string');
    assert.match(server.urls, /^stun:/);
  }
});

test('p2pkit IIFE Emitter and randomId work inside the bundle context', () => {
  const kit = loadBundleInFreshContext();
  const emitter = new kit.Emitter();
  const seen = [];
  emitter.on('msg', (m) => seen.push(m));
  emitter.emit('msg', 'a');
  emitter.emit('msg', 'b');
  assert.deepEqual(seen, ['a', 'b']);
  assert.match(kit.randomId(8), /^[0-9a-f]{16}$/);
  assert.notEqual(kit.randomId(8), kit.randomId(8));
});
