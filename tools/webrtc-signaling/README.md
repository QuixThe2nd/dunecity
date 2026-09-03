# DuneCity WebRTC Signaling Server

A minimal Node.js WebSocket signaling service for DuneCity's WebRTC
peer-to-peer sessions. It relays SDP offers/answers and ICE candidates between
exactly **two peers in a room**, and nothing else.

It deliberately does **not** provide:

- persistence of any kind (rooms live in memory only)
- accounts, authentication, or authorization of peers
- matchmaking or room discovery (you must know the 4-character room code)
- game data relay (once WebRTC connects, traffic is peer-to-peer)
- TURN/STUN services (configure those separately, e.g. coturn)

## Running

```bash
npm install        # once; installs the single dependency `ws`
npm start          # runs `node server.js`
```

Defaults: `ws://127.0.0.1:8788/` (WebSocket endpoint, path is not restricted)
and `http://127.0.0.1:8788/health`.

### Environment variables

| Variable                       | Default     | Meaning                                             |
| ------------------------------ | ----------- | --------------------------------------------------- |
| `PORT`                         | `8788`      | TCP port (`0` picks an ephemeral port)               |
| `HOST`                         | `127.0.0.1` | Bind address (use `0.0.0.0` to expose externally)    |
| `SIGNALING_ALLOWED_ORIGINS`    | _(empty)_   | Comma-separated allowlist of accepted `Origin` values |

### Using as a module

```js
import { createSignalingServer } from './server.js';

const ctx = createSignalingServer({
  allowedOrigins: ['https://dunecity.example'],
  // rateLimit: { max: 120, windowMs: 5000, strikeLimit: 3 },
  // emptyRoomTtlMs: 5000, idleRoomTtlMs: 600000, sweepIntervalMs: 1000,
  // maxMessageBytes: 256 * 1024,
});
ctx.httpServer.listen(8788, '127.0.0.1');
// ctx: { httpServer, wss, rooms, peers, sockets, options, stats(), close() }
```

`stats()` returns `{ rooms, peers }` (rooms = live rooms, peers = connected
sockets). `close()` terminates all client sockets, clears timers, and closes
the HTTP server; it returns a Promise that resolves when shutdown completes.

## Wire protocol

JSON **text frames** over a single WebSocket endpoint (default path `/`).
Protocol version is `1`; every message and reply carries `"v":1`.

### Client -> server

| Message                                        | Effect                                                                                     |
| ---------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `{"v":1,"type":"create"}`                      | Create a room. Reply: `{"v":1,"type":"created","room":"AB3D","peerId":"p1a2b3c4"}`          |
| `{"v":1,"type":"join","room":"AB3D"}`          | Join a room. Reply to joiner: `{"v":1,"type":"joined","room":"AB3D","peerId":"...","host":"<hostPeerId>"}`; the host additionally receives `{"v":1,"type":"peer-joined","peerId":"<joinerPeerId>"}` |
| `{"v":1,"type":"signal","to":"<peerId>","data":<opaque>}` | Relay `data` verbatim to that peer. Recipient gets `{"v":1,"type":"signal","from":"<senderPeerId>","data":<same opaque value>}` |

`data` is opaque JSON (SDP strings, ICE candidate objects, ...) and is relayed
unchanged. Maximum serialized message size is **256 KiB**.

### Server -> client

| Message                                    | When                                                        |
| ------------------------------------------ | ----------------------------------------------------------- |
| `{"v":1,"type":"peer-left","peerId":"..."}` | The other peer's socket closed (or was promoted away/room expired for it) |
| `{"v":1,"type":"error","code":"...","message":"..."}` | Any invalid input or policy violation                       |

Error codes:

| Code                   | Meaning                                                                     |
| ---------------------- | --------------------------------------------------------------------------- |
| `unsupported-version`  | `v` missing or not exactly `1`; no further processing of that message       |
| `invalid-message`      | Not JSON, not an object, malformed `join` room code, or `signal` missing string `to` / missing `data` |
| `invalid-type`         | Unknown or missing `type`                                                   |
| `invalid-target`       | `signal` addressed to a peerId that does not exist, is not in your room, or is yourself |
| `room-not-found`       | `join` for a room code that does not exist                                  |
| `room-full`            | Room already has two peers                                                  |
| `too-large`            | Message exceeds 256 KiB serialized                                          |
| `rate-limited`         | Per-socket rate limit exceeded (see below)                                  |
| `already-in-room`      | `create`/`join` while already in a room (a peer is in at most one room)     |
| `room-expired`         | *(extension)* The room was closed after its idle TTL; the peer was detached and may create/join again |

### Rules

- Exactly 2 peers per room. A third `join` gets `room-full`.
- Room codes are 4 characters from an unambiguous alphabet
  (`ABCDEFGHJKLMNPQRSTUVWXYZ23456789` — no `0`/`O`/`1`/`I`), generated
  randomly and regenerated on collision with a live room.
- Peer IDs are opaque (`p` + 8 hex characters), assigned when entering a room.
- If the host disconnects, the remaining peer is promoted to host and is
  reported as `"host"` to subsequent joiners.

## HTTP endpoints

| Path     | Behavior                                                                     |
| -------- | ---------------------------------------------------------------------------- |
| `GET /health` | `200` with `{"status":"ok","rooms":N,"peers":M}`; carries `Access-Control-Allow-Origin` when an origin allowlist is configured |
| anything else | `404`                                                                        |

## Limits

| Limit                | Default       | Behavior                                                                                                        |
| -------------------- | ------------- | --------------------------------------------------------------------------------------------------------------- |
| Message size         | 256 KiB       | Larger messages get `too-large`; a hard transport cap of 2 MiB (`ws` `maxPayload`) drops absurd frames           |
| Rate limit           | 120 msgs / 5 s per socket (fixed window) | The first over-limit message gets `rate-limited`. If the socket keeps exceeding the limit (3 strikes within the window), it is closed with close code `1008`. Counters and strikes reset when a new window starts. |
| Empty room TTL       | 5 s           | A room whose last peer disconnected is deleted by a periodic sweep (sweep runs every 1 s; the sweep timer is `unref`'d) |
| Idle room TTL        | 10 minutes    | A room whose remaining peer(s) stopped sending messages is deleted; remaining peers get a `room-expired` error and are detached so they can join a new room |

All limits are configurable via `createSignalingServer` options
(`maxMessageBytes`, `rateLimit`, `emptyRoomTtlMs`, `idleRoomTtlMs`,
`sweepIntervalMs`) — which is also how the test suite exercises them quickly.

## Security notes

- **Set `SIGNALING_ALLOWED_ORIGINS` in production** (e.g.
  `https://dunecity.example`). While the list is non-empty, WebSocket upgrades
  with a `Origin` header not on the list are rejected with `403`, and
  `/health` replies carry the matching `Access-Control-Allow-Origin`. The
  default (empty list) accepts all origins, which is fine for local
  development only.
- There is **no authentication**: anyone who learns a room code can occupy its
  second slot (the room is destroyed if either peer misbehaves — just leave).
  Signaling carries no secrets, but run it behind TLS (a reverse proxy such as
  nginx/caddy terminating WSS) before exposing it publicly.
- No message contents are logged; the only log line is the startup banner.
- Rate limiting and size caps exist to bound abuse, not to replace a real
  firewall or DDoS protection.
- WebRTC media/data itself is peer-to-peer and never touches this server.

## Tests

```bash
npm test   # node --test test/
```
