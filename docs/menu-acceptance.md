# Classic player setup and human co-op check — 1.0.674, 13 September 2026

## Player setup restoration

The original centered roster/map composition is restored for custom setup and
network lobbies. Full Player 1 / Player 2 labels and 180-pixel player lists are
used from 800 pixels wide; compact rows remain below that. Wide windows regain
inset Back/Start buttons. Mods with bonus colors regain the original palette
checkbox. Offline setup hides private/public visibility. Map, mod, rules,
connection switching and explicit Start Game / Create Lobby remain together.
The prior entered-name and open-seat hosting validation stays in place.

Native Release, dependency/version/signature checks and all seven CTest groups
pass. The final narrow Bonus-label adjustment was rebuilt and the menu probe
rerun successfully. Actual rendering at 640×480, 854×480 and 1280×720 is asserted;
the diagnostic main alone bypasses the dummy desktop's 1024-pixel clamp. Visual
checks include six-house Offline/Online, shared houses and Tornie bonus colors.
The final web Release build and dependency audit pass as well. These 674 checks
exercise real production menu objects, not two-peer transport.

## Human-hosted campaign — matching 673 clients

Stefan created a private Ordos campaign in the browser and clicked Start Campaign
after Codex joined as MenuHost natively. Both screens displayed the shared roster
before Start. Both entered the mission. Stefan's unit movements and newly explored
terrain appeared on the native peer. Inspected peer reports exceeded cycle 27,749;
no desync, digest-mismatch, connection-lost or peer-disconnected diagnostic was
found in that log snapshot. Both clients remained running afterward.

This confirms human hosting, admission, starting and host-command delivery in
this local cross-platform session. Same-Mac loopback signaling and direct peers
do not establish WAN/NAT behavior, saved-session resumption or full campaign
completion. The 673 gameplay observation is not a two-peer test of 674.

Evidence: `../outputs/dunecity-menu-acceptance/human-campaign-673.log`,
`build-native-674.log`, `build-native-674-final.log`, `build-web-674-final.log`,
production-menu captures
under `build/menu-probe`, and exported `674-*.png` images in the artifact folder.
The running browser at localhost:8769 still serves 673. Keep that session and
its matching native copy alive; 674 must be packaged separately. Nothing is
installed, pushed or publicly deployed.

---

# Hosting fixes and acceptance — 1.0.673, 13 September 2026

Local hosting acceptance now passes. The candidate is built in
`build/bin/dunecity.app`; `/Applications/dunecity.app` was still 1.0.653 when
checked and has not been replaced. Nothing has been pushed or deployed.

## Changes

- Create Campaign / Create Custom Game now validate and save the name entered
  on Join Online before constructing the setup and host roster. Previously
  those actions silently used the old saved name.
- Creating a custom online lobby requires an explicit Open player slot. An
  offline setup filled with AI retains its choices when switched online, but
  now explains that a place must be opened for a guest. The old lobby's fallback
  could replace an AI; this is a setup-clarity fix, not a transport fix.
- Admission now says it is connecting to the host until the host's setup is
  received. Diagnostics record receipt and opening of the roster without
  exposing invitation codes or admission credentials.
- Emscripten Release now uses the source-controlled `-O2` final optimizer
  pipeline, retaining `-O3` C++ compilation and Asyncify. The temporary
  `CMAKE_EXE_LINKER_FLAGS_RELEASE` override was cleared. The normal Release
  build completes with the checked-in settings, resolving the prior build gate.

## Live verification

All clients in these 673 checks ran the same candidate and used an isolated
local PHP signaling service. No public room or public chat message was created.

| Check | Observed result |
| --- | --- |
| Native host, public Campaign, browser guest | The room appeared in the directory; the guest roster displayed before Start on two joins. Setup-to-roster diagnostics measured 25 ms and 20 ms. Both clients entered the Atreides mission and ran beyond cycle 12,374 with no reported desync. A browser-issued infantry movement appeared on the native host. |
| Browser host, private Custom, native guest | BrowserHost673, edited on Join Online, appeared in the setup and both rosters. The guest joined by code before Start, the content/start barrier completed, and both entered the 128×128 Habbanya-Penny match. Browser host received native peer reports through at least cycle 2,249. Both returned to menus after the host left. |
| Browser host, browser guest Campaign comparison | The earlier 672 comparison also displayed the guest roster before Start and entered gameplay. The original persistent waiting-display observation was not reproduced; it is not attributed to a proven network or renderer defect. |
| Campaign keyboard Start | Actual native keyboard navigation reached Start and launched the match. A production-menu test also reaches Start after a simulated partner takes the open seat. Earlier difficulty operating it was not established as a code defect. |
| Automated checks | All seven CTest groups pass, including host-name validation and no-open-seat regressions. Native/web dependency audits, version consistency and strict native app signature verification pass. |

