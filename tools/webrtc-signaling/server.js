// DuneCity WebRTC signaling server.
//
// Minimal relay for WebRTC SDP/ICE between exactly two peers in a room.
// No persistence, no accounts, no matchmaking, no game relay, no TURN.
//
// Usable as a module (createSignalingServer) or run directly (node server.js).

import { createServer } from 'node:http';
import { randomBytes } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { WebSocketServer } from 'ws';

export const ROOM_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const ROOM_CODE_RE = new RegExp(`^[${ROOM_ALPHABET}]{4}$`);
const PEER_ID_RE = /^p[0-9a-f]{8}$/;

export const DEFAULTS = {
  host: '127.0.0.1',
  port: 8788,
  // Rooms with 0 connected peers are swept after this grace period (<= 30 s).
  emptyRoomTtlMs: 5_000,
  // Rooms whose remaining peer(s) are idle longer than this are deleted; the
  // remaining peers receive an error and are detached from the room.
  idleRoomTtlMs: 10 * 60 * 1000,
  // Periodic sweep interval (timer is unref'd).
  sweepIntervalMs: 1_000,
  // Application-level limit on a single incoming text message (serialized).
  maxMessageBytes: 256 * 1024,
  // Hard transport cap enforced by `ws` (larger than maxMessageBytes so the
  // server can answer an oversized frame with a "too-large" error instead of
  // the transport dropping the connection).
  wsMaxPayloadBytes: 2 * 1024 * 1024,
  rateLimit: {
    max: 120,
    windowMs: 5_000,
    strikeLimit: 3,
  },
  // Empty array means "allow all origins" (dev default). Set in production.
  allowedOrigins: [],
};

function envInt(name) {
  const raw = process.env[name];
  if (raw === undefined || raw === '') return undefined;
  const value = Number(raw);
  return Number.isFinite(value) ? value : undefined;
}

function envAllowedOrigins() {
  const raw = process.env.SIGNALING_ALLOWED_ORIGINS;
  if (!raw) return [];
  return raw
    .split(',')
    .map((s) => s.trim().toLowerCase())
    .filter(Boolean);
}

function now() {
  return Date.now();
}

function generateRoomCode() {
  const bytes = randomBytes(4); // 256 % 32 === 0, so no modulo bias
  let code = '';
  for (const byte of bytes) code += ROOM_ALPHABET[byte % ROOM_ALPHABET.length];
  return code;
}

function generatePeerId(peers) {
  for (;;) {
    const id = `p${randomBytes(4).toString('hex')}`;
    if (!peers.has(id)) return id;
  }
}

