/*
 * DuneCity WebRTC bridge (browser side).
 *
 * This file has two halves:
 *   1. A dependency-injected core (`createDuneCityWebRtc`) that owns all the
 *      WebRTC/signaling logic. Every browser API it needs is injected, so the
 *      core can be unit-tested under Node with mocked RTCPeerConnection /
 *      WebSocket (see test/webrtc-glue.test.js).
 *   2. An Emscripten `--js-library` wrapper that wires the core to the real
 *      browser APIs and to the C++ side (WebRtcTransport.cpp) through the
 *      exported `_webrtcOnEvent` callback and `_malloc`/`_free`.
 *
 * p2pkit integration: negotiation helpers and the SignallingChannel dialect
 * come from the committed bundle at platform/web/p2pkit/dist/p2pkit.iife.js
 * (globalThis.P2PKIT_IIFE). The deployment page must load that bundle as a
 * classic script before dunecity.js. Game packets still ride two native binary
 * RTCDataChannels created here — never p2pkit's RTCTransport JSON channel.
 *
 * Wire contract (see docs/webrtc/IMPLEMENTATION-PLAN.md):
 *   - channel 0 ("control")  : RTCDataChannel { ordered: true }            — ENet channel 0 reliable
 *   - channel 1 ("commands") : RTCDataChannel { ordered: false, maxRetransmits: 0 } — ENet channel 1 unsequenced
 *   - one application packet per DataChannel message; payload untouched.
 *
 * Backpressure:
 *   - control  : if bufferedAmount >= high water mark, outgoing messages are
 *                queued in JS and flushed on `bufferedamountlow`.
 *   - commands : if bufferedAmount >= high water mark, the send is DROPPED and
 *                reported as a failure; the game's CommandManager resends the
 *                recent command cycles, matching ENet's lossy unsequenced channel.
 */

'use strict';

// Every DUNECITY_WEBRTC_* constant is declared twice on purpose:
//   - here, as top-level const, so the Node unit tests (and module.exports) see
//     the real values;
//   - again below in the Emscripten mergeInto() block as `$NAME: '=...'`
//     verbatim-string library items, because Emscripten only emits library
//     object members into dunecity.js — these top-level declarations never
//     reach the browser.
// platform/web/test/emscripten-webrtc-library.test.cjs fails if the two halves
// drift apart or if a constant used by retained runtime code is missing.

const DUNECITY_WEBRTC_CONTROL_LABEL = 'control';
const DUNECITY_WEBRTC_COMMANDS_LABEL = 'commands';
const DUNECITY_WEBRTC_CONTROL_OPTIONS = { ordered: true };
const DUNECITY_WEBRTC_COMMANDS_OPTIONS = { ordered: false, maxRetransmits: 0 };
const DUNECITY_WEBRTC_CONTROL_HIGH_WATER = 512 * 1024;
const DUNECITY_WEBRTC_CONTROL_LOW_WATER = 128 * 1024;
const DUNECITY_WEBRTC_COMMANDS_HIGH_WATER = 512 * 1024;
const DUNECITY_WEBRTC_MAX_SIGNAL_BYTES = 256 * 1024;
const DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION = 1;

// Event codes passed to the C++ side (must match WebRtcTransport.h)
const DUNECITY_WEBRTC_EVENT_CONNECT = 0;
const DUNECITY_WEBRTC_EVENT_DISCONNECT = 1;
const DUNECITY_WEBRTC_EVENT_MESSAGE = 2;
const DUNECITY_WEBRTC_EVENT_STATE = 3;

// Transport states (must match WebRtcTransport.h)
const DUNECITY_WEBRTC_STATE_IDLE = 0;
const DUNECITY_WEBRTC_STATE_CONNECTING = 1;
const DUNECITY_WEBRTC_STATE_CONNECTED = 2;
const DUNECITY_WEBRTC_STATE_FAILED = 3;

let cachedP2pkit = null;