The 672 waiting-screen finding is superseded by these successful pre-start
roster checks. Receipt/opening diagnostics remain to investigate any recurrence.
Do not claim a transport repair where none was established.

## Crash alerts during testing

macOS report `dunecity-2026-09-13-214953.ips` identifies PID 30706 in the temporary
Dune Menu Host copy: `SIGKILL (Code Signature Invalid)` / `Taskgated Invalid
Signature`. Its executable had been replaced without re-signing that copied
bundle. It was re-signed and relaunched successfully; the built delivery app
passes strict verification. Stop copied clients before replacing their binaries,
then re-sign the copy before relaunching.

A separate `menu-probe-2026-09-13-215900.ips` was a test-fixture error: the probe
called `getChangeEventListForNewPlayer` without a NetworkManager. The corrected
fixture uses the seat-assignment operation directly; all tests pass. Neither
report establishes a gameplay crash in the candidate.

## Evidence and remaining release check

Evidence remains under `../outputs/dunecity-menu-acceptance`, including
`ctest-673-complete.log`, `menu-probe-673-final.log`, `web-673-build.log`,
`web-673-final-build.log`, `native-673-final-build.log` and
`native-673-session.log`. Browser virtual-filesystem diagnostics were inspected
through CDP and screens through computer use. The production health endpoint
answered `status=ok`, `protocol=1`; that alone is not a hosting test.

Before general release, have Stefan host using the 673 build and a second
matching client, then confirm the lobby, start and commands from another device
or network. The installed 653 app and an older public browser build are not
matching test clients. Saved-session resumption, WAN/NAT, mobile and campaign
completion are not newly signed off by this pass. Earlier offline/settings/
Extras acceptance evidence remains below.

---

# Historical 672 menu acceptance — 13 September 2026

Local candidate: 1.0.672 on `feat/menu-navigation`, following 3989cd2 (1.0.670).
The verification uses the built production app, isolated host/guest profiles,
and a private PHP signaling fixture on loopback. No public service was changed.

## Observed results

| Journey | Result | Evidence / boundary |
| --- | --- | --- |
| Home keyboard navigation | Passed after repair | Hidden legacy buttons previously trapped Tab. Only visible destinations are now registered, in visual order. Automated full cycle without Continue; live full cycle with Continue. |
| Offline Campaign | Passed | Home → Campaign → Start → briefing → actual Atreides mission, version 672. |
| Save and Continue | Passed | Saved `menu-offline-acceptance`, quit to Home, then Continue resumed the same mission. Online-only saves did not expose Continue on the host. |
| Offline Custom Game | Passed | Home → Custom Game → Start entered a 128×128 match with the default QuantBot Easy opponent, version 672. |
| Private online custom | Passed | Two native clients joined by code, acknowledged content/roster, counted down and entered gameplay. Peer traffic reached cycle 11,999 without a reported desync, version 670. |
| Public campaign directory | Passed | A real room appeared in All games and Campaign co-op; Custom games excluded it. Guest joined from the list, version 672. |
| Campaign setup preservation | Passed | Ordos, full campaign, level 4, Vanilla, Easy enemies reached the lobby. Cancelling and reopening hosting retained the choices. Runtime loaded `SCENO008.INI`. |
| Two-player campaign | Passed | Both version-672 native clients entered the same Ordos mission, with peer updates beyond cycle 2,600 and no reported desync. Saved `menu-coop-acceptance`. |
| Online save routing | Passed | Home → Load Game → Online saves reopened the version-670 custom save as a hosting lobby in both 670 and 672, preserving map and saved houses. The version-672 campaign save opened a co-op lobby with Ordos and a partner slot. A joined/resumed save session is not yet claimed. |
| Unified Settings | Passed for inspected scope | Live native/browser Graphics includes video and interface/menu controls; browser Audio switches within the same screen. Production-menu probe checks tab visibility and unrelated settings validation. |
| Mods | Passed for entry route | Home → Extras → Mods opened the installed-mod list/details; Back returned to Extras. Mod mutations were not part of this check. |
| Map Editor | Passed for entry route | Home → Extras → Map Editor opened the editor and New Map dialog. Map authoring/export was not exercised. |
| Compact layouts | Passed in menu probe | Production widgets rendered at 640×480 and 854×480; setup, house/player rows, directory, Extras and Settings inspected in the menu review. |
| Automated checks | Passed | All seven CTest groups, native Ninja dependency audit, version consistency and strict app signature verification. |
| Browser | Gameplay passed; lobby finding open | Version 672 joined a fresh public Ordos campaign hosted natively, passed the start barrier, and ran through cycle 14,624 without a reported desync. A browser unit movement appeared on the host. Home/Join/Graphics/Audio mouse navigation passed. This uses Release objects with `-O2` linking; default `-O3` linking exceeded 20 minutes in the final Stack IR optimizer. |

