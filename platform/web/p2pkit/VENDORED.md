# p2pkit browser bundle (fetched from upstream CI)

The browser WebRTC glue (`platform/web/webrtc_glue.js`) consumes a prebuilt
IIFE bundle of [p2pkit](https://github.com/QuixThe2nd/p2pkit) that exposes
`globalThis.P2PKIT_IIFE`. The bundle is built by the upstream repo's
`Build IIFE bundle` workflow on every push to `master` and committed back to
`dist/p2pkit.iife.js` there.

This directory intentionally does NOT vendor p2pkit sources. The bundle is
fetched at build time by `tools/web/fetch-p2pkit-bundle.sh`:

```sh
bash tools/web/fetch-p2pkit-bundle.sh            # use the pinned SHA below
bash tools/web/fetch-p2pkit-bundle.sh <commit>   # fetch a specific commit
P2PKIT_SKIP_FETCH=1 bash tools/web/build-emscripten.sh   # offline: reuse an already-fetched bundle
```

The wasm build remains hermetic: the fetched bundle is committed locally
before linking, and the build script never touches npm or the network beyond
this one raw.githubusercontent.com download.

Pin: to move to the latest upstream build, replace the SHA on the line below
with the current master SHA of QuixThe2nd/p2pkit.

upstream: 10d70550ceec8e94b19636cf820ae4bbf6c450af
upstream repo: https://github.com/QuixThe2nd/p2pkit
fetched file: dist/p2pkit.iife.js (gitignored, fetched by the script)
