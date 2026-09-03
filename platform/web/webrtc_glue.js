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
        const iceServers = config.iceServers || [];
        const pcConfig = { iceServers: iceServers };
        const p = new deps.RTCPeerConnection(pcConfig);
        p.onicecandidate = function (evt) {
            if (evt.candidate) {
                signalSend({ v: DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION, type: 'signal', to: remotePeerId,
                             data: { kind: 'ice', candidate: evt.candidate } });
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

    // ---- offer/answer ----
    async function createOfferAndSend() {
        attachChannel(pc.createDataChannel(DUNECITY_WEBRTC_CONTROL_LABEL, DUNECITY_WEBRTC_CONTROL_OPTIONS), 0);
        attachChannel(pc.createDataChannel(DUNECITY_WEBRTC_COMMANDS_LABEL, DUNECITY_WEBRTC_COMMANDS_OPTIONS), 1);
        const offer = await pc.createOffer();
        await pc.setLocalDescription(offer);
        signalSend({ v: DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION, type: 'signal', to: remotePeerId,
                     data: { kind: 'offer', sdp: pc.localDescription } });
    }

    async function handleSignalData(data) {
        if (!pc) pc = makePeerConnection();
        if (data.kind === 'offer') {
            await pc.setRemoteDescription(data.sdp);
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
            signalSend({ v: DUNECITY_WEBRTC_SIGNAL_PROTOCOL_VERSION, type: 'signal', to: remotePeerId,
                         data: { kind: 'answer', sdp: pc.localDescription } });
        } else if (data.kind === 'answer') {
            await pc.setRemoteDescription(data.sdp);
        } else if (data.kind === 'ice') {
            try {
                await pc.addIceCandidate(data.candidate);
            } catch (e) {
                log('webrtc: addIceCandidate failed: ' + e);
            }
        } else {
            log('webrtc: unknown signal kind ' + data.kind);
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
                break;
            case 'peer-joined':
                remotePeerId = msg.peerId;
                log('webrtc: peer joined, creating offer');
                pc = makePeerConnection();
                createOfferAndSend().catch(function (e) { fail('offer: ' + e); });
                break;
            case 'signal':
                if (msg.from !== remotePeerId) {
                    log('webrtc: signal from unknown peer ' + msg.from);
                    return;
                }
                handleSignalData(msg.data).catch(function (e) { fail('signal: ' + e); });
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
    };
}

/*
 * Emscripten --js-library wiring. Compiled in by tools/web/build-emscripten.sh via
 * `--js-library platform/web/webrtc_glue.js`. When this file is loaded under Node
 * (unit tests), mergeInto/LibraryManager do not exist and this block is skipped.
 */
if (typeof mergeInto === 'function' && typeof LibraryManager !== 'undefined') {
    mergeInto(LibraryManager.library, {
        // Retain the factory in emitted JS; Emscripten only keeps $-prefixed library symbols.
        $createDuneCityWebRtc: createDuneCityWebRtc,

        $webrtcInit__deps: ['$createDuneCityWebRtc'],
        $webrtcInit: function () {
            if (Module.__dunecityWebrtc) return;
            const config = {
                signaling: (typeof DUNECITY_WEBRTC_CONFIG !== 'undefined' && DUNECITY_WEBRTC_CONFIG && DUNECITY_WEBRTC_CONFIG.signaling) || undefined,
                iceServers: (typeof DUNECITY_WEBRTC_CONFIG !== 'undefined' && DUNECITY_WEBRTC_CONFIG && DUNECITY_WEBRTC_CONFIG.iceServers) || [],
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
