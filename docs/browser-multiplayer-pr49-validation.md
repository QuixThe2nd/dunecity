# Browser multiplayer review — PR 49

Reviewed on 20 September 2026 and refreshed against `main` c505255 after PR 61
merged during validation. Combined version: 1.0.735.

## Corrections

- Browser client `connectPeer` aliases an entry in the admission or established
  peer list. Original cleanup freed it twice. All aliases are now removed before
  freeing either allocation; repeated teardown is safe.
- Local rejection used to depend on an SDK Disconnect event that `disconnect()`
  itself discards. Deferred local cleanup now retains packet-adapter borrows until
  handling returns, then frees the peer and reports the cause exactly once.
- Browser messages now pass the existing incoming-byte budget before parsing.
- Corrected the rejected-packet log's missing vararg and the room-session null
  transport query. Duplicate Connect events cannot overwrite owned peer data.
- Cancel handles both queued and already paired clients. Leaving map selection
  releases the pair; a new match resets role, phase, seed and mod-transfer state.
  Restored the missing Back button in the browser matchmaking layout.
- Capped menu frame pacing at 50 ms. A callback can contain a whole game; the
  original code scheduled a measured 484,078 ms browser sleep after returning
  from a match, leaving a blank screen. The real-browser smoke test records
  Emscripten sleep timers and exercises the actual quit path.
- Resolved merge conflicts while retaining current updater, city metadata,
  observer protections, hot-join checkpoint recovery and named join UI. New room packets use the shared
  packet stream API so the merged browser build compiles.
- Preserved bounded ICE failure diagnostics in the direct-play adapter when
  replacing the patched vendored SDK with the pinned package.
- Fixed incremental SDK prepending, added link dependencies, HTTPS dependency
  resolutions, build/service documentation and a pinned upstream fixture check.
  CI runs browser glue tests and the wasm32 AddressSanitizer lifecycle harness.

## Local verification

- Full Apple silicon native build, including native direct WebRTC; Ninja
  dependency audit before and after the build.
- Eight CTest groups pass, including the real SDL menu-navigation probe. The main
  suite reports 770 passed and three skipped cases; no failed cases.
- Full browser build using the repository's pinned Emscripten 4.0.14. Source and
  generated-JS verifiers pass; bundled mod audit verifies 769 Tornie and six
  Dune2R files. A second incremental build leaves `dunecity.js` byte-identical.
- Browser JS suites: 29 passed. Direct SDK/bridge suites: 18 passed. Nine framing
  cross-checks and four web-build safety checks pass.
- The standalone wasm32 ASan harness links the production peer-lifetime methods;
  only game construction and the JS socket ABI are fixtures. It covers host and
  client cleanup before/after admission, orphan host aliases, repeated cleanup,
  reconnection, deferred rejection, remote/local event races, and callbacks that
  reenter cleanup. Fixed code passes with no sanitizer report. Substituting the
  original PR's `clearAllPeers` produces `AddressSanitizer: heap-use-after-free`.
- Two isolated real Chromium contexts load the built game with loopback signaling
  and host-only ICE. Cancel/retry succeeds; both clients pair and enter the
  Habbanya-Penny 128x128 map. The host reached 3,992 sent / 3,993 received command
  packets with zero reported drops while both games remained connected.
- The 1.0.734 build passes the automated real-browser acceptance test in
  `tests/web/matchmaking-smoke.mjs`, including the guest quitting back to Play
  Online promptly, with no browser errors or Emscripten sleeps over 50 ms.
  Screenshots and counters are written under `build/matchmaking-smoke/`.
- The matchmaking server fixture matches the immutable upstream source byte for
  byte after its provenance header; its pin and SHA-256 are checked offline too.

## Refresh after main advanced

The combined 1.0.735 native and browser builds pass. All eight native CTest
groups and all 193 signaling tests pass again. The real three-peer hot-join
replacement probe resumes with matching state at cycle 150. The wasm32 ASan
lifecycle harness and automated two-browser pairing/play/quit test pass again.
The PR description records CI status for the pushed revision.

## Deployment and limits

This does not deploy a server or publish game assets. **Find Match requires the
separate p2pkit WebSocket service**, including TLS/proxy routing on the production
origin; see `platform/web/README.md`. The existing PHP room directory is a
different service. No public signaling service or cross-network NAT traversal was
validated. There is no default TURN relay. Matchmaking trusts its signaling
server; it does not use the direct-room fingerprint admission protocol.

The live smoke test verifies pairing, lobby/start handoff and sustained packet
exchange on one machine, not Internet reachability or every gameplay action.
CI platform checks must be evaluated on the pushed revision before merging.
