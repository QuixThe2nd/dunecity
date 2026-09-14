# DuneCity browser (Emscripten) build

The browser build script is `tools/web/build-emscripten.sh`. It builds the full
`dunecity` game target using the existing production shell and persistence code.
This directory also holds the WebRTC JavaScript bridge (`webrtc_glue.js`) used
by the browser multiplayer transport.

## Prerequisites

- git
- cmake 3.21+
- python3
- a C++ compiler for the host (used by emsdk)

## Reproducible build

From a clean checkout:

```bash
./tools/web/build-emscripten.sh
```

The script installs the Emscripten compiler version in
`tools/web/emsdk-version.txt` using the immutable installer revision in
`tools/web/emsdk-revision.txt`, into `.emsdk/` (override with `EMSDK_DIR`).
An existing SDK must have the expected origin, revision and clean tracked files.
Use a fresh SDK directory instead of replacing a different local installation.
The compiler version matches the production browser build (4.0.14).

`BUILD_DIR` overrides the output directory. Existing outputs are preserved for
incremental builds; the script never recursively deletes the supplied directory.
Source-root, home and source-ancestor destinations are refused before SDK setup.

### Output path

```
build/emscripten/bin/
  dunecity.html
  dunecity.js
  dunecity.wasm
  dunecity.data    # preloaded PAK/config/mods/sprites
  shell.js
  shell.css
```

### WebRTC glue

`platform/web/webrtc_glue.js` is linked into the Emscripten output via
`--js-library` in `src/CMakeLists.txt`. The C++ side (`WebRtcTransport.cpp`)
calls exported `webrtcHostRoom`, `webrtcJoinRoom`, `webrtcSendTo`, etc.; the
library block wires those to `createDuneCityWebRtc`.

Run the glue unit tests (Node, no browser):

```bash
cd platform/web && npm test
```

### p2pkit bundle

`webrtc_glue.js` consumes the hardened in-tree p2pkit copy
(`platform/web/p2pkit/src`, see `platform/web/p2pkit/UPSTREAM.md`) through the
committed IIFE bundle at `platform/web/dist/p2pkit.iife.js`, which exposes
`globalThis.P2PKIT_IIFE` (`RTCTransport`, `DEFAULT_ICE_SERVERS`, `Emitter`,
`randomId`). `tools/web/build-emscripten.sh` prepends the bundle to
`dunecity.js` so the runtime resolves it without a separate script tag, and no
npm/network access is needed at build time.

After changing `platform/web/p2pkit/src`, regenerate and re-commit the bundle:

```bash
npm ci --prefix tools/webrtc-signaling   # installs the esbuild devDependency
node tools/web/build-p2pkit-iife.mjs     # or: cd platform/web && npm run build:p2pkit-iife
```

The build script self-checks the export surface by loading the bundle in a
bare vm context; `platform/web/test/p2pkit-iife.test.cjs` enforces the same
contract in CI.

## Local smoke test

```bash
cd build/emscripten/bin
python3 -m http.server 8080
# open http://127.0.0.1:8080/dunecity.html
```

You still need original Dune 2 PAK files in `data/` at build time; they are
embedded into `dunecity.data` by `--preload-file`.

## CI

GitHub Actions job `build-emscripten` in `.github/workflows/build.yml` runs the
same `./tools/web/build-emscripten.sh` command.

The verifier checks artifact presence and obvious pthread dependencies; it is
not a security audit or a multiplayer test. Browser builds keep the existing
HTTP implementation, which already separates Emscripten from native libcurl.
The build foundation does not change gameplay routing: the WebRTC handshake
runs through the in-tree p2pkit copy while game packets stay on native binary
data channels.
