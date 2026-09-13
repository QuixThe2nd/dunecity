# Campaign release validation — 1.0.680

Game source: `15e25a2`; subsequent `ae09b1a` and `77de33a` only integrate
release documentation and refine diagnostic fixtures. Native Release objects,
real campaign maps and deterministic seeds; dummy SDL and no frame pacing.
No skipped missions, forced victories or player combat commands.

## Results

71 scenarios (59 matrix plus 12 Atreides/Ordos late-game cases): **46 wins,
25 losses, zero crashes and zero timeouts**, each run to its natural result.
Every match had a nonempty enemy offensive sortie. All 13 Easy/Easy cases won,
as did all six Medium/Medium cases. All 21 level-9 Hard/Brutal helpers reached
15 harvesters. No house exceeded 15; all eight 679 over-cap reproductions now
stay within the engine cap. No sampled armed period above 600 army value went
more than five minutes without inflicting damage after minute 15.

Against 679, Harkonnen level 7 Brutal/Hard seed 1 and Atreides level 9 Hard/Hard
seed 1 changed from wins to losses. Ordos level 9 Brutal/Hard seed 1 now ends in
defeat at 26.33 minutes (679 reached the 60-minute cutoff, then lost at 85.44 in
an extended replay). Queue corrections alter spending and battle timing. These
are asymmetric campaign tests, not estimates of human win rates. Easy remains
forgiving; advanced enemies can overwhelm even advanced helpers.

## Checks

- Native dependency audits before/after the build, app signature and all seven
  CTest groups pass, including production menu widgets at three resolutions.
- Actual-engine pressure, retaliation/reinforcement, Hard and Brutal repairs,
  pacing, Starport cash and shared-house score fixtures pass.
- Worker fixture verifies eight existing workers plus a queued refinery worker
  plus six above-market imports; construction decisions explicitly reject
  extra refineries while the incoming workers fill the cap.
- Browser Release build uses the committed optimizer settings with no local
  linker override. All 769 Tornie and six Dune2R packaged files verify.
- Five browser-shell tests and 13 packaging/mod tests pass.
- In-app browser on an isolated localhost origin: fresh startup, campaign mod
  choices, Atreides level-9 launch with Brutal helper/Easy enemies, unit/building
  sidebar switching, feedback open/close and selection afterward pass.

Browser match completion, new WAN/NAT and multiplayer save/resume are not
claimed by these checks. Earlier public 679 menu/network verification remains
historical evidence. CI and publication outcomes are recorded separately.

## Evidence

Local logs: `/tmp/dunecity-campaign-balance/680-release-matrix`,
`680-release-houses`, `680-release-fixtures`, `680-helper-fixture-final`;
comparison tables: `680-release-main.md` and `680-release-houses.md` under the
same directory. Browser preview: `/tmp/dunecity-680-preview/play`.

