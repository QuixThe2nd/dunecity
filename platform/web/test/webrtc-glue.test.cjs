// Unit and integration tests for platform/web/webrtc_glue.js (createDuneCityWebRtc).
// Mocks RTCPeerConnection / RTCDataChannel / WebSocket so the DI core can run under Node.

'use strict';

const { test } = require('node:test');
const assert = require('node:assert/strict');
const WebSocket = require('ws');
const {
  createDuneCityWebRtc,
  DUNECITY_WEBRTC_CONTROL_OPTIONS,
  DUNECITY_WEBRTC_COMMANDS_OPTIONS,
  DUNECITY_WEBRTC_CONTROL_HIGH_WATER,
  DUNECITY_WEBRTC_COMMANDS_HIGH_WATER,
  DUNECITY_WEBRTC_EVENT_CONNECT,
  DUNECITY_WEBRTC_EVENT_DISCONNECT,
  DUNECITY_WEBRTC_EVENT_MESSAGE,
  DUNECITY_WEBRTC_EVENT_STATE,
  DUNECITY_WEBRTC_STATE_CONNECTING,
  DUNECITY_WEBRTC_STATE_CONNECTED,
  DUNECITY_WEBRTC_STATE_FAILED,
} = require('../webrtc_glue.js');

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// ---- mock WebRTC -----------------------------------------------------------

class MockDataChannel {
  constructor(label, options = {}) {
    this.label = label;
    this.options = options;
    this.binaryType = 'arraybuffer';
    this.readyState = 'connecting';
    this.bufferedAmount = 0;
    this.bufferedAmountLowThreshold = 0;
    this.sent = [];
    this.onopen = null;
    this.onclose = null;
    this.onerror = null;
    this.onmessage = null;
    this.onbufferedamountlow = null;
  }

  send(data) {
    this.sent.push(data instanceof Uint8Array ? data : new Uint8Array(data));
    this.bufferedAmount += this.sent[this.sent.length - 1].length;
  }

  open() {
    if (this.readyState === 'open') return;
    this.readyState = 'open';
    this.onopen?.({ type: 'open', target: this });
  }

  close() {
    if (this.readyState === 'closed') return;
    this.readyState = 'closed';
    this.onclose?.({ type: 'close', target: this });
  }

  deliver(bytes) {
    const payload = bytes instanceof Uint8Array ? bytes.buffer : bytes;
    this.onmessage?.({ data: payload, type: 'message', target: this });
  }

  setBufferedAmount(amount) {
    this.bufferedAmount = amount;
    if (amount <= this.bufferedAmountLowThreshold) {
      this.onbufferedamountlow?.({ type: 'bufferedamountlow', target: this });
    }
  }
}

class MockPeerConnection {
  constructor(config) {
    this.config = config;
    this.connectionState = 'new';
    this.localDescription = null;
    this.remoteDescription = null;
    this.onicecandidate = null;
    this.onconnectionstatechange = null;
    this.ondatachannel = null;
    this._channels = [];
    this._remote = null;
    this._candidates = [];
  }

  linkTo(other) {
    this._remote = other;
    other._remote = this;
  }

  createDataChannel(label, options) {
    const channel = new MockDataChannel(label, options);
    this._channels.push(channel);
    queueMicrotask(() => {
      if (this._remote?.ondatachannel) {
        this._remote.ondatachannel({ channel, type: 'datachannel', target: this._remote });
      }
    });
    return channel;
  }

  async createOffer() {
    return { type: 'offer', sdp: 'mock-offer' };
  }

  async createAnswer() {
    return { type: 'answer', sdp: 'mock-answer' };
  }

  async setLocalDescription(desc) {
    this.localDescription = desc;
    this._emitIce();
  }

  async setRemoteDescription(desc) {
    this.remoteDescription = desc;
    if (this._remote) {
      for (const ch of this._remote._channels) {
        if (this.ondatachannel) this.ondatachannel({ channel: ch, type: 'datachannel', target: this });
      }
    }
  }

  async addIceCandidate(candidate) {
    this._candidates.push(candidate);
  }

  _emitIce() {
    this.onicecandidate?.({
      candidate: { candidate: 'candidate:mock 1 udp 2130706431 127.0.0.1 9 typ host' },
      type: 'icecandidate',
      target: this,
    });
    this.onicecandidate?.({ candidate: null, type: 'icecandidate', target: this });
  }

