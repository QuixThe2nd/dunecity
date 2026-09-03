// Integration tests for the DuneCity WebRTC signaling server.
// Uses node:test + node:assert plus a tiny `ws` client wrapper.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import WebSocket from 'ws';
import { createSignalingServer, ROOM_ALPHABET } from '../server.js';

const ROOM_CODE_RE = new RegExp(`^[${ROOM_ALPHABET}]{4}$`);
const PEER_ID_RE = /^p[0-9a-f]{8}$/;
const HERE = path.dirname(fileURLToPath(import.meta.url));

// ---- helpers ---------------------------------------------------------------

async function startServer(options = {}) {
  const ctx = createSignalingServer(options);
  await new Promise((resolve, reject) => {
    ctx.httpServer.once('error', reject);
    ctx.httpServer.listen(0, '127.0.0.1', resolve);
  });
  const port = ctx.httpServer.address().port;
  return {
    ...ctx,
    url: `ws://127.0.0.1:${port}/`,
    httpUrl: `http://127.0.0.1:${port}`,
  };
}

class Client {
  constructor(ws) {
    this.ws = ws;
    this.messages = [];
    this.waiters = [];
    this.closeInfo = null;
    this.opened = new Promise((resolve, reject) => {
      ws.once('open', resolve);
      ws.once('error', reject);
    });
    ws.on('message', (raw) => {
      const text = raw.toString('utf8');
      let message;
      try {
        message = JSON.parse(text);
      } catch {
        message = { unparsed: text };
      }
      this.messages.push(message);
      this.pump();
    });
    ws.on('close', (code, reason) => {
      this.closeInfo = { code, reason: reason.toString() };
      this.pump();
      this.rejectAll(new Error(`socket closed (code ${code})`));
    });
    ws.on('error', (err) => {
      this.lastError = err;
    });
  }

  pump() {
    this.waiters = this.waiters.filter((waiter) => {
      if (waiter.done) return false;
      const value = waiter.predicate(this);
      if (value !== undefined) {
        waiter.done = true;
        clearTimeout(waiter.timer);
        waiter.resolve(value);
        return false;
      }
      return true;
    });
  }

  rejectAll(err) {
    for (const waiter of this.waiters) {
      if (!waiter.done) {
        waiter.done = true;
        clearTimeout(waiter.timer);
        waiter.reject(err);
      }
    }
    this.waiters = [];
  }

  expect(predicate, timeoutMs = 3000) {
    const immediate = predicate(this);
    if (immediate !== undefined) return Promise.resolve(immediate);
    return new Promise((resolve, reject) => {
      const waiter = { predicate, resolve, reject, done: false, timer: null };
      waiter.timer = setTimeout(() => {
        if (waiter.done) return;
        waiter.done = true;
        reject(
          new Error(
            `timeout waiting for message; received so far: ${JSON.stringify(this.messages).slice(0, 1500)}` +
              (this.closeInfo ? `; closed: ${JSON.stringify(this.closeInfo)}` : ''),
          ),
        );
      }, timeoutMs);
      this.waiters.push(waiter);
    });
  }

  send(payload) {
    this.ws.send(typeof payload === 'string' ? payload : JSON.stringify(payload));
  }

  close() {
    this.ws.terminate();
  }
}

async function connect(url, wsOptions = {}) {
  const client = new Client(new WebSocket(url, wsOptions));
  await client.opened;
  return client;
}

const messageOfType = (type) => (client) => client.messages.find((m) => m.type === type);
const errorOfCode = (code) => (client) => client.messages.find((m) => m.type === 'error' && m.code === code);
const gotClosed = (client) => client.closeInfo ?? undefined;

// Creates a fresh room with both peers connected; resolves { server, a, b, room, aId, bId }.
async function createPair(server) {
  const a = await connect(server.url);
  a.send({ v: 1, type: 'create' });
  const created = await a.expect(messageOfType('created'));
  const b = await connect(server.url);
  b.send({ v: 1, type: 'join', room: created.room });
  const joined = await b.expect(messageOfType('joined'));
  const peerJoined = await a.expect(messageOfType('peer-joined'));
  assert.equal(peerJoined.peerId, joined.peerId);
  return { a, b, room: created.room, aId: created.peerId, bId: joined.peerId };
}

function cleanup(server, ...clients) {
  for (const client of clients) client?.close();
  server.close();
}

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// ---- tests -----------------------------------------------------------------

