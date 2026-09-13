# Menu navigation, local candidate 1.0.674

This change follows Stefan's menu review and approval on 13 September 2026.
It starts from 81ff90a (campaign controls 1.0.669), preserving the campaign AI,
save format and network authorization fixes on that branch.

## Navigation

Home offers Campaign, Custom Game, Join Online, Load Game, Settings, Extras,
and Quit. Continue appears when a readable offline save with an installed mod
exists. It selects the most recently modified eligible save.

Campaign chooses a house, full campaign (levels 1–9) or single mission (1–22),
mod, enemy AI and game rules. Selecting a crest changes the selected house;
Start Campaign / Start Mission is explicit. Offline permits an AI partner.
Online creates a private or public co-op room from that exact setup, with two
controllers sharing a house. Cancelling the online flow retains setup choices.

Custom Game puts map, mod, Offline / Online, online visibility, shared-house option,
players and Game Rules on the existing player setup screen. Map/mod changes
rebuild eligible player slots. Connection/rules changes preserve their roster.
Offline defaults unoccupied primary slots to Easy AI. Online starts admission
only after Create Lobby, transferring the prepared roster into the lobby.
The original centered player roster/map layout is restored, including full player
labels in wide windows and compact rows on small screens. Existing start
validation and peer/config acknowledgements remain authoritative.

Join Online provides Campaign co-op / Custom filters, active-mod selection,
a public game list and an invitation-code field. Public chat has its own view.
LAN and direct-IP / legacy Internet connections remain available as a secondary
entry. The online service's existing metadata and compatibility rules are used;
this change does not invent map or mission metadata in the directory.

Load Game exposes both save locations and reads the saved game type before
launching offline or hosting an online save. Replays are available in Extras.

Extras contains Mods, Map Editor, Asset Editors, Replays, How to Play, and
About & Credits. The Dune2R asset editor requires that mod to be active.
Playable mod selection is also directly accessible from Campaign and Custom.

## Settings

General, Graphics, Audio, Controls and Advanced share one Settings screen.
Graphics includes video resolution, game zoom, scaler, fullscreen, VSync,
cursor, menu layout/colors, and interface size. Web/Android also have aspect
selection. The separate Home Display entry is removed. Controls exposes scroll
speed. Advanced contains campaign AI defaults, default game rules, legacy
network settings and config restoration. The last visited tab is remembered.

Network/name validation runs when those fields change; an unchanged empty
legacy network value cannot block an unrelated audio or graphics adjustment.
Game rules selected during setup stay local unless Remember for new games is
explicitly checked. Settings > Advanced > Default Game Rules remains global.

## Verification and scope

The macOS CTest menu_navigation_probe links the production menu objects with a
test-only entry point. It uses isolated profiles and SDL's dummy driver to
exercise campaign retry state, single-mission ranges, custom roster transfer,
Settings validation, and render the actual widgets at 640×480, 854×480 and 1280×720.
The final native Release build, dependency audit, and all seven CTest targets pass.
Images and logs are in build/menu-probe. Network transport and authorization
continue to have their dedicated CTest coverage.

Live native verification subsequently exercised all four core play routes,
private custom invites, public campaign filters/joining, setup preservation,
offline save/Continue, online custom save routing, and Extras entry points.
It caught and fixed Home's hidden-button keyboard trap. See
`docs/menu-acceptance.md` for exact versions, evidence and remaining limits.

This is a local implementation candidate. It has not been published. Matching 673 browser/native
clients passed lobby display, hosting and campaign/custom gameplay; Stefan also
personally hosted and started a campaign successfully. The source-controlled
web Release link completes. The 674 restoration has separate build and menu
checks; see the acceptance record for the exact scope.
WAN/mobile checks, a richer save metadata list, timed display rollback, parties
and rematch flows remain follow-up work.
