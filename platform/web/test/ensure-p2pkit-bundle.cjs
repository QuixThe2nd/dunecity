'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

const ROOT = path.resolve(__dirname, '../../..');
const BUNDLE = path.join(ROOT, 'platform/web/p2pkit/dist/p2pkit.iife.js');
const FETCH_SCRIPT = path.join(ROOT, 'tools/web/fetch-p2pkit-bundle.sh');

let fetched = false;

function ensureP2pkitBundle() {
  if (fs.existsSync(BUNDLE) && fs.statSync(BUNDLE).size > 0) {
    return;
  }
  if (fetched) {
    throw new Error(`p2pkit bundle still missing after fetch: ${BUNDLE}`);
  }
  fetched = true;

  const result = spawnSync('bash', [FETCH_SCRIPT], {
    cwd: ROOT,
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  if (result.status !== 0) {
    const detail = (result.stderr || result.stdout || '').trim();
    throw new Error(
      `failed to fetch p2pkit bundle via ${FETCH_SCRIPT}` +
        (detail ? `: ${detail}` : ` (exit ${result.status})`),
    );
  }

  if (!fs.existsSync(BUNDLE) || fs.statSync(BUNDLE).size === 0) {
    throw new Error(`p2pkit bundle missing or empty after fetch: ${BUNDLE}`);
  }
}

ensureP2pkitBundle();

module.exports = { ensureP2pkitBundle, BUNDLE };
