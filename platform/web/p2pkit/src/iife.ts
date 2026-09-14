// DuneCity-local browser bundle entry (see VENDORED.md, "Local additions").
//
// The game build cannot run esbuild/tsup at wasm-build time, so this entry is
// compiled ONCE into dist/p2pkit.iife.js (globalThis.P2PKIT_IIFE) and the
// committed bundle is what ships. Only the modules the WebRTC glue needs are
// exported; Node-only paths (WebSocketSignalling's `ws` fallback, µTP/DHT/HTTP
// transports) stay out of the browser graph.
//
// NOTE: DuneCity's game data plane does NOT ride p2pkit's RTCTransport — it
// needs two native binary RTCDataChannels. RTCTransport is exported anyway so
// the SignallingChannel adapter the glue implements stays provably compatible
// with stock p2pkit consumers (see platform/web/test/webrtc-glue.test.cjs).

export { isInitiator, chooseTransport, capsFor, DEFAULT_TRANSPORT_ORDER } from "./transports/negotiate.js"
export { RTCTransport } from "./transports/rtc.js"
export { DEFAULT_ICE_SERVERS } from "./utils/ice.js"
export { extractIP } from "./utils/sdp.js"
export { Emitter } from "./utils/emitter.js"
export { randomId } from "./utils/id.js"
export type { SignallingChannel, SignallingMessage } from "./signalling/types.js"
