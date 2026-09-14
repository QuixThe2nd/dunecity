// Unit and integration tests for platform/web/webrtc_glue.js (createDuneCityWebRtc).
// Mocks RTCPeerConnection / RTCDataChannel / WebSocket so the DI core can run under Node.

'use strict';

require('./ensure-p2pkit-bundle.cjs');

const { test } = require('node:test');
const assert = require('node:assert/strict');
const WebSocket = require('ws');
const {
  createDuneCityWebRtc,
  resolveP2pkit,
  _createSignallingChannelForTest,
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

const p2pkit = resolveP2pkit({});

const DUNECITY_WEBRTC_CONTROL_LOW_WATER = 128 * 1024;

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
    this._peer = null;
    this.onopen = null;
    this.onclose = null;
    this.onerror = null;
    this.onmessage = null;
    this.onbufferedamountlow = null;
  }

  linkPeer(other) {
    this._peer = other;
    other._peer = this;
  }

  send(data) {
    const bytes = data instanceof Uint8Array ? data : new Uint8Array(data);
    this.sent.push(bytes);
    this.bufferedAmount += bytes.length;
    if (this._peer?.onmessage) {
      const payload = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
      this._peer.onmessage({ data: payload, type: 'message', target: this._peer });
    }
  }

  open() {
    if (this.readyState === 'open') return;
    this.readyState = 'open';
    this.onopen?.({ type: 'open', target: this });
    if (this._peer && this._peer.readyState !== 'open') {
      this._peer.open();
    }
  }

  close() {
    if (this.readyState === 'closed') return;
    this.readyState = 'closed';
    this.onclose?.({ type: 'close', target: this });
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
    this._ondatachannelHandler = null;
    this._channels = [];
    this._outboundRemoteChannels = [];
    this._incomingChannels = [];
    this._remote = null;
    this._candidates = [];
  }

  get ondatachannel() {
    return this._ondatachannelHandler;
  }

  set ondatachannel(handler) {
    this._ondatachannelHandler = handler;
    this._scheduleDeliverIncoming();
  }

  linkTo(other) {
    this._remote = other;
    other._remote = this;
    for (const remote of this._outboundRemoteChannels) {
      other._enqueueIncomingChannel(remote);
    }
    for (const remote of other._outboundRemoteChannels) {
      this._enqueueIncomingChannel(remote);
    }
  }

  _enqueueIncomingChannel(channel) {
    this._incomingChannels.push(channel);
    this._scheduleDeliverIncoming();
  }

  _scheduleDeliverIncoming() {
    queueMicrotask(() => {
      if (!this._ondatachannelHandler || this._incomingChannels.length === 0) return;
      while (this._incomingChannels.length > 0) {
        const channel = this._incomingChannels.shift();
        this._ondatachannelHandler({ channel, type: 'datachannel', target: this });
      }
    });
  }

  createDataChannel(label, options) {
    const local = new MockDataChannel(label, options);
    const remote = new MockDataChannel(label, options);
    local.linkPeer(remote);
    this._channels.push(local);
    this._outboundRemoteChannels.push(remote);
    if (this._remote) {
      this._remote._enqueueIncomingChannel(remote);
    }
    return local;
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
  }

  async addIceCandidate(candidate) {
    this._candidates.push(candidate);
  }

  _emitIce() {
    const candidateObj = {
      candidate: 'candidate:mock 1 udp 2130706431 127.0.0.1 9 typ host',
      sdpMid: '0',
      sdpMLineIndex: 0,
      toJSON() {
        return { candidate: this.candidate, sdpMid: this.sdpMid, sdpMLineIndex: this.sdpMLineIndex };
      },
    };
    this.onicecandidate?.({
      candidate: candidateObj,
      type: 'icecandidate',
      target: this,
    });
    this.onicecandidate?.({ candidate: null, type: 'icecandidate', target: this });
  }

  openAllChannels() {
    for (const ch of this._channels) ch.open();
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

function isDescriptionOffer(msg) {
  return msg && msg.description && msg.description.type === 'offer';
}

function isDescriptionAnswer(msg) {
  return msg && msg.description && msg.description.type === 'answer';
}

async function connectMockPair(roomCode = 'PAIR', hostPeerId = 'phost001', clientPeerId = 'pclient01') {
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
  const host = createDuneCityWebRtc({ RTCPeerConnection, WebSocket: wsFactory.WebSocket, p2pkit, onEvent: handler });
  const client = createDuneCityWebRtc({ RTCPeerConnection, WebSocket: wsFactory.WebSocket, p2pkit, onEvent: handler });

  host.hostRoom();
  const hostWs = wsFactory.sockets[0];
  hostWs.receive({ v: 1, type: 'created', room: roomCode, peerId: hostPeerId });

  client.joinRoom(roomCode);
  const clientWs = wsFactory.sockets[1];
  clientWs.receive({ v: 1, type: 'joined', room: roomCode, peerId: clientPeerId, host: hostPeerId });
  hostWs.receive({ v: 1, type: 'peer-joined', peerId: clientPeerId });

  const clientInitiates = p2pkit.isInitiator(clientPeerId, hostPeerId);
  const offerWs = clientInitiates ? clientWs : hostWs;
  const answerWs = clientInitiates ? hostWs : clientWs;

  await waitFor(() => pcs.length === 1 && offerWs.sent.some(isDescriptionOffer));
  const offerMsg = offerWs.sent.find(isDescriptionOffer);
  answerWs.receive(offerMsg);

  await waitFor(() => pcs.length === 2 && answerWs.sent.some(isDescriptionAnswer));
  const answerMsg = answerWs.sent.find(isDescriptionAnswer);
  offerWs.receive(answerMsg);

  const initiatorPc = pcs[0];
  const responderPc = pcs[1];
  initiatorPc.openAllChannels();
  await waitFor(() => host.getStats().channels[0].state === 'open');

  const hostPc = clientInitiates ? responderPc : initiatorPc;
  const clientPc = clientInitiates ? initiatorPc : responderPc;

  return { host, client, hostPc, clientPc, hostWs, clientWs, events, handler, clientInitiates };
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

test('peer-joined triggers offer with expected data-channel options on initiator', async () => {
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
    p2pkit,
    onEvent: handler,
  });

  host.hostRoom();
  const ws = wsFactory.sockets[0];
  ws.receive({ v: 1, type: 'created', room: 'WXYZ', peerId: 'phost001' });
  // phost001 < pzzzzzz1, so host is initiator and creates the data channels.
  ws.receive({ v: 1, type: 'peer-joined', peerId: 'pzzzzzz1' });

  await waitFor(() => pcs.length === 1 && ws.sent.some(isDescriptionOffer));
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
  const { host, hostPc, events } = await connectMockPair('MNOP', 'phost001', 'pzzzzzz1');
  const payload = new Uint8Array([0x01, 0x00, 0x00, 0x00, 0x42]);
  assert.equal(host.send(0, payload), true);

  await waitFor(() => hostPc._channels[0].sent.length === 1);
  assert.deepEqual(Array.from(hostPc._channels[0].sent[0]), Array.from(payload));

  await waitFor(() => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0));
  const msg = events.find((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0);
  assert.deepEqual(msg.bytes, Array.from(payload));
});

