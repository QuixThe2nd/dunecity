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
This build foundation does not add P2PKit or change gameplay routing.