test('create returns a room code and own peerId', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'create' });
    const created = await client.expect(messageOfType('created'));
    assert.match(created.room, ROOM_CODE_RE);
    assert.match(created.peerId, PEER_ID_RE);
    assert.equal(created.v, 1);
    assert.equal(server.rooms.size, 1);
    assert.equal(server.stats().rooms, 1);
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('join succeeds, echoes host, and notifies host with peer-joined', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);
    assert.equal(server.rooms.get(pair.room).peers.size, 2);
    // The joiner learns the host peerId:
    const joiner = pair.b;
    const joined = joiner.messages.find((m) => m.type === 'joined');
    assert.equal(joined.room, pair.room);
    assert.equal(joined.host, pair.aId);
    assert.equal(joined.peerId, pair.bId);
    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('signals relay verbatim in both directions', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);

    const offer = { type: 'offer', sdp: 'v=0\r\no=- 46117317 2 IN IP4 127.0.0.1\r\ns=-\r\n' };
    pair.a.send({ v: 1, type: 'signal', to: pair.bId, data: offer });
    const gotOffer = await pair.b.expect(messageOfType('signal'));
    assert.equal(gotOffer.from, pair.aId);
    assert.deepEqual(gotOffer.data, offer);

    const candidate = { candidate: 'candidate:1 1 UDP 2130706431 192.168.1.4 8998 typ host', sdpMid: '0' };
    pair.b.send({ v: 1, type: 'signal', to: pair.aId, data: candidate });
    const gotCandidate = await pair.a.expect(messageOfType('signal'));
    assert.equal(gotCandidate.from, pair.bId);
    assert.deepEqual(gotCandidate.data, candidate);

    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('signal to nonexistent or invalid target returns invalid-target', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);

    pair.a.send({ v: 1, type: 'signal', to: 'pdeadbee', data: { sdp: 'x' } });
    const err1 = await pair.a.expect(errorOfCode('invalid-target'));
    assert.equal(err1.type, 'error');

    pair.b.send({ v: 1, type: 'signal', to: pair.bId, data: { sdp: 'x' } }); // self
    await pair.b.expect(errorOfCode('invalid-target'));

    // A peer that never entered a room has no valid targets either:
    const loner = await connect(server.url);
    loner.send({ v: 1, type: 'signal', to: pair.aId, data: { sdp: 'x' } });
    await loner.expect(errorOfCode('invalid-target'));
    loner.close();

    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('third peer joining a full room gets room-full', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);
    const third = await connect(server.url);
    third.send({ v: 1, type: 'join', room: pair.room });
    const err = await third.expect(errorOfCode('room-full'));
    assert.equal(err.message.includes('two peers'), true);
    assert.equal(third.messages.some((m) => m.type === 'joined'), false);
    third.close();
    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('join with missing, malformed, or unknown room code', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);

    client.send({ v: 1, type: 'join' });
    await client.expect(errorOfCode('invalid-message'));
    client.send({ v: 1, type: 'join', room: 1234 });
    await client.expect(errorOfCode('invalid-message'));
    client.send({ v: 1, type: 'join', room: 'ab3d' }); // lowercase not in alphabet
    await client.expect(errorOfCode('invalid-message'));
    client.send({ v: 1, type: 'join', room: 'AB0D' }); // 0 not in alphabet
    await client.expect(errorOfCode('invalid-message'));
    client.send({ v: 1, type: 'join', room: 'AB3DE' }); // 5 chars
    await client.expect(errorOfCode('invalid-message'));

    client.send({ v: 1, type: 'join', room: 'WXYZ' }); // well-formed but nonexistent
    await client.expect(errorOfCode('room-not-found'));
    assert.equal(server.rooms.size, 0);
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('peer disconnect sends peer-left to the other peer and cleans up the room', async () => {
  const server = await startServer({ emptyRoomTtlMs: 80, sweepIntervalMs: 10, idleRoomTtlMs: 60_000 });
  try {
    const pair = await createPair(server);
    pair.b.close();
    const left = await pair.a.expect(messageOfType('peer-left'));
    assert.equal(left.peerId, pair.bId);

    // Room survives with one peer:
    await sleep(150);
    assert.equal(server.rooms.size, 1);
    assert.equal(server.rooms.get(pair.room).peers.size, 1);

    // Once everyone is gone, the empty room is swept:
    pair.a.close();
    await sleep(400);
    assert.equal(server.rooms.size, 0);
    assert.equal(server.peers.size, 0);
  } finally {
    await cleanup(server);
  }
});