  openAllChannels() {
    for (const ch of this._channels) ch.open();
    for (const ch of this._remote?._channels ?? []) ch.open();
    this.connectionState = 'connected';
    this.onconnectionstatechange?.({ type: 'connectionstatechange', target: this });
    if (this._remote) {
      this._remote.connectionState = 'connected';
      this._remote.onconnectionstatechange?.({ type: 'connectionstatechange', target: this._remote });
    }
  }

  async getStats() {
    const report = new Map();
    report.set('pair', {
      type: 'candidate-pair',
      state: 'succeeded',
      currentRoundTripTime: 0.042,
    });
    return report;
  }

  close() {
    this.connectionState = 'closed';
    for (const ch of this._channels) ch.close();
  }
}

MockPeerConnection.generateCertificate = async () => ({});

// ---- mock / real signaling helpers -----------------------------------------

class MockWebSocket {
  static OPEN = 1;
  static CONNECTING = 0;
  static CLOSED = 3;

  constructor(url) {
    this.url = url;
    this.readyState = MockWebSocket.OPEN;
    this.sent = [];
    this.onopen = null;
    this.onclose = null;
    this.onerror = null;
    this.onmessage = null;
    queueMicrotask(() => this.onopen?.({ type: 'open', target: this }));
  }

  send(text) {
    this.sent.push(JSON.parse(text));
  }

  receive(msg) {
    this.onmessage?.({ data: JSON.stringify(msg), type: 'message', target: this });
  }

  close() {
    this.readyState = MockWebSocket.CLOSED;
    this.onclose?.({ type: 'close', target: this });
  }
}

function makeWebSocketFactory() {
  const sockets = [];
  return {
    sockets,
    WebSocket: class extends MockWebSocket {
      constructor(url) {
        super(url);
        sockets.push(this);
      }
    },
  };
}

function collectEvents(onEvent) {
  const events = [];
  const handler = (type, peer, channel, cause, bytes) => {
    events.push({
      type,
      peer,
      channel,
      cause,
      bytes: bytes ? Array.from(bytes) : null,
    });
    onEvent?.(type, peer, channel, cause, bytes);
  };
  return { events, handler };
}

async function startSignalingServer() {
  const { createSignalingServer } = await import('../../../tools/webrtc-signaling/server.js');
  const ctx = createSignalingServer();
  await new Promise((resolve, reject) => {
    ctx.httpServer.once('error', reject);
    ctx.httpServer.listen(0, '127.0.0.1', resolve);
  });
  const port = ctx.httpServer.address().port;
  return { ...ctx, url: `ws://127.0.0.1:${port}/` };
}

function makeRealWebSocketClass(urlPrefix) {
  return class NodeWebSocket {
    static OPEN = WebSocket.OPEN;
    static CONNECTING = WebSocket.CONNECTING;
    static CLOSED = WebSocket.CLOSED;

    constructor(path) {
      this._ws = new WebSocket(`${urlPrefix}${path ?? ''}`);
      this.readyState = WebSocket.CONNECTING;
      this._ws.on('open', () => {
        this.readyState = WebSocket.OPEN;
        this.onopen?.({ type: 'open', target: this });
      });
      this._ws.on('message', (raw) => {
        this.onmessage?.({ data: raw.toString('utf8'), type: 'message', target: this });
      });
      this._ws.on('close', () => {
        this.readyState = WebSocket.CLOSED;
        this.onclose?.({ type: 'close', target: this });
      });
      this._ws.on('error', () => {
        this.onerror?.({ type: 'error', target: this });
      });
    }

    send(text) {
      this._ws.send(text);
    }

    close() {
      this._ws.close();
    }
  };
}

async function waitFor(predicate, timeoutMs = 3000, label = 'condition') {
  const start = Date.now();
  while (Date.now() - start < timeoutMs) {
    const value = predicate();
    if (value) return value;
    await sleep(10);
  }
  throw new Error(`timeout waiting for ${label}`);
}

