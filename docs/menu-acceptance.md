# Menu acceptance checks — 13 September 2026

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
