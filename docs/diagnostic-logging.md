# Diagnostic logging

From 1.0.722, **Settings → Advanced → Diagnostic logs (development)** controls
development capture. Apply saves the preference and reinitializes the menu;
the next game uses the selected setting. This preference is local, not a game
rule and not part of saved gameplay or multiplayer negotiation.

- Browser: off by default, including existing profiles without the new key.
- Desktop: on by default for the existing development workflow.
- An explicit saved choice is respected on subsequent starts/reloads.

The corresponding user INI setting is:

```ini
[General]
Diagnostic Logs = true
```

Set it to `false` to disable capture. Missing keys use the platform default.
The packaged config template cannot accidentally enable fresh browser profiles:
default-config creation writes the platform-specific value explicitly.

When disabled, the game does not create AI decision/ledger/performance JSONL
sessions, aggregate their metrics, or write the performance text log. Routine
SDL application messages are filtered before formatting. The additional
`dunecity-crash.log` development mirror is also disabled. Warning, error and
critical SDL messages still go to stderr; fatal exceptions are logged and shown.
Native crash handling remains installed. Existing log files are not deleted.

Browser stdout/stderr stays on the browser console instead of being redirected
into a persistent profile log. Opting in enables routine console output plus
the structured and performance files under `/home/web_user/.config/DuneCity`.
Those files participate in the existing browser storage sync. Disabling capture
stops them growing; saves/settings persistence is unchanged. Desktop errors
continue using the normal stderr destination, including the native log when
started without `--showlog`.

`DUNECITY_AI_TELEMETRY=0` remains an additional developer override to suppress
structured capture. It does not enable capture when the setting is off, nor does
it control the other diagnostic channels. `--showlog` only chooses the native
stdout/stderr destination; it does not override the setting.

For a real-engine saved-game check in private profiles:

```sh
python3 tests/performance/run-simulation-probe.py --save /absolute/city.dls \
  --output-dir /absolute/diagnostics-on --diagnostics on --cycles 4000
python3 tests/performance/run-simulation-probe.py --save /absolute/city.dls \
  --output-dir /absolute/diagnostics-off --diagnostics off --cycles 4000 \
  --compare-dir /absolute/diagnostics-on
```

The probe checks that capture files exist when enabled and are absent when
disabled, and that an error still reaches stderr. Its own checkpoint output
bypasses diagnostic filtering so the saved-state comparison remains available.
CTest also exercises capture shutdown and the real Settings control/persistence.