async function connectMockPair(roomCode = 'PAIR') {
  const wsFactory = makeWebSocketFactory();
  const pcs = [];
  const RTCPeerConnection = class extends MockPeerConnection {
    constructor(...args) {
      super(...args);
      pcs.push(this);
      if (pcs.length === 2) pcs[0].linkTo(pcs[1]);
    }
  };

  const { events, handler } = collectEvents();
  const host = createDuneCityWebRtc({ RTCPeerConnection, WebSocket: wsFactory.WebSocket, onEvent: handler });
  const client = createDuneCityWebRtc({ RTCPeerConnection, WebSocket: wsFactory.WebSocket, onEvent: handler });

  host.hostRoom();
  const hostWs = wsFactory.sockets[0];
  hostWs.receive({ v: 1, type: 'created', room: roomCode, peerId: 'phost001' });

  client.joinRoom(roomCode);
  const clientWs = wsFactory.sockets[1];
  clientWs.receive({ v: 1, type: 'joined', room: roomCode, peerId: 'pclient01', host: 'phost001' });
  hostWs.receive({ v: 1, type: 'peer-joined', peerId: 'pclient01' });

  await waitFor(() => pcs.length === 1 && hostWs.sent.some((m) => m.type === 'signal' && m.data?.kind === 'offer'));
  const hostPc = pcs[0];
  const offerMsg = hostWs.sent.find((m) => m.type === 'signal' && m.data?.kind === 'offer');
  clientWs.receive({ v: 1, type: 'signal', from: 'phost001', data: offerMsg.data });

  await waitFor(() => pcs.length === 2 && clientWs.sent.some((m) => m.type === 'signal' && m.data?.kind === 'answer'));
  const clientPc = pcs[1];
  const answerMsg = clientWs.sent.find((m) => m.type === 'signal' && m.data?.kind === 'answer');
  hostWs.receive({ v: 1, type: 'signal', from: 'pclient01', data: answerMsg.data });
  hostPc.openAllChannels();
  await waitFor(() => host.getStats().channels[0].state === 'open');

  return { host, client, hostPc, clientPc, hostWs, clientWs, events, handler };
}

// ---- tests -----------------------------------------------------------------

test('createDuneCityWebRtc requires injected dependencies', () => {
  assert.throws(() => createDuneCityWebRtc(null), /RTCPeerConnection is required/);
  assert.throws(() => createDuneCityWebRtc({ RTCPeerConnection: MockPeerConnection }), /WebSocket is required/);
  assert.throws(
    () => createDuneCityWebRtc({ RTCPeerConnection: MockPeerConnection, WebSocket: MockWebSocket }),
    /onEvent is required/,
  );
});

test('hostRoom sends create after signaling opens and reports room code', async () => {
  const wsFactory = makeWebSocketFactory();
  const { events, handler } = collectEvents();
  const rtc = createDuneCityWebRtc({
    RTCPeerConnection: MockPeerConnection,
    WebSocket: wsFactory.WebSocket,
    config: { signaling: 'ws://mock/' },
    onEvent: handler,
  });

  assert.equal(rtc.hostRoom(), true);
  assert.equal(rtc.hostRoom(), false, 'second hostRoom is rejected');

  await waitFor(() => wsFactory.sockets.length === 1 && wsFactory.sockets[0].sent.length === 1);
  const ws = wsFactory.sockets[0];
  assert.deepEqual(ws.sent[0], { v: 1, type: 'create' });

  ws.receive({ v: 1, type: 'created', room: 'ABCD', peerId: 'phost001' });
  assert.equal(rtc.getRoomCode(), 'ABCD');
  assert.equal(rtc.getRole(), 'host');
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_CONNECTING));
});

test('joinRoom validates room code alphabet and length', () => {
  const wsFactory = makeWebSocketFactory();
  const { events, handler } = collectEvents();
  const rtc = createDuneCityWebRtc({
    RTCPeerConnection: MockPeerConnection,
    WebSocket: wsFactory.WebSocket,
    onEvent: handler,
  });

  assert.equal(rtc.joinRoom('ab12'), false);
  assert.equal(rtc.joinRoom('AB0D'), false);
  assert.equal(rtc.joinRoom('ABCDE'), false);
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_FAILED));
});

test('peer-joined creates offer with expected data-channel options', async () => {
  const wsFactory = makeWebSocketFactory();
  const pcs = [];
  const RTCPeerConnection = class extends MockPeerConnection {
    constructor(...args) {
      super(...args);
      pcs.push(this);
    }
  };

  const { handler } = collectEvents();
  const host = createDuneCityWebRtc({
    RTCPeerConnection,
    WebSocket: wsFactory.WebSocket,
    onEvent: handler,
  });

  host.hostRoom();
  const ws = wsFactory.sockets[0];
  ws.receive({ v: 1, type: 'created', room: 'WXYZ', peerId: 'phost001' });
  ws.receive({ v: 1, type: 'peer-joined', peerId: 'pclient01' });

  await waitFor(() => pcs.length === 1 && ws.sent.some((m) => m.type === 'signal' && m.data?.kind === 'offer'));
  const pc = pcs[0];
  assert.equal(pc._channels.length, 2);
  assert.deepEqual(pc._channels[0].options, DUNECITY_WEBRTC_CONTROL_OPTIONS);
  assert.deepEqual(pc._channels[1].options, DUNECITY_WEBRTC_COMMANDS_OPTIONS);
});

