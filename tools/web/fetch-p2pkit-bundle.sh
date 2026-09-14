#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VENDORED_MD="${ROOT}/platform/web/p2pkit/VENDORED.md"
DIST_DIR="${ROOT}/platform/web/p2pkit/dist"
BUNDLE="${DIST_DIR}/p2pkit.iife.js"
CHECKSUM="${DIST_DIR}/p2pkit.iife.js.sha256"

if [[ $# -ge 1 ]]; then
    SHA="$1"
else
    SHA="$(sed -n 's/^upstream:[[:space:]]*//p' "${VENDORED_MD}" | head -n1 | tr -d '[:space:]')"
fi

if [[ ! "${SHA}" =~ ^[0-9a-f]{7,40}$ ]]; then
    echo "ERROR: invalid p2pkit commit SHA: '${SHA}' (expected 7-40 lowercase hex digits)" >&2
    exit 1
fi

mkdir -p "${DIST_DIR}"

URL="https://raw.githubusercontent.com/QuixThe2nd/p2pkit/${SHA}/dist/p2pkit.iife.js"
curl -fsSL "${URL}" -o "${BUNDLE}"

if [[ ! -s "${BUNDLE}" ]]; then
    rm -f "${BUNDLE}"
    echo "ERROR: downloaded p2pkit bundle is empty (${URL})" >&2
    exit 1
fi

if ! grep -q 'P2PKIT_IIFE' "${BUNDLE}"; then
    rm -f "${BUNDLE}"
    echo "ERROR: downloaded p2pkit bundle does not contain P2PKIT_IIFE (${URL})" >&2
    exit 1
fi

(
    cd "${DIST_DIR}"
    sha256sum p2pkit.iife.js > p2pkit.iife.js.sha256
)

HASH="$(awk '{print $1}' "${CHECKSUM}")"
BYTES="$(wc -c < "${BUNDLE}" | tr -d '[:space:]')"
echo "${HASH}  ${BYTES} bytes"