export function createSignalingServer(userOptions = {}) {
  const options = {
    ...DEFAULTS,
    ...userOptions,
    rateLimit: { ...DEFAULTS.rateLimit, ...(userOptions.rateLimit ?? {}) },
    allowedOrigins:
      userOptions.allowedOrigins ?? envAllowedOrigins(),
  };
  const allowedOrigins = options.allowedOrigins.map((o) => String(o).toLowerCase());

  // code -> { code, hostPeerId, peers: Map<peerId, socketState>, createdAt, lastActivityAt, emptySince }
  const rooms = new Map();
  // peerId -> socketState
  const peers = new Map();
  // ws -> socketState { ws, peerId, roomCode, rate: { count, windowStart, strikes } }
  const sockets = new Map();

  function sendJson(state, payload) {
    if (state.ws.readyState === state.ws.OPEN) {
      state.ws.send(JSON.stringify(payload));
    }
  }

  function sendError(state, code, message) {
    sendJson(state, { v: 1, type: 'error', code, message });
  }

  function stats() {
    return { rooms: rooms.size, peers: sockets.size };
  }

  // ---- HTTP -------------------------------------------------------------

  function healthPayload() {
    return JSON.stringify({ status: 'ok', ...stats() });
  }

  function accessControlOrigin(req) {
    if (allowedOrigins.length === 0) return null;
    if (allowedOrigins.includes('*')) return '*';
    const origin = String(req.headers.origin ?? '').toLowerCase();
    if (origin && allowedOrigins.includes(origin)) return origin;
    return allowedOrigins[0];
  }

  const httpServer = createServer((req, res) => {
    const url = (req.url ?? '/').split('?')[0];
    if (req.method === 'GET' && (url === '/health' || url === '/health/')) {
      const headers = { 'content-type': 'application/json; charset=utf-8' };
      const acao = accessControlOrigin(req);
      if (acao) headers['access-control-allow-origin'] = acao;
      res.writeHead(200, headers);
      res.end(healthPayload());
      return;
    }
    res.writeHead(404, { 'content-type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify({ error: 'not found' }));
  });

  // ---- WebSocket ----------------------------------------------------------

  const wss = new WebSocketServer({
    noServer: true,
    maxPayload: options.wsMaxPayloadBytes,
  });

  httpServer.on('upgrade', (req, socket, head) => {
    if (allowedOrigins.length > 0) {
      const origin = String(req.headers.origin ?? '').toLowerCase();
      if (!origin || !allowedOrigins.includes(origin)) {
        socket.write('HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n');
        socket.destroy();
        return;
      }
    }
    wss.handleUpgrade(req, socket, head, (ws) => wss.emit('connection', ws, req));
  });

  wss.on('connection', (ws) => {
    const state = { ws, peerId: null, roomCode: null, rate: { count: 0, windowStart: now(), strikes: 0 } };
    sockets.set(ws, state);
    ws.on('message', (data) => handleIncoming(state, data));
    ws.on('close', () => handleDisconnected(state));
    ws.on('error', () => {
      /* transport-level error; close handler performs cleanup */
    });
  });

  wss.on('error', () => {
    /* keep the process alive; individual sockets clean themselves up */
  });

  // ---- Rate limiting (fixed window per socket) ---------------------------

  function checkRate(state, at) {
    const { max, windowMs } = options.rateLimit;
    const r = state.rate;
    if (at - r.windowStart >= windowMs) {
      r.windowStart = at;
      r.count = 0;
      r.strikes = 0;
    }
    r.count += 1;
    if (r.count <= max) return { limited: false };
    r.strikes += 1;
    return { limited: true, close: r.strikes >= options.rateLimit.strikeLimit };
  }

  // ---- Message handling ----------------------------------------------------

  function handleIncoming(state, data) {
    const at = now();

    const rate = checkRate(state, at);
    if (rate.limited) {
      sendError(
        state,
        'rate-limited',
        `Rate limit exceeded (${options.rateLimit.max} messages per ${options.rateLimit.windowMs} ms); slow down`,
      );
      if (rate.close) state.ws.close(1008, 'rate limit exceeded');
      return;
    }

    const text = data.toString('utf8');
    if (Buffer.byteLength(text, 'utf8') > options.maxMessageBytes) {
      sendError(state, 'too-large', `Message exceeds maximum size of ${options.maxMessageBytes} bytes`);
      return;
    }

    let message;
    try {
      message = JSON.parse(text);
    } catch {
      sendError(state, 'invalid-message', 'Message must be a UTF-8 JSON object');
      return;
    }
    if (message === null || typeof message !== 'object' || Array.isArray(message)) {
      sendError(state, 'invalid-message', 'Message must be a JSON object');
      return;
    }
    if (message.v !== 1) {
      sendError(state, 'unsupported-version', `Unsupported protocol version ${JSON.stringify(message.v)}; expected 1`);
      return;
    }

    switch (message.type) {
      case 'create':
        handleCreate(state, at);
        return;
      case 'join':
        handleJoin(state, message, at);
        return;
      case 'signal':
        handleSignal(state, message, at);
        return;
      default:
        sendError(state, 'invalid-type', `Unknown message type ${JSON.stringify(message.type)}`);
    }
  }

  function requireNotInRoom(state) {
    if (state.roomCode !== null && rooms.has(state.roomCode)) {
      sendError(state, 'already-in-room', 'Peer is already in a room; leave first');
      return false;
    }
    state.roomCode = null;
    if (state.peerId !== null) peers.delete(state.peerId);
    state.peerId = null;
    return true;
  }

  function handleCreate(state, at) {
    if (!requireNotInRoom(state)) return;

    let code = generateRoomCode();
    while (rooms.has(code)) code = generateRoomCode();

    const room = {
      code,
      hostPeerId: null,
      peers: new Map(),
      createdAt: at,
      lastActivityAt: at,
      emptySince: null,
    };
    state.peerId = generatePeerId(peers);
    peers.set(state.peerId, state);
    room.peers.set(state.peerId, state);
    room.hostPeerId = state.peerId;
    state.roomCode = code;
    rooms.set(code, room);

    sendJson(state, { v: 1, type: 'created', room: code, peerId: state.peerId });
  }

  function handleJoin(state, message, at) {
    if (typeof message.room !== 'string' || !ROOM_CODE_RE.test(message.room)) {
      sendError(state, 'invalid-message', 'join requires "room": 4-character room code (A-HJ-NP-Z2-9)');
      return;
    }
    if (!requireNotInRoom(state)) return;

    const room = rooms.get(message.room);
    if (!room) {
      sendError(state, 'room-not-found', `Room ${message.room} does not exist`);
      return;
    }
    if (room.peers.size >= 2) {
      sendError(state, 'room-full', 'Room already has two peers');
      return;
    }

    state.peerId = generatePeerId(peers);
    peers.set(state.peerId, state);
    room.peers.set(state.peerId, state);
    state.roomCode = room.code;
    room.lastActivityAt = at;
    room.emptySince = null;

    sendJson(state, { v: 1, type: 'joined', room: room.code, peerId: state.peerId, host: room.hostPeerId });

    const hostState = room.peers.get(room.hostPeerId);
    if (hostState && hostState !== state) {
      sendJson(hostState, { v: 1, type: 'peer-joined', peerId: state.peerId });
    }
  }

  function handleSignal(state, message, at) {
    if (typeof message.to !== 'string' || !('data' in message)) {
      sendError(state, 'invalid-message', 'signal requires string "to" and a "data" payload');
      return;
    }
    const room = state.roomCode !== null ? rooms.get(state.roomCode) : undefined;
    if (!room || state.peerId === null || !room.peers.has(state.peerId)) {
      sendError(state, 'invalid-target', 'Signal target invalid: sender is not in a room');
      return;
    }
    const targetState = peers.get(message.to);
    if (!targetState || targetState.roomCode !== room.code || targetState.peerId === state.peerId) {
      sendError(state, 'invalid-target', `Signal target ${JSON.stringify(message.to)} is not valid`);
      return;
    }

    room.lastActivityAt = at;
    sendJson(targetState, { v: 1, type: 'signal', from: state.peerId, data: message.data });
  }

  // ---- Disconnection / rooms lifecycle ------------------------------------

  function handleDisconnected(state) {
    sockets.delete(state.ws);
    const peerId = state.peerId;
    state.peerId = null;
    if (peerId !== null) peers.delete(peerId);
    if (state.roomCode === null) return;

    const room = rooms.get(state.roomCode);
    state.roomCode = null;
    if (!room) return;

    room.peers.delete(peerId);
    for (const remaining of room.peers.values()) {
      sendJson(remaining, { v: 1, type: 'peer-left', peerId });
    }
    room.lastActivityAt = now();
    if (room.peers.size === 0) {
      room.emptySince = now();
    } else if (!room.peers.has(room.hostPeerId)) {
      // Promote the remaining peer so later joiners get a valid "host".
      room.hostPeerId = room.peers.keys().next().value;
    }
  }

  function sweep() {
    const at = now();
    for (const room of rooms.values()) {
      if (room.peers.size === 0) {
        if (room.emptySince === null) room.emptySince = at;
        if (at - room.emptySince >= options.emptyRoomTtlMs) {
          rooms.delete(room.code);
        }
      } else if (at - room.lastActivityAt >= options.idleRoomTtlMs) {
        for (const member of room.peers.values()) {
          const peerId = member.peerId;
          member.roomCode = null;
          member.peerId = null;
          if (peerId !== null) peers.delete(peerId);
          sendError(member, 'room-expired', 'Room closed due to inactivity');
        }
        room.peers.clear();
        rooms.delete(room.code);
      }
    }
  }

  const sweepTimer = setInterval(sweep, Math.max(10, options.sweepIntervalMs));
  sweepTimer.unref?.();

  // ---- Shutdown ------------------------------------------------------------

  let closed = false;
  function close() {
    if (closed) return Promise.resolve();
    closed = true;
    clearInterval(sweepTimer);
    for (const ws of wss.clients) ws.terminate();
    rooms.clear();
    peers.clear();
    sockets.clear();
    httpServer.closeIdleConnections?.();
    return new Promise((resolve) => {
      wss.close(() => {});
      httpServer.close(() => resolve());
      httpServer.closeIdleConnections?.();
    });
  }

  return { httpServer, wss, rooms, peers, sockets, options, stats, close };
}

// ---- Direct execution -------------------------------------------------------

const isMain =
  process.argv[1] !== undefined &&
  import.meta.url === pathToFileURL(process.argv[1]).href;

async function main() {
  const port = envInt('PORT') ?? DEFAULTS.port;
  const host = process.env.HOST || DEFAULTS.host;

  const ctx = createSignalingServer();
  ctx.httpServer.listen(port, host, () => {
    const address = ctx.httpServer.address();
    const boundPort = typeof address === 'object' && address ? address.port : port;
    console.log(
      `dunecity-webrtc-signaling listening on ws://${host}:${boundPort}/ (health: http://${host}:${boundPort}/health)` +
        (ctx.options.allowedOrigins.length ? ` allowedOrigins=${ctx.options.allowedOrigins.join(',')}` : ' allowedOrigins=all'),
    );
  });

  let shuttingDown = false;
  const shutdown = (signal) => {
    if (shuttingDown) return;
    shuttingDown = true;
    console.log(`received ${signal}, shutting down`);
    ctx.close().then(() => process.exit(0));
  };
  process.on('SIGINT', () => shutdown('SIGINT'));
  process.on('SIGTERM', () => shutdown('SIGTERM'));
}

if (isMain) {
  main();
}