function resolveP2pkit(deps) {
    if (cachedP2pkit) return cachedP2pkit;
    if (deps && deps.p2pkit) {
        cachedP2pkit = deps.p2pkit;
        return cachedP2pkit;
    }
    if (typeof globalThis !== 'undefined' && globalThis.P2PKIT_IIFE) {
        cachedP2pkit = globalThis.P2PKIT_IIFE;
        return cachedP2pkit;
    }
    if (typeof require === 'function' && typeof __dirname === 'string') {
        const fs = require('fs');
        const path = require('path');
        const vm = require('vm');
        const src = fs.readFileSync(path.join(__dirname, 'p2pkit', 'dist', 'p2pkit.iife.js'), 'utf8');
        vm.runInThisContext(src, { filename: 'p2pkit.iife.js' });
        if (globalThis.P2PKIT_IIFE) {
            cachedP2pkit = globalThis.P2PKIT_IIFE;
            return cachedP2pkit;
        }
    }
    return null;
}

function isP2pkitDialectMessage(msg) {
    if (!msg || typeof msg !== 'object' || Array.isArray(msg)) return false;
    if (Object.prototype.hasOwnProperty.call(msg, 'v')) return false;
    if (Object.prototype.hasOwnProperty.call(msg, 'type')) return false;
    return msg.announce === true || msg.description !== undefined || msg.iceCandidate !== undefined;
}

function validateSignallingMessage(message) {
    if (!message || typeof message !== 'object' || Array.isArray(message)) return 'message must be an object';
    if (typeof message.from !== 'string' || message.from.length === 0) return 'missing from';
    if (message.announce === true) return null;
    if (message.description !== undefined) {
        if (typeof message.to !== 'string' || message.to.length === 0) return 'description requires to';
        if (typeof message.description !== 'object' || message.description === null || Array.isArray(message.description)) {
            return 'description must be an object';
        }
        return null;
    }
    if (message.iceCandidate !== undefined) {
        if (typeof message.to !== 'string' || message.to.length === 0) return 'iceCandidate requires to';
        if (typeof message.iceCandidate !== 'object' || message.iceCandidate === null || Array.isArray(message.iceCandidate)) {
            return 'iceCandidate must be an object';
        }
        return null;
    }
    return 'unknown signalling message shape';
}

function createDuneCitySignallingChannel({ sendRaw, p2pkit, log }) {
    let readyResolve;
    let readyDone = false;
    const ready = new Promise(function (resolve) {
        readyResolve = resolve;
    });
    const emitter = new p2pkit.Emitter();

    function send(message) {
        const err = validateSignallingMessage(message);
        if (err) {
            log('webrtc: invalid signalling send: ' + err);
            return false;
        }
        const text = JSON.stringify(message);
        if (text.length > DUNECITY_WEBRTC_MAX_SIGNAL_BYTES) {
            log('webrtc: signal message too large');
            return false;
        }
        return sendRaw(text);
    }

    return {
        send: send,
        onMessage: function (handler) {
            return emitter.on('message', handler);
        },
        ready: ready,
        _deliver: function (msg) {
            emitter.emit('message', msg);
        },
        _setReady: function () {
            if (readyDone) return;
            readyDone = true;
            readyResolve();
        },
    };
}

