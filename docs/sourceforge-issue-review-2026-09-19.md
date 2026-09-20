# SourceForge issue review — 2026-09-19

Reviewed against DuneCity **1.0.713**, source commit **386ca05**. Read-only review:
no game-code changes, ticket comments/closures, pushes, releases or deployments.

## Implementation follow-up — local 1.0.714

The original inventory below is the 713 assessment. Bugs 105, 86, 113 and 88,
and features 62 and 45, are now implemented locally. Native and browser builds
pass; all seven CTest suites and real-engine regressions pass (Vanilla and
DuneCity). See HANDOVER.md for evidence and behavior. New shortcuts: S Stop
(Shift+S with WASD), T matching types on screen, Ctrl+T matching types on the
map, Shift+T timer. Hover intent and a flashing target outline address feature 45.
Follow-up preference: ordinary movement retains the arrow; only the Move button
or M activates the move icon. Attack/harvester-return hover cues remain.

Two corrections to the assessment: T previously toggled the timer, rather than
being a no-op; the original palace diagnostic also used an unavailable campaign
build item, whose prerequisite refresh can independently remove production.
The corrected regression makes Palace available, tests both option values,
and verifies the cancellation policy. The unconditional cancellation in 713's
House::placeStructure remains direct source evidence of bug 113.

No ticket was closed merely for age. Comments/closures for the six implemented
tickets remain pending because computer access is blocked by the locked Mac.
The build is local and unreleased; browser interaction/download validation is
pending too. Accessibility colors, attack-move and order queuing remain separate
work, as do historical reports requiring reproduction.

## Coverage

SourceForge REST inventory: 116 bug tickets, 65 feature requests and 4 support
requests (185 total). Of these, 39 bugs, 47 features and 3 support requests are
not closed (89). The inventory includes closed history, but the current triage
focuses on outstanding descriptions and comments. This is not an exhaustive
review of all forum threads or historical attached save files.

Raw snapshot: `/tmp/dunecity-sourceforge-details-20260919.json`.
All ticket links below refer to SourceForge, not same-numbered GitHub issues.
Age alone is not grounds to mark a report fixed, and a DuneCity fix does not
establish that every older Dune Legacy binary contains it.

## Recommended first fixes

1. **Bug 105 — Ctrl+0 crash.** Current `Game::handleKeyInput` range-iterates
   `selectedList`, while `ObjectBase::removeFromSelectionLists` erases that same
   element. Isolated real-engine 713 test reproduced SIGSEGV with one selected
   unit. Iterate a stable snapshot or erase safely; tolerate stale object IDs.
2. **Bug 86 — screenshot capture.** `renderReadSurface` allocates from logical
   `getRendererSize` but reads the entire physical render target. With logical
   320x240 and output 1024x768, an oversized diagnostic buffer measured **678,656
   changed bytes beyond the allocation the real function would make**, with SDL
   reporting success. Fix target/output sizing and screenshot write-error reporting
   in both game and menus. The test reserved physical storage, so did not corrupt
   its process. This is a confirmed memory-safety defect, not just a historical crash.
3. **Bug 113 — palace construction cancellation.** With `onlyOnePalace=false`,
   placing one palace reduced the second yard’s queued palace count from 1 to 0.
   `House::placeStructure` unconditionally cancels other palace queues. Apply the
   one-palace option consistently; keep normal multi-yard construction independent.
4. **Bug 88 and feature 62 — missing shortcuts.** S did not stop a selected
   hunting tank in the real handler. T toggles the timer (corrected after review). Add Stop and select-same-type
   controls with clear modifiers when WASD camera is enabled.

Useful subsequent UI/gameplay work: contextual hover cursors (feature 45),
colorblind/friend–foe display (64/44), then separately scoped attack-move (63)
and queued orders (43). New explicit cursor art does not itself satisfy hover
intent, and movement path visualization is not an order queue.

## Verification and boundaries

