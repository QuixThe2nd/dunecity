#!/usr/bin/env node
/**
 * Post-build verification for dunecity.js WebRTC glue wiring.
 * Fails on DCE of createDuneCityWebRtc or literal $-prefixed helper calls at runtime.
 *
 * Usage:
 *   node tools/web/verify-dunecity-js.mjs path/to/dunecity.js
 *   node tools/web/verify-dunecity-js.mjs --source platform/web/webrtc_glue.js
 */
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const args = process.argv.slice(2);
if (args.length !== 2 || !['--built', '--source'].includes(args[0])) {
  console.error('usage: verify-dunecity-js.mjs --built|--source <path>');
  process.exit(2);
}

const mode = args[0];
const filePath = path.resolve(args[1]);
const text = fs.readFileSync(filePath, 'utf8');

function fail(msg) {
  console.error(`ERROR: ${msg}`);
  process.exit(1);
}

function hoistEmscriptenLibraryHelpers(lib, target) {
  for (const [key, value] of Object.entries(lib)) {
    if (key.startsWith('$')) {
      target[key.slice(1)] = value;
    }
  }
}

if (mode === '--source') {
  if (!text.includes('$createDuneCityWebRtc: createDuneCityWebRtc')) {
    fail('webrtc_glue.js must export $createDuneCityWebRtc to survive Emscripten DCE');
  }
  if (!text.includes('$webrtcInit__deps')) {
    fail('webrtc_glue.js must declare $webrtcInit__deps');
  }
  if (!text.includes('webrtcHostRoom__deps')) {
    fail('webrtc_glue.js must declare webrtcHostRoom__deps');
  }
  if (text.includes('$createDuneCityWebRtc__postset')) {
    fail('webrtc_glue.js must not use $createDuneCityWebRtc__postset; $ keys emit unprefixed runtime ids');
  }
  if (/\$createDuneCityWebRtc\s*\(/.test(text)) {
    fail('webrtc_glue.js must call createDuneCityWebRtc(...), not literal $createDuneCityWebRtc(...) at runtime');
  }
  if (/\$webrtcInit\s*\(/.test(text)) {
    fail('webrtc_glue.js must call webrtcInit(), not literal $webrtcInit() at runtime');
  }
  if (!/\bcreateDuneCityWebRtc\s*\(/.test(text)) {
    fail('webrtc_glue.js must call createDuneCityWebRtc(...) inside $webrtcInit');
  }
  if (!/\bwebrtcInit\s*\(/.test(text)) {
    fail('webrtc_glue.js must call webrtcInit() from exported wrappers');
  }
  console.log(`OK: source WebRTC library wiring in ${filePath}`);
  process.exit(0);
}

// --built: validate emitted dunecity.js
if (
  !/function\s+createDuneCityWebRtc\s*\(/.test(text) &&
  !/var\s+createDuneCityWebRtc\s*=/.test(text) &&
  !/createDuneCityWebRtc\s*=\s*function/.test(text)
) {
  fail('dunecity.js missing createDuneCityWebRtc factory (DCE or glue not linked)');
}

if (!/function _webrtcHostRoom\(\)\{webrtcInit\(\)/.test(text)) {
  fail('dunecity.js _webrtcHostRoom must call webrtcInit() (Emscripten $ key emits unprefixed id)');
}
if (/function _webrtcHostRoom\(\)\{\$webrtcInit\(\)/.test(text)) {
  fail('dunecity.js _webrtcHostRoom calls literal $webrtcInit() (ReferenceError in browser)');
}
if (/\$createDuneCityWebRtc\s*\(/.test(text)) {
  fail('dunecity.js must not reference literal $createDuneCityWebRtc(...) at runtime');
}
if (/\$webrtcInit\s*\(/.test(text)) {
  fail('dunecity.js must not reference literal $webrtcInit() at runtime');
}

const exportNames = [
  '_webrtcHostRoom',
  '_webrtcJoinRoom',
  '_webrtcSendTo',
  '_webrtcGetRoomCode',
  '_webrtcGetState',
  '_webrtcGetRttMs',
  '_webrtcDisconnect',
  '_webrtcOnEvent',
];
for (const name of exportNames) {
  if (!text.includes(name)) {
    fail(`dunecity.js missing exported symbol ${name}`);
  }
}

if (/dunecity\.worker\.js|ENVIRONMENT_IS_PTHREAD=true|USE_PTHREADS/.test(text)) {
  fail('dunecity.js appears to require pthread worker (expected single-threaded build)');
}

// Runtime smoke: re-load glue with mocked Emscripten runtime (validates source wiring).
globalThis.mergeInto = (target, lib) => Object.assign(target, lib);
globalThis.LibraryManager = { library: {} };
globalThis.Module = { print: () => {} };
globalThis.HEAPU8 = { set() {}, slice(_s, _e) { return new Uint8Array(0); } };
globalThis._malloc = () => 0;
globalThis._free = () => {};
globalThis._webrtcOnEvent = () => {};
globalThis.UTF8ToString = () => '';
globalThis.stringToUTF8 = () => {};
globalThis.RTCPeerConnection = class {};
globalThis.WebSocket = class { static OPEN = 1; };

const repoGlue = path.resolve(process.cwd(), 'platform/web/webrtc_glue.js');
if (!fs.existsSync(repoGlue)) {
  fail('cannot locate platform/web/webrtc_glue.js for runtime wrapper smoke test');
}
await import(pathToFileURL(repoGlue).href);

const lib = globalThis.LibraryManager.library;
if (typeof lib.$createDuneCityWebRtc !== 'function') {
  fail('LibraryManager.library missing $createDuneCityWebRtc after loading glue');
}
if (typeof lib.webrtcHostRoom !== 'function') {
  fail('LibraryManager.library missing webrtcHostRoom wrapper');
}

hoistEmscriptenLibraryHelpers(lib, globalThis);

try {
  lib.webrtcHostRoom();
} catch (err) {
  if (err instanceof ReferenceError) {
    fail(`webrtcHostRoom init path threw ReferenceError: ${err.message}`);
  }
  throw err;
}

console.log(`OK: built dunecity.js WebRTC glue checks passed for ${filePath}`);