function createDuneCityWebRtc(deps) {
    if (!deps || !deps.RTCPeerConnection) throw new Error('deps.RTCPeerConnection is required');
    if (!deps.WebSocket) throw new Error('deps.WebSocket is required');
    if (typeof deps.onEvent !== 'function') throw new Error('deps.onEvent is required');

    const log = deps.log || function () {};
    const now = deps.now || function () { return Date.now(); };
    const config = deps.config || {};

    // ---- passive telemetry (diagnostics only; no behavior depends on it) ----
    const stats = {
        role: null,               // 'host' | 'client'
        roomCode: null,
        signalingState: 'idle',   // idle|connecting|open|closed|error
        peerConnectionState: 'new',
        channels: {
            0: { label: DUNECITY_WEBRTC_CONTROL_LABEL, state: 'new', sent: 0, received: 0, dropped: 0, queued: 0,
                 lastPacketId: -1, lastPacketLen: 0 },
            1: { label: DUNECITY_WEBRTC_COMMANDS_LABEL, state: 'new', sent: 0, received: 0, dropped: 0, queued: 0,
                 lastPacketId: -1, lastPacketLen: 0 },
        },
        messages: [],             // capped ring of {dir, channel, packetId, len, t}
    };

    function recordMessage(dir, channel, bytes) {
        const ch = stats.channels[channel];
        if (!ch) return;
        // first 4 bytes LE = application packet type (see NetworkManager wire format)
        let packetId = -1;
        if (bytes && bytes.length >= 4) {
            packetId = (bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24)) >>> 0;
        }
        ch.lastPacketId = packetId;
        ch.lastPacketLen = bytes ? bytes.length : 0;
        stats.messages.push({ dir: dir, channel: channel, packetId: packetId, len: bytes ? bytes.length : 0, t: now() });
        if (stats.messages.length > 512) stats.messages.splice(0, stats.messages.length - 512);
    }

    // ---- signaling ----
    let ws = null;
    let selfPeerId = null;
    let remotePeerId = null;
    let peerHandle = 0;         // stable C++-facing peer id (assigned on connect)
    let signallingChannel = null;
    let p2pkit = null;

    function getP2pkit() {
        if (!p2pkit) p2pkit = resolveP2pkit(deps);
        return p2pkit;
    }

    function signalSend(obj) {
        if (!ws || ws.readyState !== deps.WebSocket.OPEN) {
            log('webrtc: cannot signal, socket not open');
            return false;
        }
        const text = JSON.stringify(obj);
        if (text.length > DUNECITY_WEBRTC_MAX_SIGNAL_BYTES) {
            log('webrtc: signal message too large');
            return false;
        }
        ws.send(text);
        return true;
    }

    function ensureSignallingChannel() {
        if (signallingChannel) return signallingChannel;
        const kit = getP2pkit();
        if (!kit) return null;
        signallingChannel = createDuneCitySignallingChannel({
            sendRaw: function (text) {
                if (!ws || ws.readyState !== deps.WebSocket.OPEN) {
                    log('webrtc: cannot signal, socket not open');
                    return false;
                }
                ws.send(text);
                return true;
            },
            p2pkit: kit,
            log: log,
        });
        signallingChannel.onMessage(function (msg) {
            handleDialectMessage(msg).catch(function (e) { fail('dialect: ' + e); });
        });
        return signallingChannel;
    }

    // ---- peer connection ----
    let pc = null;
    let channels = { 0: null, 1: null };       // RTCDataChannel per game channel
    let controlOutbox = [];                     // queued control messages (backpressure)
    let controlPaused = false;
    let bothChannelsOpen = false;

    function setState(next) {
        if (stats.peerConnectionState === next) return;
        stats.peerConnectionState = next;
        if (deps.onStateChange) deps.onStateChange(next);
    }

    function fail(reason) {
        log('webrtc: failed: ' + reason);
        setState('failed');
        if (deps.onEvent) deps.onEvent(DUNECITY_WEBRTC_EVENT_STATE, 0, 0, DUNECITY_WEBRTC_STATE_FAILED, null);
        closeEverything();
    }

    function makePeerConnection() {
        const kit = getP2pkit();
        const iceServers = config.iceServers || (kit && kit.DEFAULT_ICE_SERVERS) || [];
        const pcConfig = { iceServers: iceServers };
        const p = new deps.RTCPeerConnection(pcConfig);
        p.onicecandidate = function (evt) {
            if (evt.candidate && signallingChannel && selfPeerId && remotePeerId) {
                signallingChannel.send({
                    iceCandidate: evt.candidate.toJSON ? evt.candidate.toJSON() : evt.candidate,
                    from: selfPeerId,
                    to: remotePeerId,
                });
            }
        };
        p.onconnectionstatechange = function () {
            setState(p.connectionState);
            if (p.connectionState === 'failed') fail('peer connection failed');
        };
        return p;
    }

    function attachChannel(channel, gameChannel) {
        channels[gameChannel] = channel;
        channel.binaryType = 'arraybuffer';
        stats.channels[gameChannel].state = channel.readyState;

        channel.onopen = function () {
            stats.channels[gameChannel].state = 'open';
            if (channels[0] && channels[0].readyState === 'open' &&
                channels[1] && channels[1].readyState === 'open' && !bothChannelsOpen) {
                bothChannelsOpen = true;
                peerHandle += 1;
                log('webrtc: both data channels open (peer ' + peerHandle + ')');
                startRttPolling();
                if (deps.onStateChange) deps.onStateChange('connected');
                deps.onEvent(DUNECITY_WEBRTC_EVENT_CONNECT, peerHandle, 0, 0, null);
                if (deps.onEvent) {
                    deps.onEvent(DUNECITY_WEBRTC_EVENT_STATE, 0, 0, DUNECITY_WEBRTC_STATE_CONNECTED, null);
                }
            }
        };
        channel.onclose = function () {
            stats.channels[gameChannel].state = 'closed';
            notifyPeerLeft();
        };
        channel.onerror = function (e) {
            log('webrtc: channel ' + gameChannel + ' error');
        };
        channel.onmessage = function (evt) {
            const data = evt.data;
            if (typeof data === 'string') {
                log('webrtc: ignoring unexpected text message on channel ' + gameChannel);
                return;
            }
            const bytes = new Uint8Array(data);
            stats.channels[gameChannel].received += 1;
            recordMessage('recv', gameChannel, bytes);
            deps.onEvent(DUNECITY_WEBRTC_EVENT_MESSAGE, peerHandle, gameChannel, 0, bytes);
        };

        if (gameChannel === 0) {
            channel.bufferedAmountLowThreshold = DUNECITY_WEBRTC_CONTROL_LOW_WATER;
            channel.onbufferedamountlow = function () {
                if (controlPaused) {
                    controlPaused = false;
                    flushControlOutbox();
                }
            };
        }
    }

    function notifyPeerLeft() {
        if (!bothChannelsOpen) return;
        bothChannelsOpen = false;
        stopRttPolling();
        if (deps.onEvent) {
            deps.onEvent(DUNECITY_WEBRTC_EVENT_DISCONNECT, peerHandle, 0, 1 /* NETWORKDISCONNECT_QUIT */, null);
            deps.onEvent(DUNECITY_WEBRTC_EVENT_STATE, 0, 0, DUNECITY_WEBRTC_STATE_FAILED, null);
        }
    }

    // ---- RTT estimation via WebRTC stats (polled; cached for the sync C++ api) ----
    let rttMs = 0;
    let rttTimer = null;
    function startRttPolling() {
        if (rttTimer || !pc || !pc.getStats) return;
        rttTimer = setInterval(function () {
            if (!pc) { stopRttPolling(); return; }
            pc.getStats().then(function (report) {
                let best = 0;
                report.forEach(function (entry) {
                    if (entry.type === 'candidate-pair' && entry.state === 'succeeded' &&
                        typeof entry.currentRoundTripTime === 'number') {
                        const ms = entry.currentRoundTripTime * 1000;
                        if (best === 0 || ms < best) best = ms;
                    }
                });
                if (best > 0) rttMs = Math.round(best);
            }).catch(function () {});
        }, 2000);
        if (typeof rttTimer === 'object' && rttTimer && typeof rttTimer.unref === 'function') rttTimer.unref();
    }
    function stopRttPolling() {
        if (rttTimer) { clearInterval(rttTimer); rttTimer = null; }
        rttMs = 0;
    }

    // ---- offer/answer via p2pkit dialect ------------------------------------
    async function createOfferAndSend() {
        if (!pc) pc = makePeerConnection();
        attachChannel(pc.createDataChannel(DUNECITY_WEBRTC_CONTROL_LABEL, DUNECITY_WEBRTC_CONTROL_OPTIONS), 0);
        attachChannel(pc.createDataChannel(DUNECITY_WEBRTC_COMMANDS_LABEL, DUNECITY_WEBRTC_COMMANDS_OPTIONS), 1);
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        const kit = getP2pkit();
        const sdp = pc.localDescription.sdp;
        if (kit && kit.extractIP) {
            const ip = kit.extractIP(sdp);
            log('webrtc: sending offer' + (ip ? ' (ip=' + ip + ')' : ''));
        }
        if (!signallingChannel || !signallingChannel.send({
            description: { type: 'offer', sdp: sdp },
            from: selfPeerId,
            to: remotePeerId,
        })) {
            fail('offer send failed');
        }
    }

    async function handleDescription(description) {
        if (!pc) pc = makePeerConnection();
        const kit = getP2pkit();
        const sdp = description.sdp || '';
        if (description.type === 'offer') {
            if (kit && kit.extractIP) {
                const ip = kit.extractIP(sdp);
                log('webrtc: received offer' + (ip ? ' (ip=' + ip + ')' : ''));
            }
            await pc.setRemoteDescription(description);
            if (!channels[0]) {
                pc.ondatachannel = function (evt) {
                    const label = evt.channel.label;
                    if (label === DUNECITY_WEBRTC_CONTROL_LABEL) attachChannel(evt.channel, 0);
                    else if (label === DUNECITY_WEBRTC_COMMANDS_LABEL) attachChannel(evt.channel, 1);
                    else log('webrtc: ignoring unknown data channel ' + label);
                };
            }
            const answer = await pc.createAnswer();
            await pc.setLocalDescription(answer);
            if (kit && kit.extractIP) {
                const ip = kit.extractIP(pc.localDescription.sdp);
                log('webrtc: sending answer' + (ip ? ' (ip=' + ip + ')' : ''));
            }
            if (!signallingChannel || !signallingChannel.send({
                description: { type: 'answer', sdp: pc.localDescription.sdp },
                from: selfPeerId,
                to: remotePeerId,
            })) {
                fail('answer send failed');
            }
        } else if (description.type === 'answer') {
            if (kit && kit.extractIP) {
                const ip = kit.extractIP(sdp);
                log('webrtc: received answer' + (ip ? ' (ip=' + ip + ')' : ''));
            }
            await pc.setRemoteDescription(description);
        } else {
            log('webrtc: unknown description type ' + description.type);
        }
    }

    async function handleDialectMessage(msg) {
        if (!msg || typeof msg.from !== 'string') return;
        if (typeof msg.to === 'string' && msg.to !== selfPeerId) return;
        if (!remotePeerId && typeof msg.to === 'string' && msg.to === selfPeerId) {
            remotePeerId = msg.from;
        }
        if (msg.from !== remotePeerId) {
            log('webrtc: dialect from unknown peer ' + msg.from);
            return;
        }
        if (msg.description) {
            await handleDescription(msg.description);
        } else if (msg.iceCandidate) {
            if (!pc) pc = makePeerConnection();
            try {
                await pc.addIceCandidate(msg.iceCandidate);
            } catch (e) {
                log('webrtc: addIceCandidate failed: ' + e);
            }
        }
    }

    function maybeStartNegotiation() {
        if (!selfPeerId || !remotePeerId) return;
        const kit = getP2pkit();
        if (!kit) {
            fail('p2pkit unavailable');
            return;
        }
        if (!ensureSignallingChannel()) {
            fail('p2pkit unavailable');
            return;
        }
        if (kit.isInitiator(selfPeerId, remotePeerId)) {
            if (!pc) {
                log('webrtc: initiating offer (self=' + selfPeerId + ', remote=' + remotePeerId + ')');
                createOfferAndSend().catch(function (e) { fail('offer: ' + e); });
            }
        }
    }

    function handleSignalMessage(msg) {
        if (!msg || msg.v !== DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION) return;
        switch (msg.type) {
            case 'created':
                stats.roomCode = msg.room;
                selfPeerId = msg.peerId;
                log('webrtc: room created, code ' + msg.room);
                if (deps.onRoom) deps.onRoom(msg.room);
                if (deps.onEvent) deps.onEvent(DUNECITY_WEBRTC_EVENT_STATE, 0, 0, DUNECITY_WEBRTC_STATE_CONNECTING, null);
                break;
            case 'joined':
                stats.roomCode = msg.room;
                selfPeerId = msg.peerId;
                remotePeerId = msg.host;
                log('webrtc: joined room ' + msg.room + ', host ' + msg.host);
                if (deps.onRoom) deps.onRoom(msg.room);
                maybeStartNegotiation();
                break;
            case 'peer-joined':
                remotePeerId = msg.peerId;
                log('webrtc: peer joined');
                maybeStartNegotiation();
                break;
            case 'peer-left':
                log('webrtc: peer left');
                notifyPeerLeft();
                break;
            case 'error':
                fail('signaling: ' + msg.code + ' ' + msg.message);
                break;
            default:
                break;
        }
    }

    // ---- websocket lifecycle ----
    function connectSignaling(onOpen) {
        const url = resolveSignalingUrl(config.signaling);
        stats.signalingState = 'connecting';
        ws = new deps.WebSocket(url);
        ws.onopen = function () {
            stats.signalingState = 'open';
            log('webrtc: signaling connected (' + url + ')');
            const ch = ensureSignallingChannel();
            if (ch) ch._setReady();
            if (deps.onSignalingOpen) deps.onSignalingOpen();
            if (onOpen) onOpen();
        };
        ws.onclose = function () {
            if (stats.signalingState !== 'error') stats.signalingState = 'closed';
            log('webrtc: signaling closed');
            if (!bothChannelsOpen) fail('signaling closed before connect');
        };
        ws.onerror = function () {
            stats.signalingState = 'error';
            fail('signaling error');
        };
        ws.onmessage = function (evt) {
            let msg;
            try {
                msg = JSON.parse(evt.data);
            } catch (e) {
                log('webrtc: invalid signaling JSON');
                return;
            }
            if (isP2pkitDialectMessage(msg)) {
                const ch = ensureSignallingChannel();
                if (ch) ch._deliver(msg);
                return;
            }
            handleSignalMessage(msg);
        };
    }

    function resolveSignalingUrl(cfg) {
        if (cfg) return cfg;
        if (typeof location !== 'undefined' && location.host) {
            const scheme = location.protocol === 'https:' ? 'wss:' : 'ws:';
            return scheme + '//' + location.host;
        }
        return 'ws://127.0.0.1:8788';
    }

    function closeEverything() {
        controlOutbox = [];
        controlPaused = false;
        for (const k of [0, 1]) {
            if (channels[k]) {
                try { channels[k].close(); } catch (e) {}
                channels[k] = null;
                stats.channels[k].state = 'closed';
            }
        }
        if (pc) {
            try { pc.close(); } catch (e) {}
            pc = null;
        }
        if (ws) {
            try { ws.close(); } catch (e) {}
            ws = null;
            stats.signalingState = 'closed';
        }
        signallingChannel = null;
        remotePeerId = null;
    }

    // ---- outgoing game traffic ----
    function flushControlOutbox() {
        if (!channels[0] || channels[0].readyState !== 'open') return;
        while (controlOutbox.length > 0) {
            const bytes = controlOutbox[0];
            if (channels[0].bufferedAmount >= DUNECITY_WEBRTC_CONTROL_HIGH_WATER) {
                controlPaused = true;
                return;
            }
            controlOutbox.shift();
            channels[0].send(bytes);
            stats.channels[0].sent += 1;
            recordMessage('send', 0, bytes);
        }
    }

    function send(gameChannel, bytes) {
        const channel = channels[gameChannel];
        if (!channel || channel.readyState !== 'open') return false;
        if (gameChannel === 0) {
            if (controlPaused || channel.bufferedAmount >= DUNECITY_WEBRTC_CONTROL_HIGH_WATER) {
                controlPaused = true;
                controlOutbox.push(bytes);
                stats.channels[0].queued += 1;
                return true;   // queued, will be delivered in order
            }
            channel.send(bytes);
            stats.channels[0].sent += 1;
            recordMessage('send', 0, bytes);
            return true;
        }
        // commands channel: unreliable by contract — drop under congestion
        if (channel.bufferedAmount >= DUNECITY_WEBRTC_COMMANDS_HIGH_WATER) {
            stats.channels[1].dropped += 1;
            return false;
        }
        channel.send(bytes);
        stats.channels[1].sent += 1;
        recordMessage('send', 1, bytes);
        return true;
    }

    // ---- public api ----
    const api = {
        hostRoom: function () {
            if (stats.role) return false;
            stats.role = 'host';
            connectSignaling(function () {
                signalSend({ v: DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION, type: 'create' });
            });
            return true;
        },
        joinRoom: function (roomCode) {
            if (stats.role) return false;
            if (typeof roomCode !== 'string' || !/^[A-Z2-9]{4}$/.test(roomCode)) {
                fail('invalid room code');
                return false;
            }
            stats.role = 'client';
            connectSignaling(function () {
                signalSend({ v: DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION, type: 'join', room: roomCode });
            });
            return true;
        },
        send: send,
        getRoomCode: function () { return stats.roomCode; },
        getRole: function () { return stats.role; },
        getStats: function () { return stats; },
        getPeerHandle: function () { return peerHandle; },
        getRemotePeerId: function () { return remotePeerId; },
        // RTT estimate from WebRTC getStats (candidate-pair currentRoundTripTime),
        // refreshed every 2 s while connected; 0 while not connected.
        getRttMs: function () {
            return rttMs;
        },
        disconnect: function () {
            notifyPeerLeft();
            closeEverything();
            stats.role = null;
        },
        _flushControlOutboxForTest: flushControlOutbox,
    };

    return api;
}