test('both channels open emits CONNECT and CONNECTED state', async () => {
  const { events } = await connectMockPair('TRQZ');
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_CONNECT));
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_CONNECTED));
});

test('send delivers binary payloads on the control channel', async () => {
  const { host, hostPc, events } = await connectMockPair('MNOP');
  const payload = new Uint8Array([0x01, 0x00, 0x00, 0x00, 0x42]);
  assert.equal(host.send(0, payload), true);

  hostPc._channels[0].deliver(payload);
  await waitFor(() => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0));
  const msg = events.find((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0);
  assert.deepEqual(msg.bytes, Array.from(payload));
});

test('control channel backpressure queues and flushes on bufferedamountlow', async () => {
  const { host, hostPc } = await connectMockPair('BKPR');
  const control = hostPc._channels[0];
  control.setBufferedAmount(DUNECITY_WEBRTC_CONTROL_HIGH_WATER);

  const bytes = new Uint8Array([9, 9, 9, 9]);
  assert.equal(host.send(0, bytes), true);
  assert.equal(host.getStats().channels[0].queued, 1);
  assert.equal(control.sent.length, 0);

  control.setBufferedAmount(DUNECITY_WEBRTC_CONTROL_HIGH_WATER - 1);
  host._flushControlOutboxForTest();
  assert.equal(control.sent.length, 1);
  assert.deepEqual(Array.from(control.sent[0]), Array.from(bytes));
});

test('commands channel drops when bufferedAmount is at high water', async () => {
  const { host, hostPc } = await connectMockPair('DROP');
  hostPc._channels[1].bufferedAmount = DUNECITY_WEBRTC_COMMANDS_HIGH_WATER;

  const payload = new Uint8Array([1, 2, 3, 4]);
  assert.equal(host.send(1, payload), false);
  assert.equal(host.getStats().channels[1].dropped, 1);
});

test('peer-left emits DISCONNECT after connect', async () => {
  const { hostWs, events } = await connectMockPair('LEFT');
  hostWs.receive({ v: 1, type: 'peer-left', peerId: 'pclient01' });
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_DISCONNECT));
});

test('integration: host and client connect through real signaling server', async () => {
  const server = await startSignalingServer();
  const NodeWebSocket = makeRealWebSocketClass(server.url);
  const pcs = [];

  const RTCPeerConnection = class extends MockPeerConnection {
    constructor(...args) {
      super(...args);
      pcs.push(this);
      if (pcs.length === 2) pcs[0].linkTo(pcs[1]);
    }
  };

  const { events, handler } = collectEvents();
  const host = createDuneCityWebRtc({
    RTCPeerConnection,
    WebSocket: NodeWebSocket,
    config: { signaling: server.url },
    onEvent: handler,
  });
  const client = createDuneCityWebRtc({
    RTCPeerConnection,
    WebSocket: NodeWebSocket,
    config: { signaling: server.url },
    onEvent: handler,
  });

  try {
    assert.equal(host.hostRoom(), true);
    await waitFor(() => host.getRoomCode(), 3000, 'room code');
    const room = host.getRoomCode();
    assert.match(room, /^[A-Z2-9]{4}$/);

    assert.equal(client.joinRoom(room), true);
    await waitFor(() => pcs.length === 2, 5000, 'two peer connections');

    const hostPc = pcs[0];
    const clientPc = pcs[1];

    await waitFor(() => hostPc.localDescription && clientPc.localDescription, 5000, 'local descriptions');
    hostPc.openAllChannels();

    await waitFor(
      () => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_CONNECT),
      5000,
      'CONNECT event',
    );

    const ping = new Uint8Array([0x04, 0x00, 0x00, 0x00, 0x7]);
    assert.equal(host.send(0, ping), true);
    hostPc._channels[0].deliver(ping);
    await waitFor(() => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE), 3000, 'MESSAGE event');
  } finally {
    host.disconnect();
    client.disconnect();
    server.close();
  }
});

test('module exports include channel option constants', () => {
  assert.equal(DUNECITY_WEBRTC_CONTROL_OPTIONS.ordered, true);
  assert.equal(DUNECITY_WEBRTC_COMMANDS_OPTIONS.ordered, false);
  assert.equal(DUNECITY_WEBRTC_COMMANDS_OPTIONS.maxRetransmits, 0);
  assert.equal(DUNECITY_WEBRTC_CONTROL_HIGH_WATER, 512 * 1024);
  assert.equal(DUNECITY_WEBRTC_COMMANDS_HIGH_WATER, 512 * 1024);
});
