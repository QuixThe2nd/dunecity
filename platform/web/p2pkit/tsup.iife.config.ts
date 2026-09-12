// DuneCity-local IIFE bundle config (see VENDORED.md, "Local additions").
//
// Produces dist/p2pkit.iife.js, a self-contained browser bundle that exposes
// globalThis.P2PKIT_IIFE. Run ONCE when vendoring/updating p2pkit and commit
// the result — the Emscripten build consumes the committed bundle and must not
// need npm/esbuild or network access:
//
//   cd platform/web/p2pkit && npm ci --omit=optional && npm run build:iife
import { defineConfig } from "tsup"

export default defineConfig({
  entry: { p2pkit: "src/iife.ts" },
  format: ["iife"],
  target: "es2022",
  globalName: "P2PKIT_IIFE",
  outDir: "dist",
  outExtension: () => ({ js: ".iife.js" }),
  sourcemap: false,
  clean: false,
  treeshake: true,
  // The browser graph must stay self-contained: everything reachable from
  // src/iife.ts is pure TypeScript with no Node-only imports.
  footer: { js: "globalThis.P2PKIT_IIFE = P2PKIT_IIFE;" },
})
