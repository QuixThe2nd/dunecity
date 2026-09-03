'use strict';

const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const ROOT = path.resolve(__dirname, '../../..');
const GLUE = path.join(ROOT, 'platform/web/webrtc_glue.js');
const VERIFY = path.join(ROOT, 'tools/web/verify-dunecity-js.mjs');

test('webrtc_glue.js uses Emscripten $ deps and retains createDuneCityWebRtc', () => {
  const text = fs.readFileSync(GLUE, 'utf8');
  assert.match(text, /\$createDuneCityWebRtc:\s*createDuneCityWebRtc/);
  assert.match(text, /\$webrtcInit__deps:/);
  assert.match(text, /webrtcHostRoom__deps:\s*\[\s*'\$webrtcInit'\s*\]/);
  assert.match(text, /webrtcJoinRoom__deps:\s*\[\s*'\$webrtcInit'/);
  assert.doesNotMatch(text.replace(/\$webrtcInit/g, ''), /\bwebrtcInit\s*\(/);
});

test('verify-dunecity-js.mjs accepts current webrtc_glue.js source', () => {
  const result = spawnSync(process.execPath, [VERIFY, '--source', GLUE], {
    cwd: ROOT,
    encoding: 'utf8',
  });
  assert.equal(result.status, 0, result.stderr || result.stdout);
});

test('Emscripten library wrappers init without ReferenceError', () => {
  global.mergeInto = (target, lib) => Object.assign(target, lib);
  global.LibraryManager = { library: {} };
  global.Module = { print: () => {} };
  global.HEAPU8 = {
    set() {},
    slice(start, end) {
      return new Uint8Array(end - start);
    },
  };
  global._malloc = (n) => 1024;
  global._free = () => {};
  global._webrtcOnEvent = () => {};
  global.UTF8ToString = () => 'ABCD';
  global.stringToUTF8 = () => {};
  global.RTCPeerConnection = class MockPC {};
  global.WebSocket = class MockWS {
    static OPEN = 1;
    static CONNECTING = 0;
    static CLOSED = 3;
    constructor() {
      this.readyState = MockWS.OPEN;
      this.onopen = null;
      queueMicrotask(() => this.onopen?.({}));
    }
    send() {}
    close() {}
  };

  delete require.cache[require.resolve('../webrtc_glue.js')];
  require('../webrtc_glue.js');

  const lib = global.LibraryManager.library;
  assert.equal(typeof lib.$createDuneCityWebRtc, 'function');
  assert.equal(typeof lib.webrtcHostRoom, 'function');
  assert.equal(typeof lib.webrtcJoinRoom, 'function');
  assert.equal(typeof lib.webrtcGetState, 'function');

  // Emscripten hoists $-prefixed library helpers into generated JS scope.
  for (const [key, value] of Object.entries(lib)) {
    if (key.startsWith('$')) {
      global[key] = value;
    }
  }

  assert.doesNotThrow(() => lib.webrtcHostRoom());
  assert.equal(lib.webrtcGetState(), 1, 'hostRoom leaves transport connecting');
  assert.doesNotThrow(() => lib.webrtcGetRoomCode(0, 0));
  assert.doesNotThrow(() => lib.webrtcDisconnect());
});