Diagnostics linked existing 713 engine objects into an isolated test executable;
they did not touch the user’s running match or installed profile. Evidence:
`/tmp/dunecity-sourceforge-probe-713` (Ctrl+0 crash and palace queue),
`/tmp/dunecity-sourceforge-probe-713b` (Mentat/Stop/buffer dimensions), and
`/tmp/dunecity-sourceforge-probe-713c` (guarded screenshot read).
Diagnostic source/runner: `/tmp/sourceforge-campaign-probe.inc`,
`/tmp/sourceforge-probe-body.inc`, `/tmp/run-sourceforge-probe.py`.
The second/third runs deliberately skip Ctrl+0; their generic completion marker
means the diagnostic completed, not that the reported product defects passed.

Mentat help (115) opened a real topic and closed after three click/update cycles.
Current repair-yard code resolves/clears stale jobs and checks the pointer before
carryall use, addressing the specific old 101/107/110/114 dereference; the old
save attachments were not replayed. Reports 117, 118 and 108 include explicit
later-version success comments. Trooper close-range sound (106) now follows the
actual fired projectile. Auto-repair and an independent credits-tick mute exist.

Still worth current reproduction: audio after loading (92), Starport ghost counts
(79), authored scenario Hunt/Sabotage (95), map-editor infantry rotation (102),
mission completion (97), and platform-specific flicker/edge scrolling (93/91).
Air-unit scouting (90) is deliberately disabled in source; enabling it is a
balance/modding decision rather than a crash fix.

## Outstanding-ticket inventory

“Needs reproduction” means unresolved by this review, not obsolete. Feature
proposals retain their design scope; this review does not authorize implementing
all of them.

### Bugs

