#!/usr/bin/env node
/**
 * Bundles the pinned p2pkit GitHub dependency into a classic-script IIFE at
 * platform/web/dist/p2pkit.iife.js exposing globalThis.P2PKIT_IIFE.
 *
 * The dependency lives in platform/web/package.json as
 *   "p2pkit": "github:QuixThe2nd/p2pkit#<exact-commit>"
 * and is installed under platform/web/node_modules. This script runs
 * `npm install` there when the install is missing or does not match the pin,
 * then bundles the package's browser-safe "p2pkit/iife" module with esbuild —
 * the same export surface as upstream's own dist/p2pkit.iife.js.
 *
 * webrtc_glue.js resolves the global lazily at runtime, and
 * tools/web/build-emscripten.sh prepends the bundle to dunecity.js so the
 * browser runtime always sees it. The bundle is COMMITTED so the wasm build
 * never needs npm; regenerate it only when bumping the p2pkit pin:
 *
 *   (cd platform/web && npm run build:iife)
 *
 * Set P2PKIT_SKIP_INSTALL=1 to make a missing/stale install a hard error
 * instead of running npm install (for offline sandboxes).
 */
import { createRequire } from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { fileURLToPath } from 'node:url';
import { webcrypto } from 'node:crypto';
import { spawnSync } from 'node:child_process';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const WEB_DIR = path.join(ROOT, 'platform', 'web');
const OUT_FILE = path.join(WEB_DIR, 'dist', 'p2pkit.iife.js');
const PKG_DIR = path.join(WEB_DIR, 'node_modules', 'p2pkit');

// Mirrors src/iife.ts upstream and the EXPECTED_KEYS in
// platform/web/test/p2pkit-iife.test.cjs.
const EXPECTED_KEYS = [
  'DEFAULT_ICE_SERVERS',
  'DEFAULT_TRANSPORT_ORDER',
  'Emitter',
  'RTCDataChannelSendQueue',
  'RTCTransport',
  'capsFor',
  'chooseTransport',
  'extractIP',
  'isInitiator',
  'randomId',
];

const ENTRY = `export {\n${EXPECTED_KEYS.map((k) => `  ${k},`).join('\n')}\n} from 'p2pkit/iife'\n`;

function fail(message) {
  console.error(`ERROR: ${message}`);
  process.exit(1);
}

function pinnedSpec() {
  let pkg;
  try {
    pkg = JSON.parse(fs.readFileSync(path.join(WEB_DIR, 'package.json'), 'utf8'));
  } catch {
    fail(`cannot read ${path.join(WEB_DIR, 'package.json')}`);
  }
  const spec = pkg.dependencies && pkg.dependencies.p2pkit;
  if (!spec) {
    fail(
      'platform/web/package.json has no p2pkit dependency.\n' +
        '       Add "p2pkit": "github:QuixThe2nd/p2pkit#<full-commit-sha>" to its dependencies.',
    );
  }
  const match = /^github:QuixThe2nd\/p2pkit#([0-9a-f]{40})$/.exec(spec);
  if (!match) {
    fail(
      `p2pkit dependency "${spec}" is not pinned to an exact commit.\n` +
        '       Use the full 40-char sha: "github:QuixThe2nd/p2pkit#<full-commit-sha>" (bump = deliberate upgrade).',
    );
  }
  return { spec, sha: match[1] };
}

/** The commit actually present in node_modules/p2pkit, from npm's install ledger. */
function installedSha() {
  const lockPath = path.join(WEB_DIR, 'node_modules', '.package-lock.json');
  try {
    const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
    const resolved = lock.packages && lock.packages['node_modules/p2pkit'] && lock.packages['node_modules/p2pkit'].resolved;
    const match = resolved && /#([0-9a-f]{40})$/.exec(resolved);
    return match ? match[1] : null;
  } catch {
    return null;
  }
}

function installIsUsable(sha) {
  return (
    installedSha() === sha &&
    fs.existsSync(path.join(PKG_DIR, 'dist', 'iife.js')) &&
    fs.existsSync(path.join(PKG_DIR, 'package.json'))
  );
}

function npmInstall() {
  const npm = process.platform === 'win32' ? 'npm.cmd' : 'npm';
  console.log(`==> npm install in ${path.relative(ROOT, WEB_DIR)} (p2pkit pin changed or not installed yet)`);
  const result = spawnSync(npm, ['install', '--no-audit', '--no-fund'], {
    cwd: WEB_DIR,
    stdio: 'inherit',
  });
  if (result.status !== 0) {
    fail(`npm install in platform/web failed (exit ${result.status}). Fix the error above and re-run.`);
  }
}

const { spec, sha } = pinnedSpec();

if (!installIsUsable(sha)) {
  if (process.env.P2PKIT_SKIP_INSTALL) {
    fail(
      `platform/web/node_modules/p2pkit is missing or not at ${sha}, and P2PKIT_SKIP_INSTALL is set.\n` +
        '       Run: (cd platform/web && npm install)',
    );
  }
  npmInstall();
  if (!installIsUsable(sha)) {
    fail(
      `platform/web/node_modules/p2pkit is still not at the pinned commit ${sha} after npm install.\n` +
        '       Check the p2pkit dependency in platform/web/package.json and the lockfile, then re-run.',
    );
  }
}

// esbuild is a devDependency of platform/web (installed by the npm install above).
const requireFromWeb = createRequire(path.join(WEB_DIR, 'package.json'));
let esbuild;
try {
  esbuild = requireFromWeb('esbuild');
} catch {
  fail('esbuild is not installed in platform/web.\n       Run: (cd platform/web && npm install)');
}

fs.mkdirSync(path.dirname(OUT_FILE), { recursive: true });

try {
  await esbuild.build({
    stdin: {
      contents: ENTRY,
      resolveDir: WEB_DIR,
      loader: 'js',
    },
    bundle: true,
    format: 'iife',
    globalName: 'P2PKIT_IIFE',
    target: ['es2022'],
    outfile: OUT_FILE,
    charset: 'utf8',
    logLevel: 'silent',
    banner: {
      js: [
        '// Generated by tools/web/build-p2pkit-iife.mjs — do not edit.',
        `// Bundled from the npm package pinned in platform/web/package.json: ${spec}`,
        '// Exposes globalThis.P2PKIT_IIFE for webrtc_glue.js (Emscripten browser runtime).',
      ].join('\n'),
    },
    footer: { js: 'globalThis.P2PKIT_IIFE = P2PKIT_IIFE;' },
  });
} catch (error) {
  fail(`esbuild could not bundle ${spec}: ${error.message}`);
}

// Self-check: the bundle must load in a bare context and expose exactly the
// expected surface, so a bad regenerate cannot silently break the glue.
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
vm.runInNewContext(fs.readFileSync(OUT_FILE, 'utf8'), sandbox, { filename: 'p2pkit.iife.js' });
const kit = sandbox.P2PKIT_IIFE;
if (!kit) fail('bundle did not expose P2PKIT_IIFE');
const keys = Object.keys(kit).sort();
const expected = [...EXPECTED_KEYS].sort();
if (keys.join(',') !== expected.join(',')) {
  fail(`bundle exports [${keys}] but expected [${expected}].\n       The pinned p2pkit commit changed its iife surface; align EXPECTED_KEYS.`);
}
for (const name of EXPECTED_KEYS) {
  if (typeof kit[name] !== 'function' && !Array.isArray(kit[name])) {
    fail(`bundle export ${name} is not usable`);
  }
}

console.log(`OK: ${path.relative(ROOT, OUT_FILE)} (${spec}) exports ${keys.join(', ')}`);