| Case | Result | Game minutes | Helper peak harvesters | Enemy sorties | Enemy/player harvested spice | Enemy/player HP damage |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| atreides-l4-easy-easy-s42 | won | 15.29 | 15 | 1 | 0.149 | 0.374 |
| atreides-l9-easy-easy-s42 | won | 27.07 | 13 | 2 | 0.313 | 0.21 |
| harkonnen-l4-brutal-brutal-s42 | won | 16.18 | 15 | 1 | 0.435 | 0.177 |
| harkonnen-l4-easy-easy-s1 | won | 16.82 | 14 | 1 | 0.171 | 0.222 |
| harkonnen-l4-easy-easy-s42 | won | 16.61 | 15 | 1 | 0.159 | 0.2 |
| harkonnen-l4-hard-brutal-s42 | won | 15.64 | 15 | 1 | 0.439 | 0.173 |
| harkonnen-l4-hard-easy-s42 | won | 14.72 | 15 | 1 | 0.161 | 0.18 |
| harkonnen-l4-hard-hard-s42 | won | 15.35 | 15 | 1 | 0.336 | 0.231 |
| harkonnen-l4-hard-medium-s42 | won | 16.66 | 15 | 1 | 0.26 | 0.488 |
| harkonnen-l4-medium-medium-s42 | won | 17.99 | 14 | 2 | 0.303 | 0.381 |
| harkonnen-l5-brutal-brutal-s42 | lost | 17.08 | 15 | 2 | 2.73 | 2.737 |
| harkonnen-l5-easy-easy-s1 | won | 17.78 | 15 | 1 | 0.215 | 0.243 |
| harkonnen-l5-easy-easy-s42 | won | 17.61 | 15 | 1 | 0.288 | 0.295 |
| harkonnen-l5-hard-brutal-s42 | lost | 17.08 | 15 | 2 | 2.73 | 2.737 |
| harkonnen-l5-hard-easy-s42 | won | 15.42 | 15 | 1 | 0.255 | 0.159 |
| harkonnen-l5-hard-hard-s42 | lost | 12.91 | 15 | 1 | 0.729 | 5.529 |
| harkonnen-l5-hard-medium-s42 | won | 17.21 | 15 | 2 | 0.494 | 0.297 |
| harkonnen-l5-medium-medium-s42 | won | 20.44 | 15 | 3 | 0.528 | 0.331 |
| harkonnen-l6-brutal-brutal-s42 | lost | 19.26 | 15 | 3 | 0.861 | 1.835 |
| harkonnen-l6-brutal-hard-s1 | won | 21.02 | 15 | 3 | 0.304 | 0.376 |
| harkonnen-l6-brutal-hard-s42 | won | 19.94 | 15 | 2 | 0.367 | 0.387 |
| harkonnen-l6-easy-easy-s42 | won | 18.93 | 15 | 1 | 0.194 | 0.296 |
| harkonnen-l6-hard-brutal-s42 | lost | 19.26 | 15 | 3 | 0.861 | 1.835 |
| harkonnen-l6-hard-easy-s42 | won | 17.92 | 15 | 1 | 0.226 | 0.231 |
| harkonnen-l6-hard-hard-s42 | won | 19.17 | 15 | 2 | 0.398 | 0.419 |
| harkonnen-l6-hard-medium-s42 | won | 18.55 | 15 | 2 | 0.363 | 0.254 |
| harkonnen-l6-medium-medium-s42 | won | 18.66 | 14 | 1 | 0.365 | 0.246 |
| harkonnen-l7-brutal-brutal-s42 | lost | 17.37 | 15 | 2 | 1.307 | 2.682 |
| harkonnen-l7-brutal-hard-s1 | lost | 26.35 | 15 | 4 | 1.069 | 1.825 |
| harkonnen-l7-brutal-hard-s42 | won | 21.04 | 15 | 1 | 0.362 | 0.495 |
| harkonnen-l7-easy-easy-s42 | won | 21.24 | 15 | 1 | 0.176 | 0.306 |
| harkonnen-l7-hard-brutal-s42 | lost | 17.37 | 15 | 2 | 1.307 | 2.682 |
| harkonnen-l7-hard-easy-s42 | won | 19.01 | 15 | 1 | 0.184 | 0.311 |
| harkonnen-l7-hard-hard-s42 | won | 19.75 | 15 | 1 | 0.377 | 0.381 |
| harkonnen-l7-hard-medium-s42 | won | 17.51 | 15 | 2 | 0.342 | 0.301 |
| harkonnen-l7-medium-medium-s42 | won | 19.93 | 15 | 3 | 0.363 | 0.383 |
| harkonnen-l8-brutal-brutal-s42 | lost | 13.43 | 15 | 2 | 1.61 | 3.668 |
| harkonnen-l8-brutal-hard-s1 | lost | 37.63 | 15 | 14 | 0.906 | 0.852 |
| harkonnen-l8-brutal-hard-s42 | won | 24.19 | 15 | 4 | 0.803 | 0.332 |
| harkonnen-l8-easy-easy-s42 | won | 28.73 | 14 | 4 | 0.459 | 0.42 |
| harkonnen-l8-hard-brutal-s42 | lost | 13.43 | 15 | 2 | 1.61 | 3.668 |
| harkonnen-l8-hard-easy-s42 | won | 17.92 | 15 | 1 | 0.4 | 0.178 |
| harkonnen-l8-hard-hard-s42 | won | 33.27 | 15 | 12 | 0.829 | 0.485 |
| harkonnen-l8-hard-medium-s42 | won | 22.25 | 15 | 2 | 0.59 | 0.283 |
| harkonnen-l8-medium-medium-s42 | won | 29.7 | 11 | 5 | 0.863 | 0.384 |
| harkonnen-l9-brutal-brutal-s1 | lost | 13.47 | 15 | 3 | 3.339 | 3.978 |
| harkonnen-l9-brutal-brutal-s42 | lost | 15.11 | 15 | 3 | 4.485 | 2.682 |
| harkonnen-l9-brutal-hard-s1 | lost | 13.88 | 15 | 2 | 2.109 | 3.126 |
| harkonnen-l9-brutal-hard-s42 | lost | 16.59 | 15 | 4 | 2.153 | 1.82 |
| harkonnen-l9-easy-easy-s1 | won | 26.58 | 13 | 1 | 0.347 | 0.205 |
| harkonnen-l9-easy-easy-s42 | won | 29.32 | 12 | 3 | 0.304 | 0.234 |
| harkonnen-l9-hard-brutal-s42 | lost | 15.11 | 15 | 3 | 4.485 | 2.682 |
| harkonnen-l9-hard-easy-s42 | won | 21.54 | 15 | 1 | 0.314 | 0.138 |
| harkonnen-l9-hard-hard-s1 | lost | 13.88 | 15 | 2 | 2.109 | 3.126 |
| harkonnen-l9-hard-hard-s42 | lost | 16.59 | 15 | 4 | 2.153 | 1.82 |
| harkonnen-l9-hard-medium-s42 | won | 22.92 | 15 | 3 | 0.599 | 0.231 |
| harkonnen-l9-medium-medium-s42 | won | 25.13 | 11 | 3 | 0.613 | 0.226 |
| ordos-l4-easy-easy-s42 | won | 15.54 | 14 | 1 | 0.159 | 0.21 |
| ordos-l9-easy-easy-s42 | won | 38.16 | 13 | 6 | 0.396 | 0.496 |
| atreides-l9-brutal-easy-s1 | won | 22.81 | 15 | 2 | 0.366 | 0.142 |
| atreides-l9-brutal-easy-s42 | won | 24.62 | 15 | 2 | 0.408 | 0.145 |
| atreides-l9-brutal-hard-s1 | won | 28.34 | 15 | 13 | 0.93 | 0.229 |
| atreides-l9-brutal-hard-s42 | lost | 18.03 | 15 | 4 | 1.787 | 2.44 |
| atreides-l9-hard-hard-s1 | lost | 46.92 | 15 | 42 | 1.115 | 0.653 |
| atreides-l9-hard-hard-s42 | lost | 18.03 | 15 | 4 | 1.787 | 2.44 |
| ordos-l9-brutal-easy-s1 | won | 24.16 | 15 | 2 | 0.334 | 0.286 |
| ordos-l9-brutal-easy-s42 | won | 19.7 | 15 | 1 | 0.296 | 0.191 |
| ordos-l9-brutal-hard-s1 | lost | 26.33 | 15 | 14 | 1.266 | 1.254 |
| ordos-l9-brutal-hard-s42 | lost | 16.1 | 15 | 4 | 1.77 | 3.216 |
| ordos-l9-hard-hard-s1 | lost | 18.94 | 15 | 6 | 1.798 | 1.675 |
| ordos-l9-hard-hard-s42 | lost | 16.1 | 15 | 4 | 1.77 | 3.216 |