| Ticket | Current assessment |
| --- | --- |
| [#118: Defautl install location required](https://sourceforge.net/p/dunelegacy/bugs/118/) | Already addressed: reporter confirms 0.98.7.1; current executable-relative data paths also checked. |
| [#117: Your game and AI configuration files are not in line with latest app version.](https://sourceforge.net/p/dunelegacy/bugs/117/) | Reporter confirms fixed in a later version; not evidence of a current startup defect. |
| [#116: Getting 403 Error when trying to find a Internetgame](https://sourceforge.net/p/dunelegacy/bugs/116/) | Legacy metaserver incident; current crossplay/lobby architecture differs. Do not infer current outage. |
| [#115: Mentat Help Tab Bug](https://sourceforge.net/p/dunelegacy/bugs/115/) | Not reproduced: current Mentat topic opened and closed after three click/update cycles. |
| [#114: Systematic crash on a specific action](https://sourceforge.net/p/dunelegacy/bugs/114/) | Same historical repair/carryall crash family as 101/107/110; current resolveRepairUnit/null guard addresses reported dereference. Historical save not replayed. |
| [#113: Completing construction in one Yard cancels the same construction in the other Yard](https://sourceforge.net/p/dunelegacy/bugs/113/) | CONFIRMED CURRENT: placing a palace cancels another yard’s palace queue with onlyOnePalace=false. |
| [#112: SP - 128x128 - Sardaukar Base crash v0.97.2 - v0.96.5.A2](https://sourceforge.net/p/dunelegacy/bugs/112/) | Needs current reproduction: nonspecific crashes across old 0.96/0.97 versions; no current root cause established. |
| [#111: MetaServer error on Multiplayer lobby!! Cannot Create and  Cannot join!](https://sourceforge.net/p/dunelegacy/bugs/111/) | Legacy metaserver incident; current networking differs. |
| [#110: Crash in SP campaign](https://sourceforge.net/p/dunelegacy/bugs/110/) | Historical manual-carryall/repair family; current stale-job guard present; old save not replayed. |
| [#109: MetaServer error on list game ](https://sourceforge.net/p/dunelegacy/bugs/109/) | Legacy metaserver incident; current networking differs. |
| [#108: MT32 music broke in Dune Legacy '0.97.0alpha'](https://sourceforge.net/p/dunelegacy/bugs/108/) | Reporter/commenter confirms music fixed in 0.97.01; do not equate with every modern audio problem. |
| [#107: Crash in multiplayer game - save attached](https://sourceforge.net/p/dunelegacy/bugs/107/) | Historical manual-carryall/repair family; reported patch and current stale-job guard present. |
| [#106: Heavy troopers emit wrong sound when firing close range](https://sourceforge.net/p/dunelegacy/bugs/106/) | Addressed in current Trooper::playAttackSound: actual last-fired projectile selects gun versus rocket sound. |
| [#105: Crash when pressing CTRL+0 with unit selected](https://sourceforge.net/p/dunelegacy/bugs/105/) | CONFIRMED CURRENT CRASH: Ctrl+0 erases from selectedList while range-iterating it; isolated 713 diagnostic exits SIGSEGV. |
| [#104: Remaining starting credits are not stored into first refinery](https://sourceforge.net/p/dunelegacy/bugs/104/) | Needs current reproduction: initial credits versus stored-spice accounting and early campaign goals. |
| [#103: mission bug](https://sourceforge.net/p/dunelegacy/bugs/103/) | Old campaign balance report; current campaign pacing/waves substantially revised. Retest the named mission before changing balance. |
| [#102: Rotation of infantry units in Map Editor](https://sourceforge.net/p/dunelegacy/bugs/102/) | Needs editor round-trip reproduction; rotation angle is serialized, but reported infantry rotation path not exercised. |
| [#101: 0.96.5 alpha 3 crashes upon repairing a unit](https://sourceforge.net/p/dunelegacy/bugs/101/) | Current RepairYard::deployRepairUnit resolves and clears the job, then checks null before dereferencing. Old save not replayed. |
| [#100: HD Scaling & Zoom issue with certain resolutions](https://sourceforge.net/p/dunelegacy/bugs/100/) | Needs current 1080p scaling comparison; old fixed-scale/HD behavior is not proof of a current display defect. |
| [#99: Crash due to "Invalid item ID"](https://sourceforge.net/p/dunelegacy/bugs/99/) | Needs current crash trace; original report lacks a trigger. |
| [#98: Ordors cannot upgrade Heavy Factory after MCV](https://sourceforge.net/p/dunelegacy/bugs/98/) | Needs current mission/tech-tree reproduction; do not change faction unlocks from title alone. |
| [#97: Mission won't end even after destroying enemies?](https://sourceforge.net/p/dunelegacy/bugs/97/) | Needs current saved-game reproduction; mission-end path has changed, old hidden-unit report not verified. |
| [#96: Carryall flight and pickup speed](https://sourceforge.net/p/dunelegacy/bugs/96/) | Balance proposal, not a proven current bug; carryall timing should be evaluated with current combat/repair behavior. |
| [#95: AI scripts don't treat "Sabotage" and "Hunt" commands properly](https://sourceforge.net/p/dunelegacy/bugs/95/) | Still merits a scenario-order probe: authored Hunt/Sabotage versus each AI controller. Not established fixed. |
| [#94: Special weapons aren't used by the Harkonnen & Sardaukar](https://sourceforge.net/p/dunelegacy/bugs/94/) | Mixed: AIPlayer and QuantBot now explicitly launch Death Hand; SmartBot has no doLaunchDeathhand call. Needs controller-specific test. |
| [#93: Graphics issue](https://sourceforge.net/p/dunelegacy/bugs/93/) | Old flicker/driver reports; rendering has changed. Needs hardware-specific current reproduction, especially Wayland. |
| [#92: Sound disappears when loading a game while playing.](https://sourceforge.net/p/dunelegacy/bugs/92/) | Needs load/restart audio lifecycle reproduction; startup-audio fallback does not prove this fixed. |
| [#91: Scroll down fails when cursor is at max Y on non-Unity Ubuntu window manager](https://sourceforge.net/p/dunelegacy/bugs/91/) | Needs Linux window-manager test; current keyboard scrolling offers a workaround, not proof of edge-scroll fix. |
| [#90: Air Units Always have Zero Visibility](https://sourceforge.net/p/dunelegacy/bugs/90/) | Current behavior confirmed by source: AirUnit::assignToMap deliberately omits viewMap and checkPos is empty. Changing scouting is a design/balance decision. |
| [#89: Missile Launchers Like To Get In Melee Range](https://sourceforge.net/p/dunelegacy/bugs/89/) | Needs combat reproduction: current AI kiting changes do not prove player-issued launcher pursuit is fixed. |
| [#88: Hotkey For Stop Command Is Missing](https://sourceforge.net/p/dunelegacy/bugs/88/) | CONFIRMED CURRENT: S does not stop a selected hunting unit; no SDLK_s order handler. Account for optional WASD when adding one. |
| [#87: Cannot Spawn Infantry In Custom Maps Via Reinforcements](https://sourceforge.net/p/dunelegacy/bugs/87/) | Needs infantry reinforcement fixture; old report alone is insufficient. |
| [#86: Crash via "Print Screen"](https://sourceforge.net/p/dunelegacy/bugs/86/) | CONFIRMED CURRENT unsafe capture sizing: logical 320x240 allocation versus 1024x768 output; guarded read writes 678,656 bytes beyond nominal logical allocation. |
| [#85: Ornithopters can not be targeted and won't hunt others](https://sourceforge.net/p/dunelegacy/bugs/85/) | Needs explicit-air-target and Hunt reproduction; do not infer from general canAttack checks alone. |
| [#80: Units just stop doing anything](https://sourceforge.net/p/dunelegacy/bugs/80/) | Broad old idle-unit symptom overlaps many later fixes, including 713 harvester field reselection; keep specific remaining stalls reproducible. |
| [#79: Negative amounts in spaceport](https://sourceforge.net/p/dunelegacy/bugs/79/) | Needs Starport delivery/cancel reproduction; negative displayed counts not established fixed. |
| [#75: SDL_CreateTextureFromSurface() failed: Texture dimensions are limited to 2048x2048](https://sourceforge.net/p/dunelegacy/bugs/75/) | Original size failure was confirmed fixed by reporter; remaining driver/flicker comments overlap 93. |
| [#35: Infantry Trooper shooting rocket during Capturing](https://sourceforge.net/p/dunelegacy/bugs/35/) | Needs capture-order firing test; current C capture hotkey already exists. |
| [#21: Message Ticker's Slow](https://sourceforge.net/p/dunelegacy/bugs/21/) | Old UI responsiveness request, partly improved in original follow-up; low priority without current reproduction. |

### Feature requests

| Ticket | Current assessment |
| --- | --- |
| [#65: New and existing palace superweapons through ObjectData.ini](https://sourceforge.net/p/dunelegacy/feature-requests/65/) | Large data-driven superweapon/modding design; not obsolete, not a small defect fix. |
| [#64: Alternative color for Mercenaries](https://sourceforge.net/p/dunelegacy/feature-requests/64/) | Useful accessibility request; selectable house colors do not establish a colorblind/friend–foe presentation mode. |
| [#63: Could we have a modern "attack-move" command?](https://sourceforge.net/p/dunelegacy/feature-requests/63/) | Useful missing distinct attack-move behavior; existing ground attack and Hunt are not equivalent. |
| [#62: Select all units of same type using T](https://sourceforge.net/p/dunelegacy/feature-requests/62/) | Useful small control feature: SDLK_t case is currently a commented/no-op action, not select-same-type. |
| [#61: Start campaign at desired mission](https://sourceforge.net/p/dunelegacy/feature-requests/61/) | Substantially addressed by current campaign/mission selection; exact incompatible-save recovery workflow remains separate. |
| [#60: More hotkeys](https://sourceforge.net/p/dunelegacy/feature-requests/60/) | Still a feature proposal: contextual building shortcuts. |
| [#59: Modern day Rat buttons scheme would be golden.](https://sourceforge.net/p/dunelegacy/feature-requests/59/) | Already present: left selects/right orders; optional left-click orders added in 711. |
| [#58: Portable install](https://sourceforge.net/p/dunelegacy/feature-requests/58/) | Portable user profile/save directory remains a separate feature from relocatable installation; no --user-dir option found. |
| [#57: More structured data sorting for saved scenarios in Map Editor](https://sourceforge.net/p/dunelegacy/feature-requests/57/) | Editor export ordering proposal; no round-trip/canonicalization assessment made. |
| [#56: Custom graphics and tilesets for maps](https://sourceforge.net/p/dunelegacy/feature-requests/56/) | Partly covered by current mod graphics infrastructure; per-map total-conversion bundling needs a separate scope. |
| [#55: Improved campaign selection screen for famous bonus campaigns](https://sourceforge.net/p/dunelegacy/feature-requests/55/) | Substantially superseded by current campaign selector; bundled fan-campaign completeness not audited. |
| [#54: option to disable campaign mode enemy carryall drops in your base](https://sourceforge.net/p/dunelegacy/feature-requests/54/) | Optional campaign drop restriction remains a design request; current base defense fixes are not that option. |
| [#53: Compile Dune Legacy for the new Broadcom Cortex A53 ARM64 (ARMv8)](https://sourceforge.net/p/dunelegacy/feature-requests/53/) | ARM64 is no longer a blanket architecture blocker (native Apple ARM64 builds); dedicated Raspberry Pi Linux package/performance still separate. |
| [#52: Give option to disable cash tick sound effect.](https://sourceforge.net/p/dunelegacy/feature-requests/52/) | Already present: Settings → Audio → Play Credits SFX, persisted independently. |
| [#51: Specifiable Allies parameter in House properties](https://sourceforge.net/p/dunelegacy/feature-requests/51/) | Needs authored single-player Allies/Team semantics test; cooperative support does not prove this exact INI feature. |
| [#50: Campaign AI behavior specialties](https://sourceforge.net/p/dunelegacy/feature-requests/50/) | Mixed campaign AI design wishlist; many economy/defense behaviors have since changed, no blanket completion claim. |
| [#49: Development: Need a unified unit item list tracker](https://sourceforge.net/p/dunelegacy/feature-requests/49/) | Unit-cap/Starport accounting design; original comments disagree on whether bypass is a bug. |
| [#48: Development: Switch to json format for game saves, configuration etc](https://sourceforge.net/p/dunelegacy/feature-requests/48/) | Save-format migration proposal remains architectural work; changing encoding alone does not ensure compatibility. |
| [#47: Sandworm behavior](https://sourceforge.net/p/dunelegacy/feature-requests/47/) | Sandworm balance/modding request; no current regression established. |
| [#46: Units should move in some sort of formation](https://sourceforge.net/p/dunelegacy/feature-requests/46/) | Formation movement remains a distinct feature; destination/path lines do not implement formation preservation. |
| [#45: Mouse cursor should change to reflect what the right click will do](https://sourceforge.net/p/dunelegacy/feature-requests/45/) | Still relevant: shared cursor icons show explicitly chosen action modes, but normal hover does not yet infer the right-click action. |
| [#44: Friend/Foe colors](https://sourceforge.net/p/dunelegacy/feature-requests/44/) | Useful accessibility feature: friend/foe display mode distinct from selectable player color. |
| [#43: Shift-click to add chain of orders](https://sourceforge.net/p/dunelegacy/feature-requests/43/) | Queued move/attack waypoints remain a feature gap; Shift selection is not an order queue. |
| [#42: Wish List For Next Version](https://sourceforge.net/p/dunelegacy/feature-requests/42/) | Linked forum wishlist; no single actionable defect specified in ticket. |
| [#41: Optional auto-slab building (as seen in D2:TGP)](https://sourceforge.net/p/dunelegacy/feature-requests/41/) | Duplicate of feature 33: optional painted automatic slab queue; separate from AI slab planning. |
| [#38: Build 'x' units; build to infinity](https://sourceforge.net/p/dunelegacy/feature-requests/38/) | Continuous production feature request; ordinary finite build queues are not infinite production. |
| [#37: improvement in graphics](https://sourceforge.net/p/dunelegacy/feature-requests/37/) | Alternative Genesis art/content project, not an engine defect. |
| [#36: Custom game on random map](https://sourceforge.net/p/dunelegacy/feature-requests/36/) | Convenience feature: random map generation directly in game setup; editor generation is separate. |
| [#35: HQ4X](https://sourceforge.net/p/dunelegacy/feature-requests/35/) | 4x zoom/HQ4x request; current game exposes three zoom levels. |
| [#34: Change MIDI device in the game or ini file.](https://sourceforge.net/p/dunelegacy/feature-requests/34/) | External MIDI output selection request; not addressed by audio startup fallback. |
| [#33: Optional auto-slab building (as seen in D2:TGP)](https://sourceforge.net/p/dunelegacy/feature-requests/33/) | Optional painted auto-slab queue; not established implemented for human players. |
| [#32: Optional auto-repair of buildings](https://sourceforge.net/p/dunelegacy/feature-requests/32/) | Already present: sidebar Auto repair toggle for paid building repair. |
| [#30: Wishlist 2](https://sourceforge.net/p/dunelegacy/feature-requests/30/) | Mixed wishlist; split individual features before implementation. Repair airlift exists but other items differ. |
| [#29: Select music track on Options](https://sourceforge.net/p/dunelegacy/feature-requests/29/) | Music track selection UI request; not established implemented. |
| [#28: Wishlist](https://sourceforge.net/p/dunelegacy/feature-requests/28/) | Mixed wishlist; some shortcuts/repair controls exist, hotkey remapping and waypoints remain distinct. |
| [#27: Ornitopther Maneuvers](https://sourceforge.net/p/dunelegacy/feature-requests/27/) | Ornithopter maneuver/balance request; needs current behavior comparison. |
| [#26: Multiple building use](https://sourceforge.net/p/dunelegacy/feature-requests/26/) | Multi-building batch production feature; not equivalent to multiple independent factories. |
| [#22: Recruiting 3 infantry/ troopers at once, with the cost of 2](https://sourceforge.net/p/dunelegacy/feature-requests/22/) | Infantry bundle production/balance proposal; not a bug. |
| [#21: Death Hand fire power and cooldown buff](https://sourceforge.net/p/dunelegacy/feature-requests/21/) | Death Hand balance preference; do not adopt old tuning automatically. |
| [#20: Starport prices for Trike and Quad too low](https://sourceforge.net/p/dunelegacy/feature-requests/20/) | Starport price-floor balance preference; no current defect established. |
| [#17: Playing all music files](https://sourceforge.net/p/dunelegacy/feature-requests/17/) | Needs directory-music cue reproduction for all campaign/mentat screens. |
| [#16: Configurable hotkeys for commands](https://sourceforge.net/p/dunelegacy/feature-requests/16/) | Full configurable key mapping remains a feature; optional WASD alone is not remapping. |
| [#14: New Map](https://sourceforge.net/p/dunelegacy/feature-requests/14/) | Map contribution/support discussion, not a current engine defect. |
| [#13: Some potential improvement](https://sourceforge.net/p/dunelegacy/feature-requests/13/) | Mixed art/faction content request; Super Dune campaigns exist, remaining art requests separate. |
| [#12: Radar info is missing 'friends' and 'enemy' count](https://sourceforge.net/p/dunelegacy/feature-requests/12/) | Radar statistics and balance wishlist; individual requirements need scope. |
| [#10: Spice Bloom Spawning](https://sourceforge.net/p/dunelegacy/feature-requests/10/) | Optional spice-regeneration design; deliberately disputed in comments, not an automatic bug fix. |
| [#8: "Stand still but fire" mode for units](https://sourceforge.net/p/dunelegacy/feature-requests/8/) | Strict hold-position while firing requires behavior test/design; Guard/Stop do not by themselves prove the requested semantics. |

### Support requests

| Ticket | Current assessment |
| --- | --- |
| [#4: Win 7(64) Build v0.97.02 MetaServer error on list game server](https://sourceforge.net/p/dunelegacy/support-requests/4/) | Legacy metaserver failure; no current server outage established. |
| [#3: bCheatsEnableld in Game.cpp](https://sourceforge.net/p/dunelegacy/support-requests/3/) | Answered in ticket discussion; development/cheat documentation, not current failure. |
| [#2: Win 7(64) Build v0.96.3](https://sourceforge.net/p/dunelegacy/support-requests/2/) | 2013 Windows/SDL1.2 build environment; obsolete instructions relative to current CMake/SDL2 builds. |