test('host promotion: joiner can still be reached and reported as host after host leaves', async () => {
  const server = await startServer({ idleRoomTtlMs: 60_000 });
  try {
    const pair = await createPair(server);
    pair.a.close();
    await pair.b.expect(messageOfType('peer-left'));
    await sleep(50);

    const lateJoiner = await connect(server.url);
    lateJoiner.send({ v: 1, type: 'join', room: pair.room });
    const joined = await lateJoiner.expect(messageOfType('joined'));
    assert.equal(joined.host, pair.bId);

    pair.b.send({ v: 1, type: 'signal', to: joined.peerId, data: 'ping' });
    const relayed = await lateJoiner.expect(messageOfType('signal'));
    assert.equal(relayed.from, pair.bId);
    assert.equal(relayed.data, 'ping');

    lateJoiner.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('a peer may only be in one room: create/create and create/join give already-in-room', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'create' });
    const created = await client.expect(messageOfType('created'));

    client.send({ v: 1, type: 'create' });
    await client.expect(errorOfCode('already-in-room'));

    client.send({ v: 1, type: 'join', room: created.room });
    await client.expect(errorOfCode('already-in-room'));
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('malformed JSON and non-object JSON are rejected with invalid-message', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send('this is not json');
    await client.expect(errorOfCode('invalid-message'));
    client.send('42');
    await client.expect(errorOfCode('invalid-message'));
    client.send('"a string"');
    await client.expect(errorOfCode('invalid-message'));
    client.send('["array"]');
    await client.expect(errorOfCode('invalid-message'));
    client.send('null');
    await client.expect(errorOfCode('invalid-message'));

    // Connection is still usable afterwards:
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('messages with a protocol version other than 1 are rejected with unsupported-version', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send({ v: 2, type: 'create' });
    await client.expect(errorOfCode('unsupported-version'));
    client.send({ type: 'create' }); // missing v
    await client.expect(errorOfCode('unsupported-version'));
    client.send({ v: '1', type: 'create' }); // string, not number
    await client.expect(errorOfCode('unsupported-version'));
    assert.equal(client.messages.some((m) => m.type === 'created'), false);
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('unknown or missing type is rejected with invalid-type', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'wat' });
    await client.expect(errorOfCode('invalid-type'));
    client.send({ v: 1 });
    await client.expect(errorOfCode('invalid-type'));
    client.send({ v: 1, type: 7 });
    await client.expect(errorOfCode('invalid-type'));
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('signal without string "to" or without "data" is rejected with invalid-message', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);
    pair.a.send({ v: 1, type: 'signal', data: { sdp: 'x' } }); // missing to
    await pair.a.expect(errorOfCode('invalid-message'));
    pair.a.send({ v: 1, type: 'signal', to: 42, data: { sdp: 'x' } }); // non-string to
    await pair.a.expect(errorOfCode('invalid-message'));
    pair.a.send({ v: 1, type: 'signal', to: pair.bId }); // missing data
    await pair.a.expect(errorOfCode('invalid-message'));
    assert.equal(pair.b.messages.some((m) => m.type === 'signal'), false);
    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('oversized messages are rejected with too-large and do not kill the connection', async () => {
  const server = await startServer();
  try {
    const pair = await createPair(server);
    const huge = 'x'.repeat(300 * 1024); // > 256 KiB serialized
    pair.a.send({ v: 1, type: 'signal', to: pair.bId, data: huge });
    await pair.a.expect(errorOfCode('too-large'));
    assert.equal(pair.b.messages.some((m) => m.type === 'signal'), false);

    // Still alive:
    pair.a.send({ v: 1, type: 'signal', to: pair.bId, data: { ok: true } });
    const relayed = await pair.b.expect(messageOfType('signal'));
    assert.deepEqual(relayed.data, { ok: true });
    pair.a.close();
    pair.b.close();
  } finally {
    await cleanup(server);
  }
});

test('rate limiting kicks in after a burst of messages (defaults)', async () => {
  const server = await startServer(); // default: 120 messages / 5 s window
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));

    // Burst well past the 120-message window budget. Every message is valid
    // JSON with v:1 but an unknown type, so each draws an invalid-type error
    // until the limiter engages.
    for (let i = 0; i < 130; i += 1) {
      client.send({ v: 1, type: 'tick', n: i });
    }

    const limited = await client.expect(errorOfCode('rate-limited'));
    assert.equal(limited.code, 'rate-limited');

    // Persistent abuse closes the socket (3 strikes) with 1008:
    const closeInfo = await client.expect(gotClosed, 3000);
    assert.equal(closeInfo.code, 1008);
  } finally {
    await cleanup(server);
  }
});