test('control channel backpressure queues and flushes on bufferedamountlow', async () => {
  const { host, hostPc } = await connectMockPair('BKPR', 'phost001', 'pzzzzzz1');
  const control = hostPc._channels[0];
  control.setBufferedAmount(DUNECITY_WEBRTC_CONTROL_HIGH_WATER);

  const bytes = new Uint8Array([9, 9, 9, 9]);
  assert.equal(host.send(0, bytes), true);
  assert.equal(host.getStats().channels[0].queued, 1);
  assert.equal(control.sent.length, 0);

  control.setBufferedAmount(DUNECITY_WEBRTC_CONTROL_LOW_WATER);
  await waitFor(() => control.sent.length === 1, 3000, 'queued control flush');
  assert.deepEqual(Array.from(control.sent[0]), Array.from(bytes));
});

test('commands channel drops when bufferedAmount is at high water', async () => {
  const { host, hostPc } = await connectMockPair('DROP', 'phost001', 'pzzzzzz1');
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
    p2pkit,
    onEvent: handler,
  });
  const client = createDuneCityWebRtc({
    RTCPeerConnection,
    WebSocket: NodeWebSocket,
    config: { signaling: server.url },
    p2pkit,
    onEvent: handler,
  });

  try {
    assert.equal(host.hostRoom(), true);
    await waitFor(() => host.getRoomCode(), 3000, 'room code');
    const room = host.getRoomCode();
    assert.match(room, /^[A-Z2-9]{4}$/);

    assert.equal(client.joinRoom(room), true);
    await waitFor(() => pcs.length === 2, 5000, 'two peer connections');

    await waitFor(() => pcs.some((pc) => pc._channels.length === 2), 5000, 'initiator data channels');
    const initiatorPc = pcs.find((pc) => pc._channels.length === 2);
    await waitFor(() => pcs.every((pc) => pc.localDescription), 5000, 'local descriptions');
    initiatorPc.openAllChannels();

    await waitFor(
      () => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_CONNECT),
      5000,
      'CONNECT event',
    );

    const ping = new Uint8Array([0x04, 0x00, 0x00, 0x00, 0x7]);
    assert.equal(host.send(0, ping), true);

    await waitFor(
      () => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0),
      3000,
      'MESSAGE event',
    );
    const msg = events.find((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0);
    assert.deepEqual(msg.bytes, Array.from(ping));
    assert.equal(host.getStats().channels[0].sent, 1);
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

test('signalling adapter send() emits p2pkit dialect with from/to and rejects malformed messages', () => {
  const sent = [];
  const channel = _createSignallingChannelForTest({
    sendRaw: (text) => {
      sent.push(JSON.parse(text));
      return true;
    },
    p2pkit,
    log: () => {},
  });

  assert.equal(
    channel.send({ description: { type: 'offer', sdp: 'x' }, from: 'pa', to: 'pb' }),
    true,
  );
  assert.equal(sent.length, 1);
  assert.equal(sent[0].from, 'pa');
  assert.equal(sent[0].to, 'pb');
  assert.deepEqual(sent[0].description, { type: 'offer', sdp: 'x' });

  assert.equal(channel.send({ description: { type: 'offer', sdp: 'x' }, from: 'pa' }), false);
});

test('p2pkit dialect envelopes produce the expected webrtcOnEvent sequence', async () => {
  const { host, client, hostWs, clientWs, events, clientInitiates } = await connectMockPair('EVNT');

  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_CONNECTING));
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_CONNECT));
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_CONNECTED));

  const payload = new Uint8Array([0x02, 0x00, 0x00, 0x00, 0x01]);
  assert.equal(host.send(0, payload), true);
  await waitFor(() => events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_MESSAGE && e.channel === 0));

  hostWs.receive({ v: 1, type: 'peer-left', peerId: 'pclient01' });
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_DISCONNECT));
  assert.ok(events.some((e) => e.type === DUNECITY_WEBRTC_EVENT_STATE && e.cause === DUNECITY_WEBRTC_STATE_FAILED));

  host.disconnect();
  client.disconnect();
});

