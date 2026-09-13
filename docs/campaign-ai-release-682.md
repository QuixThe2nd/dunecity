# Dune City 1.0.682 release verification

The campaign candidate was originally numbered 1.0.680. Main's independent
1.0.681 map-browser fix was integrated as 1.0.682 in f117665. No AI or unit
source changed during this integration.

Native: both dependency audits, app signature and all seven CTest groups pass.
The new worker fixture passes. Atreides level 9 Hard/Hard seed 42 resolves at
cycle 67614, exactly matching the 680 candidate. The 71-scenario assessment
in campaign-ai-validation-680.md remains the full gameplay evidence.

Browser: final build dependency and bundled-mod audits pass. The real Custom
Game map list opens before Players and Back/Escape returns through that list.
Campaign starts correctly. An actual Atreides level 1 campaign with Easy helper
and Easy enemies completed naturally at maximum speed: score 94, 10 game
minutes, 6206 spice and nine units destroyed credited to the player. The
browser telemetry identifies SCENA001.INI, seed 194126089 and version 1.0.682.
No mission skip or injected victory was used. The prior 680 Atreides9 browser
session reached 15 workers and attacked but its completion was not observed.

PR: https://github.com/ggtothemax/dunecity/pull/33
Final candidate CI: https://github.com/ggtothemax/dunecity/actions/runs/34763416797
Stable artifact browser: the actual DuneCity-Emscripten artifact from tagged
build 34764559810 also completed Atreides level 4 Easy/Easy naturally at fastest
speed (4 ms). SCENA008.INI, seed 780537672; score 446, 15 game minutes.
Player/enemy spice: 36,799 / 5,186; units destroyed: 54 / 18;
buildings destroyed: 25 / 0. Telemetry recorded helper sorties at cycles
43,149, 49,349 and 54,599 and an enemy four-unit sortie at 48,750.
Only setup, speed, briefing and results UI were operated; no combat orders,
skipping or injected victory. Player-house statistics were correctly credited.

## Published release

- PR #33 merged as `d2dc426823c51a56c0966732c1dbec3f6cc38cde`, tagged `v1.0.682`.
- [Stable CI 34764559810](https://github.com/ggtothemax/dunecity/actions/runs/34764559810)
  passed all tests and Windows, Linux, macOS and Emscripten builds.
- All six [GitHub release assets](https://github.com/ggtothemax/dunecity/releases/tag/v1.0.682)
  were downloaded and their SHA-256 digests checked against GitHub metadata.
  The downloaded Mac DMG matched the inspected CI DMG: image verification,
  version 1.0.682, strict application signature and both bundled mods passed.
- [SourceForge 34765728864](https://github.com/ggtothemax/dunecity/actions/runs/34765728864)
  read back and verified uploaded checksums, published the dedicated source tag
  and branch at d2dc426, and confirmed Windows/macOS/Linux defaults at 1.0.682.
  No source archive was added to Files.
- The exact stable Emscripten artifact was packaged from the tagged source
  with production signaling. Redundant automatic browser rebuild 34765728821
  was cancelled, preserving stable release and SourceForge jobs.
- Website `9c82c5ff0b049fbef97a4206d0779dc465ce7ff9` publishes the browser package
  and combined campaign/map-browser release copy. Deploy 34765821210,
  Web security 34765821206 and CodeQL 34765821213 passed.
- Live `https://dunelegacy.com/play/build.json` reports 1.0.682/source d2dc426;
  all seven public artifact files were downloaded and matched its SHA-256 hashes.
  Both live download pages show 1.0.682 and combined release copy; the co-op
  map-browser guide is retained. Signaling health returns status=ok/protocol=1.
  The public browser starts at v1.0.682 and opens the campaign setup correctly.
- The separate Discord signaling fix in PR #37 is server-only and newer than
  the release tag; future service publication must retain it rather than
  restoring the tagged service over it.

Local evidence: `/tmp/dunecity-682-public-verification.json`,
`/tmp/dunecity-682-release-assets.json`, `/tmp/dunecity-682-sourceforge.log`,
`/tmp/dunecity-682-published/` and `/tmp/dunecity-682-ci-macos/`.

This is simulation and release verification, not a measured human win rate.
No installed application was replaced and no new two-device WAN multiplayer
claim is made.