// Node export (unit tests); browser/Emscripten wiring below.
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        createDuneCityWebRtc,
        createDuneCitySignallingChannel,
        resolveP2pkit,
        DUNECITY_WEBRTC_CONTROL_OPTIONS,
        DUNECITY_WEBRTC_COMMANDS_OPTIONS,
        DUNECITY_WEBRTC_CONTROL_HIGH_WATER,
        DUNECITY_WEBRTC_COMMANDS_HIGH_WATER,
        DUNECITY_WEBRTC_MAX_SIGNAL_BYTES,
        DUNECITY_WEBRTC_EVENT_CONNECT,
        DUNECITY_WEBRTC_EVENT_DISCONNECT,
        DUNECITY_WEBRTC_EVENT_MESSAGE,
        DUNECITY_WEBRTC_EVENT_STATE,
        DUNECITY_WEBRTC_STATE_IDLE,
        DUNECITY_WEBRTC_STATE_CONNECTING,
        DUNECITY_WEBRTC_STATE_CONNECTED,
        DUNECITY_WEBRTC_STATE_FAILED,
        _createSignallingChannelForTest: createDuneCitySignallingChannel,
    };
}

/*
 * Emscripten --js-library wiring. Compiled in by tools/web/build-emscripten.sh via
 * `--js-library platform/web/webrtc_glue.js`. When this file is loaded under Node
 * (unit tests), mergeInto/LibraryManager do not exist and this block is skipped.
 */