test('stock RTCTransport negotiates over the signalling adapter wire format', async () => {
  assert.ok(p2pkit && p2pkit.RTCTransport, 'p2pkit bundle must export RTCTransport');

  const hostSent = [];
  const clientSent = [];
  const hostAdapter = _createSignallingChannelForTest({
    sendRaw: (text) => {
      hostSent.push(JSON.parse(text));
      return true;
    },
    p2pkit,
    log: () => {},
  });
  const clientAdapter = _createSignallingChannelForTest({
    sendRaw: (text) => {
      clientSent.push(JSON.parse(text));
      return true;
    },
    p2pkit,
    log: () => {},
  });
  hostAdapter._setReady();
  clientAdapter._setReady();

  hostAdapter.onMessage((msg) => {
    if (msg.to === 'pb') clientAdapter._deliver(msg);
  });
  clientAdapter.onMessage((msg) => {
    if (msg.to === 'pa') hostAdapter._deliver(msg);
  });

  const pcs = [];
  const RTCPeerConnection = class extends MockPeerConnection {
    constructor(...args) {
      super(...args);
      pcs.push(this);
      if (pcs.length === 2) pcs[0].linkTo(pcs[1]);
    }
  };

  const hostTransport = new p2pkit.RTCTransport({
    self: 'pa',
    remote: 'pb',
    signalling: hostAdapter,
    backend: { RTCPeerConnection },
    initiator: p2pkit.isInitiator('pa', 'pb'),
  });
  const clientTransport = new p2pkit.RTCTransport({
    self: 'pb',
    remote: 'pa',
    signalling: clientAdapter,
    backend: { RTCPeerConnection },
    initiator: p2pkit.isInitiator('pb', 'pa'),
  });

  await waitFor(() => hostSent.some(isDescriptionOffer) || clientSent.some(isDescriptionOffer), 3000, 'offer envelope');
  const offerWire = hostSent.find(isDescriptionOffer) || clientSent.find(isDescriptionOffer);
  assert.equal(typeof offerWire.from, 'string');
  assert.equal(typeof offerWire.to, 'string');
  assert.equal(offerWire.description.type, 'offer');

  await waitFor(() => pcs.length === 2, 3000, 'peer connections');
  const initiatorPc = pcs.find((pc) => pc._channels.length > 0) || pcs[0];
  initiatorPc.openAllChannels();

  hostTransport.disconnect();
  clientTransport.disconnect();
});
