# Joining a running online game

Version 1.0.726 adds direct-session live joining (game protocol 6). New custom
online games have “Allow hot join” checked by default; the map screen can opt out. Legacy hosts omit
`allowLateJoin`, which remains false on the service. ENet/LAN and the legacy
WebSocket relay do not implement this feature.

## Discovery and admission

`POST /v1/admission/list` with `details=1` returns eleven fields per game:
`code|players|max|mode|hexHost|contentHash|hexMod|hexMap|phase|elapsedSeconds|allowLateJoin`.
The old five/seven-field requests remain compatible. Elapsed time is wall time
since the first match start, including pauses. Running games are listed only
when the host is active, opted in, and has a transport seat available. Selecting
a row shows full metadata; compact rows show map, mod and waiting/elapsed time.

A compatibility-checked `/v1/admission/request` creates a private request ticket,
not a room seat. `/v1/admission/request-status` polls it using the same claims;
`cancel=1` cancels the request. Polls use the polling rate allowance, not the
smaller room-creation allowance. Requests expire after inactivity or five minutes.
Only the authenticated host session can use `/v1/p2p/join-requests` to list,
approve, decline or abort. Approval creates a single-use grant bound to the
approved name and claims. General admission remains closed after match start.

From 1.0.727, ordinary joins and hot-join requests require an exact application
version match as well as protocol/content compatibility. A `version_mismatch`
refusal names both host and client versions; clients display an acknowledged,
wrapped popup and return to the lobby without granting a seat or notifying the
host. Older services' generic compatibility errors also open a popup.

## Synchronization

The host chooses an eligible living house and controller in Options → Join
requests. Existing humans cannot be replaced. An AI may be replaced; an extra
controller is allowed only in shared-house modes, up to two controllers per
house. Campaign co-op restricts assignment to the campaign's shared house.

The host saves an authoritative checkpoint and sends a host-only prepare packet
(21). Existing peers pause and acknowledge (22). Only then does the host open a
specific-name membership window and approve the service request. The original
peer identities, certificates and connections remain bound. A service poll alone
cannot reopen a running roster. Losing an original peer still ends the match.

Once the newcomer connects to every existing peer, the host transfers the same
serialized network-save settings to everyone over direct WebRTC channels. The
shared GamePayloadRouter parses these packets for all transports. Chunks are
at most 48 KiB, acknowledged cumulatively by every peer before the next chunk;
there is no unbounded send queue. The complete envelope is capped at 5 MiB and
the saved-game payload retains the existing 4 MiB network-save limit. Larger
saves fail visibly before pausing. No saved state passes through HTTP.

Every receiver validates the complete settings before acknowledging completion.
The existing roster-close/start prepare/ack/commit barrier commits the enlarged
mesh. All peers reload the same checkpoint and retain existing human state,
unit ownership, teams and house colors. Changed controllers keep their slot's
player ID. A new simulation epoch rejects packets from before synchronization;
future commands from the checkpoint are discarded consistently to avoid replaying
buffered input under changed ownership. No pathfinding/node budget is changed.

The host can cancel before the start barrier. Cancellation/timeout discards the
pending checkpoint, revokes the newcomer and resumes the original match. Peers
have a two-minute synchronization deadline. A failure after roster commitment
uses the existing fail-closed start behavior. Progress remains visible while
paused. Original game saves and their version are unchanged.

## Logging and verification

Public seating events retain their existing named activity log. Each resumed
roster commitment also records the updated public start roster with a fresh
start ID, without another public-lobby start notification or anonymous new-match
count. Server analytics settings remain independent of browser diagnostics.

`tools/p2p-signaling/test/test_late_join.py` exercises real HTTP admission,
name-bound grants, cancellation, authorization, compatibility and rate allowances.
`tests/network/run-late-join-probe.py` links a separate diagnostic main against
production game objects and runs three isolated native processes with real local
PHP/WebRTC. Modes cover AI replacement, human sharing, AI sharing and abort;
matching simulation digests are required after resumption. `--browser` provides
a local service/static origin for an actual browser newcomer. Nothing in the
probe changes production objects or uses the user's settings/saves.