test('rate limit state resets after the window passes', async () => {
  const server = await startServer({ rateLimit: { max: 3, windowMs: 150 } });
  try {
    const client = await connect(server.url);
    for (let i = 0; i < 3; i += 1) client.send({ v: 1, type: 'tick', n: i });
    await sleep(50);
    assert.equal(client.messages.filter((m) => m.type === 'error' && m.code === 'invalid-type').length, 3);
    assert.equal(client.messages.some((m) => m.code === 'rate-limited'), false);

    client.send({ v: 1, type: 'tick', n: 3 }); // over the limit
    await client.expect(errorOfCode('rate-limited'));

    await sleep(250); // window + strikes reset
    client.send({ v: 1, type: 'tick', n: 4 });
    const reply = await client.expect((c) =>
      c.messages.filter((m) => m.code === 'invalid-type').length >= 4 ? true : undefined,
    );
    assert.equal(reply, true);
    assert.equal(
      client.messages.filter((m) => m.code === 'rate-limited').length,
      1,
      'no additional rate-limited errors after window reset',
    );
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('idle room expires and detaches the remaining peer', async () => {
  const server = await startServer({ idleRoomTtlMs: 150, emptyRoomTtlMs: 10_000, sweepIntervalMs: 10 });
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));

    await sleep(500); // > idleRoomTtlMs + a few sweeps
    const expired = await client.expect(errorOfCode('room-expired'));
    assert.equal(expired.code, 'room-expired');
    assert.equal(server.rooms.size, 0);
    assert.equal(server.peers.size, 0);

    // The peer was detached, so it may create a new room:
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('health endpoint reports 200 ok and other paths 404', async () => {
  const server = await startServer();
  try {
    const client = await connect(server.url);
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));

    const health = await fetch(`${server.httpUrl}/health`);
    assert.equal(health.status, 200);
    assert.match(health.headers.get('content-type'), /application\/json/);
    const body = await health.json();
    assert.deepEqual(body, { status: 'ok', rooms: 1, peers: 1 });

    const missing = await fetch(`${server.httpUrl}/nope`);
    assert.equal(missing.status, 404);
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('allowed origins: disallowed Origin is refused, allowed Origin works, health sends ACAO', async () => {
  const server = await startServer({ allowedOrigins: ['https://game.example'] });
  try {
    // Disallowed origin: the upgrade is rejected with 403 and the socket errors.
    const bad = new WebSocket(server.url, { headers: { Origin: 'https://evil.example' } });
    const badOutcome = new Promise((resolve, reject) => {
      bad.once('open', () => reject(new Error('unexpected connection for disallowed origin')));
      bad.once('error', (err) => resolve(err));
    });
    const badError = await badOutcome;
    assert.match(badError.message, /403/);

    // Missing origin header is likewise refused:
    const none = new WebSocket(server.url, { headers: {} });
    const noneOutcome = new Promise((resolve, reject) => {
      none.once('open', () => reject(new Error('unexpected connection without origin')));
      none.once('error', (err) => resolve(err));
    });
    await noneOutcome;

    // Allowed origin connects fine:
    const client = await connect(server.url, { headers: { Origin: 'https://game.example' } });
    client.send({ v: 1, type: 'create' });
    await client.expect(messageOfType('created'));

    // Health reflects the CORS header:
    const health = await fetch(`${server.httpUrl}/health`, { headers: { Origin: 'https://game.example' } });
    assert.equal(health.status, 200);
    assert.equal(health.headers.get('access-control-allow-origin'), 'https://game.example');
    client.close();
  } finally {
    await cleanup(server);
  }
});

test('server.js runs directly: binds PORT, serves health, relays, and shuts down on SIGTERM', async () => {
  const child = spawn(process.execPath, [path.join(HERE, '..', 'server.js')], {
    env: { ...process.env, PORT: '0', HOST: '127.0.0.1' },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  let stdout = '';
  child.stdout.on('data', (chunk) => {
    stdout += chunk.toString();
  });
  let stderr = '';
  child.stderr.on('data', (chunk) => {
    stderr += chunk.toString();
  });

  const ready = new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`server did not start; stdout=${stdout} stderr=${stderr}`)), 8000);
    child.stdout.on('data', () => {
      const match = stdout.match(/ws:\/\/127\.0\.0\.1:(\d+)\//);
      if (match) {
        clearTimeout(timer);
        resolve(Number(match[1]));
      }
    });
  });

  const port = await ready;
  try {
    const health = await fetch(`http://127.0.0.1:${port}/health`);
    assert.equal(health.status, 200);
    const body = await health.json();
    assert.equal(body.status, 'ok');
    assert.equal(body.rooms, 0);

    const a = await connect(`ws://127.0.0.1:${port}/`);
    a.send({ v: 1, type: 'create' });
    const created = await a.expect(messageOfType('created'));
    assert.match(created.room, ROOM_CODE_RE);
    a.close();
    await sleep(100); // let the disconnect propagate before shutdown
  } finally {
    const exited = new Promise((resolve) => child.once('exit', (code, signal) => resolve({ code, signal })));
    child.kill('SIGTERM');
    const result = await exited;
    assert.equal(result.signal !== 'SIGKILL', true);
    assert.equal(stdout.includes('listening'), true);
  }
});