if (typeof mergeInto === 'function' && typeof LibraryManager !== 'undefined') {
    mergeInto(LibraryManager.library, {
        // Emscripten emits $NAME library items whose value is a string starting
        // with '=' as `var NAME = <verbatim>;` in dunecity.js. The values below
        // must match the top-level const declarations above exactly;
        // emscripten-webrtc-library.test.cjs enforces that.
        $DUNECITY_WEBRTC_CONTROL_LABEL: "='control'",
        $DUNECITY_WEBRTC_COMMANDS_LABEL: "='commands'",
        $DUNECITY_WEBRTC_CONTROL_OPTIONS: '={ ordered: true }',
        $DUNECITY_WEBRTC_COMMANDS_OPTIONS: '={ ordered: false, maxRetransmits: 0 }',
        $DUNECITY_WEBRTC_CONTROL_HIGH_WATER: '=(512 * 1024)',
        $DUNECITY_WEBRTC_CONTROL_LOW_WATER: '=(128 * 1024)',
        $DUNECITY_WEBRTC_COMMANDS_HIGH_WATER: '=(512 * 1024)',
        $DUNECITY_WEBRTC_MAX_SIGNAL_BYTES: '=(256 * 1024)',
        $DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION: '=1',
        $DUNECITY_WEBRTC_EVENT_CONNECT: '=0',
        $DUNECITY_WEBRTC_EVENT_DISCONNECT: '=1',
        $DUNECITY_WEBRTC_EVENT_MESSAGE: '=2',
        $DUNECITY_WEBRTC_EVENT_STATE: '=3',
        $DUNECITY_WEBRTC_STATE_IDLE: '=0',
        $DUNECITY_WEBRTC_STATE_CONNECTING: '=1',
        $DUNECITY_WEBRTC_STATE_CONNECTED: '=2',
        $DUNECITY_WEBRTC_STATE_FAILED: '=3',

        $cachedP2pkit: '=null',
        $resolveP2pkit__deps: ['$cachedP2pkit'],
        $resolveP2pkit: resolveP2pkit,
        $isP2pkitDialectMessage: isP2pkitDialectMessage,
        $validateSignallingMessage: validateSignallingMessage,
        $createDuneCitySignallingChannel__deps: [
            '$validateSignallingMessage', '$DUNECITY_WEBRTC_MAX_SIGNAL_BYTES',
        ],
        $createDuneCitySignallingChannel: createDuneCitySignallingChannel,

        // Retain the factory in emitted JS; Emscripten only keeps $-prefixed library
        // symbols. __deps recursively retains every $DUNECITY_WEBRTC_* constant above,
        // so the emitted factory has no free missing identifiers.
        $createDuneCityWebRtc__deps: [
            '$DUNECITY_WEBRTC_CONTROL_LABEL', '$DUNECITY_WEBRTC_COMMANDS_LABEL',
            '$DUNECITY_WEBRTC_CONTROL_OPTIONS', '$DUNECITY_WEBRTC_COMMANDS_OPTIONS',
            '$DUNECITY_WEBRTC_CONTROL_HIGH_WATER', '$DUNECITY_WEBRTC_CONTROL_LOW_WATER',
            '$DUNECITY_WEBRTC_COMMANDS_HIGH_WATER', '$DUNECITY_WEBRTC_MAX_SIGNAL_BYTES',
            '$DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION',
            '$DUNECITY_WEBRTC_EVENT_CONNECT', '$DUNECITY_WEBRTC_EVENT_DISCONNECT',
            '$DUNECITY_WEBRTC_EVENT_MESSAGE', '$DUNECITY_WEBRTC_EVENT_STATE',
            '$DUNECITY_WEBRTC_STATE_IDLE', '$DUNECITY_WEBRTC_STATE_CONNECTING',
            '$DUNECITY_WEBRTC_STATE_CONNECTED', '$DUNECITY_WEBRTC_STATE_FAILED',
            '$resolveP2pkit', '$createDuneCitySignallingChannel', '$isP2pkitDialectMessage',
        ],
        $createDuneCityWebRtc: createDuneCityWebRtc,

        $webrtcInit__deps: ['$createDuneCityWebRtc'],
        $webrtcInit: function () {
            if (Module.__dunecityWebrtc) return;
            const config = {
                signaling: (typeof DUNECITY_WEBRTC_CONFIG !== 'undefined' && DUNECITY_WEBRTC_CONFIG && DUNECITY_WEBRTC_CONFIG.signaling) || undefined,
                iceServers: (typeof DUNECITY_WEBRTC_CONFIG !== 'undefined' && DUNECITY_WEBRTC_CONFIG && DUNECITY_WEBRTC_CONFIG.iceServers) || undefined,
            };
            Module.__dunecityWebrtc = createDuneCityWebRtc({
                RTCPeerConnection: (typeof RTCPeerConnection !== 'undefined') ? RTCPeerConnection : window.RTCPeerConnection,
                WebSocket: WebSocket,
                config: config,
                log: function (msg) { Module.print('[' + msg + ']'); },
                onEvent: function (type, peer, channel, cause, bytes) {
                    if (type === 2 /* MESSAGE */ && bytes) {
                        const ptr = _malloc(bytes.length);
                        if (!ptr) return;
                        HEAPU8.set(bytes, ptr);
                        _webrtcOnEvent(type, peer, channel, cause, ptr, bytes.length);
                        _free(ptr);
                    } else {
                        _webrtcOnEvent(type, peer, channel, cause, 0, 0);
                    }
                },
            });
            Module.dunecityWebrtcStats = Module.__dunecityWebrtc.getStats;
        },

        webrtcHostRoom__deps: ['$webrtcInit'],
        webrtcHostRoom: function () {
            webrtcInit();
            return Module.__dunecityWebrtc.hostRoom() ? 1 : 0;
        },

        webrtcJoinRoom__deps: ['$webrtcInit'],
        webrtcJoinRoom: function (roomPtr) {
            webrtcInit();
            const room = UTF8ToString(roomPtr);
            return Module.__dunecityWebrtc.joinRoom(room) ? 1 : 0;
        },

        webrtcSendTo: function (peer, channel, ptr, len) {
            if (!Module.__dunecityWebrtc) return 0;
            const bytes = HEAPU8.slice(ptr, ptr + len);
            return Module.__dunecityWebrtc.send(channel, bytes) ? 1 : 0;
        },

        webrtcGetRoomCode: function (bufPtr, bufLen) {
            if (!Module.__dunecityWebrtc) return 0;
            const code = Module.__dunecityWebrtc.getRoomCode();
            if (!code) return 0;
            stringToUTF8(code, bufPtr, bufLen);
            return 1;
        },

        webrtcGetState: function () {
            if (!Module.__dunecityWebrtc) return 0; /* IDLE */
            const s = Module.__dunecityWebrtc.getStats();
            if (s.peerConnectionState === 'connected' && s.channels[0].state === 'open' && s.channels[1].state === 'open') return 2; /* CONNECTED */
            if (s.role) return 1; /* CONNECTING */
            return 0;
        },

        webrtcGetRttMs: function () {
            return (Module.__dunecityWebrtc && Module.__dunecityWebrtc.getRttMs()) | 0;
        },

        webrtcDisconnect: function () {
            if (Module.__dunecityWebrtc) Module.__dunecityWebrtc.disconnect();
        },
    });
}
