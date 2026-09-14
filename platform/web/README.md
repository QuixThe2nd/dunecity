# DuneCity browser (Emscripten) build

This directory holds the WebRTC JavaScript bridge and the reproducible browser
build script for the full `dunecity` game target (not a toy demo).

## Prerequisites

- git
- cmake 3.15+
- python3
- a C++ compiler for the host (used by emsdk)

## Reproducible build

From a clean checkout:

```bash
./tools/web/build-emscripten.sh
```

The script installs a **pinned** Emscripten SDK version from
`tools/web/emsdk-version.txt` into `.emsdk/` (override with `EMSDK_DIR`), then
configures and builds with `emcmake`.

### Output path

```
build/emscripten/bin/
  dunecity.html
  dunecity.js
  dunecity.wasm
  dunecity.data    # preloaded PAK/config/mods/sprites
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