## Open browser lobby finding

In this browser test build, joining either the saved private co-op room or the
fresh public campaign could leave the displayed screen on Join Online with
"Waiting for the host's game setup", while the host already showed MenuBrowser
as ready. The fresh campaign nevertheless passed the content/start barrier and
entered synchronized gameplay when the host pressed Start. Its direct bridge
reported Connected with no error. A saved campaign was joined but not resumed
before switching to the fresh-campaign comparison.

The initial lobby display is therefore **not signed off**. Reproduce with the
default browser release build and inspect the received-setup/menu transition;
the evidence does not establish packet loss or its root cause. Do not describe
the whole browser flow or saved-session resumption as passed.

## Defect fixed during verification

Home still registered hidden Display, Help, About and asset-editor buttons.
The widget container could focus them, while the hidden button refused to
handle Tab, leaving keyboard users stuck. The visible button array now follows
the displayed order, legacy entries are disabled, and initial focus is assigned
after registration. `menu_navigation_probe` asserts a complete Home Tab cycle.

## Evidence and limits

Session artifacts are in
`/Users/stefan/Documents/projects/outputs/dunecity-menu-acceptance`:
`ctest-672.log`, native/web build logs, `host-stdout.log` and `guest-stdout.log`
(670 custom), `host-672-stdout.log` and `guest-offline-672-stdout.log` (672
campaign, save/continue and custom), and the isolated `profile-host` /
`profile-guest` saves. Screens were observed through native computer-use
screenshots in this task. Probe renders remain in `build/menu-probe`.
The browser's virtual `/home/web_user/.config/DuneCity/Dune City.log` was read
through the browser debugger: `Starting DuneCity 1.0.672 on Emscripten`, final
peer reports at cycles 13,874 / 14,249 / 14,624, and no desync/digest-mismatch
diagnostic. The native host log also records the cross-platform peer traffic.

No reported desync means the inspected logs contain no desync diagnostic;
it does not claim a counted series of matching digests or a completed campaign.
These same-Mac checks do not establish WAN/NAT, mobile/touch, reconnect, long
campaign progression or cross-version gameplay compatibility. Human usability
testing is still needed to measure whether new players understand the choices.

Native pointer automation was unreliable, so these live journeys used keyboard
navigation. Escape did not dismiss the existing Game Rules and New Map dialogs;
their keyboard dismissal remains a follow-up, not a passed acceptance check.
Other Extras tools retain their existing behavior and have not received deep
functional testing in this menu change.

### Browser build constraint

The default Emscripten Release link was stopped after its final `wasm-opt`
pass spent over 20 minutes in `StackIROptimizer::local2Stack`, using 12.2 GB
physical footprint at sampling. `web-link-sample.txt` captures the stack.
Binaryen `123-346-gfc1a391b9` only invokes this slow pass at optimization level
3 or a size-optimization level ([upstream source](https://github.com/WebAssembly/binaryen/blob/fc1a391b9/src/wasm/wasm-stack-opts.cpp#L32-L38)). For local functional
verification the existing Release objects are linked with `-O2` via
`CMAKE_EXE_LINKER_FLAGS_RELEASE=-O2`; game code and Asyncify remain unchanged.
The project's default build flags are unchanged. A pass on this test build is
not evidence that the default `-O3` browser release finished successfully.
