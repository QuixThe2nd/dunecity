# macOS release runner

DuneCity's macOS DMG is built by a repository-level self-hosted GitHub Actions
runner. The designated builder is the **Mac mini** (`claw.local`, M4, 10 cores /
16 GB), registered as `dunecity-mac-mini`.

Runner selection is by label, not by name. The job asks for
`[self-hosted, macOS, ARM64, dunecity-builder]`, and `dunecity-builder` is the
only part that is settable — the other three are read-only labels GitHub assigns
by platform. The MacBook Air is still registered as `dunecity-macbook-air` but
deliberately carries only the three read-only labels, so it can never claim a
release job. That is the fix for a real failure: on 2026-09-06 the Air took the
1.0.536 macOS build, went to sleep partway through `Install build deps`, and the
job died with every remaining step unreported.

Re-arm the Air as a fallback when the mini is down:

```bash
gh api -X POST repos/VR48/dunecity/actions/runners/25/labels -f 'labels[]=dunecity-builder'
# and to stand it down again:
gh api -X DELETE repos/VR48/dunecity/actions/runners/25/labels/dunecity-builder
```

With the label on both machines, whichever runner is online and idle claims the
job — so only do that deliberately.

The workflow deliberately excludes pull requests. A self-hosted runner executes
repository code on the host, so only pushes by maintainers, version tags, and
manual workflow runs may use it.

## One-time host setup

1. Install the native tools:

   ```bash
   xcode-select --install
   brew install autoconf autoconf-archive automake cmake libtool pkg-config nasm ninja
   ```

2. Register the runner outside the repository. A registration token is valid for
   one hour and can be minted from the CLI rather than the web UI:

   ```bash
   TOKEN=$(gh api -X POST repos/VR48/dunecity/actions/runners/registration-token --jq .token)
   mkdir -p ~/actions-runner-dunecity && cd ~/actions-runner-dunecity
   curl -fsSL -o runner.tar.gz \
     https://github.com/actions/runner/releases/download/v2.337.0/actions-runner-osx-arm64-2.337.0.tar.gz
   tar xzf runner.tar.gz && rm runner.tar.gz
   ./config.sh --url https://github.com/VR48/dunecity --token "$TOKEN" \
     --name dunecity-mac-mini --labels dunecity-builder --work _work --unattended --replace
   ```

3. **Give the runner a PATH that includes Homebrew.** This is the step that is
   easy to miss and fails confusingly. Jobs inherit their environment from
   launchd, not from a login shell, so `/opt/homebrew/bin` is absent and `brew`
   is not found — even though it works fine over SSH. The runner reads `.path`
   in its own directory and uses it verbatim:

   ```bash
   echo '/opt/homebrew/bin:/opt/homebrew/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin' \
     > ~/actions-runner-dunecity/.path
   ```

4. Install and start its LaunchAgent from the runner directory:

   ```bash
   ./svc.sh install
   ./svc.sh start
   ./svc.sh status
   ```

5. **Stop the machine sleeping through builds.** The mini ships with
   `pmset sleep 1` — it naps a minute after the last session, which drops the
   runner's long-poll connection and kills any job in flight. Rather than change
   the system-wide power settings (which needs sudo and outlives the runner),
   wrap the service in `caffeinate` so the assertion lives exactly as long as the
   runner does. Edit
   `~/Library/LaunchAgents/actions.runner.VR48-dunecity.<name>.plist` so
   `ProgramArguments` reads:

   ```xml
   <array>
     <string>/usr/bin/caffeinate</string>
     <string>-s</string>
     <string>-i</string>
     <string>-m</string>
     <string>/Users/<user>/actions-runner-dunecity/runsvc.sh</string>
   </array>
   ```

   Add `<key>KeepAlive</key><true/>` while you are in there, then
   `./svc.sh stop && ./svc.sh start`. Confirm with
   `pmset -g assertions | grep PreventSystemSleep`.

6. Seed the vcpkg binary cache from an existing runner so the first build
   restores prebuilt SDL2 and friends instead of compiling them:

   ```bash
   cd ~/.cache/vcpkg/archives && tar cf - . | ssh <new-host> 'mkdir -p ~/.cache/vcpkg/archives && tar xf - -C ~/.cache/vcpkg/archives'
   ```

7. Set the repository Actions variable `ENABLE_MACOS_BUILD` to `true`. Set it to
   `false` before taking every Mac runner offline for an extended period, or
   release workflows will wait for the Mac job until its timeout.

The service runs as the user that configured it, from a LaunchAgent — so that
account must be logged in for the runner to come back after a reboot. Enable
automatic login on a headless builder, or expect to log in by hand.

## What the job produces

The runner builds the ARM64 app with vcpkg, stages non-system dylibs inside
`dunecity.app/Contents/Frameworks`, applies an ad-hoc signature, and creates
`DuneCity-X.Y.Z-macOS.dmg`. The verification step mounts the DMG and checks:

- app metadata and version match the source version;
- the executable has an ARM64 slice;
- no Homebrew, user-directory, or vcpkg build paths remain;
- the app bundle has a valid code signature and required game data;
- the drag-to-Applications link exists.

The DMG is uploaded as the `DuneCity-macOS-DMG` workflow artifact. On a `vX.Y.Z`
tag, the existing release job attaches it to the GitHub Release and updates the
dunelegacy.com download links after publishing.

## Release sequence

Run a manual workflow on the release commit before tagging it. Once its macOS
job passes:

```bash
scripts/bump-version.sh X.Y.Z
git add CMakeLists.txt include/config.h vcpkg.json
git commit -m "release: prepare DuneCity X.Y.Z"
git push origin main
git tag vX.Y.Z
git push origin vX.Y.Z
```

The source version must be committed before the tag. The tag workflow rejects a
version mismatch and publishes the website only after the GitHub Release exists.

## Unattended Developer ID signing

The dedicated `DuneCity-Signing.keychain-db` must be unlocked and its signing
key must allow Apple signing tools without an interactive access prompt.
Unlocking alone is insufficient: a background runner can fail with
`errSecInternalComponent`, while Security logs report
`errSecInteractionNotAllowed` or `CSSMERR_CSP_NO_USER_INTERACTION`.

Provision the key partition list once after importing or replacing the key.
Scope this operation to the dedicated release keychain; do not apply it to the
login or system keychain. Read its password from protected local storage and
capture tool output so keychain details are not added to build logs:

```python
from pathlib import Path
import subprocess

keychain = Path.home() / "Library/Keychains/DuneCity-Signing.keychain-db"
password_file = (Path.home() / "Library/Application Support/DuneCity Signing"
                 / "34X7AYJZ93/signing-keychain-password")
password = password_file.read_text().strip()
for arguments in (
    ["security", "unlock-keychain", "-p", password, str(keychain)],
    ["security", "set-key-partition-list", "-S", "apple-tool:,apple:,codesign:",
     "-s", "-k", password, str(keychain)],
):
    result = subprocess.run(arguments, capture_output=True)
    if result.returncode:
        raise SystemExit("Dedicated signing keychain setup failed")
```

Also add the dedicated keychain to the user search list while preserving existing
entries. `codesign --keychain` alone was insufficient on this host: a separate
background security session selected an inaccessible duplicate identity from the
login keychain. Prepending the dedicated keychain fixed the isolated signing probe:

```python
import shlex
existing = subprocess.run(["security", "list-keychains", "-d", "user"],
                          capture_output=True, text=True, check=True)
keychains = shlex.split(existing.stdout)
if str(keychain) not in keychains:
    subprocess.run(["security", "list-keychains", "-d", "user", "-s",
                    str(keychain), *keychains], capture_output=True, check=True)
```

Verify the saved password with an explicit lock/unlock cycle before relying on
it. On this host an already-unlocked keychain masked a saved-password mismatch.
Keep an encrypted identity backup and its protected password. If recovery is
needed, preserve the old keychain before restoring the identity into a fresh one;
Apple notarization credentials must be provisioned again through the setup shortcut.

Verify signing in a temporary LaunchAgent with `SessionCreate=true`, matching
the real runner, and then remove the probe. A detached subprocess in an existing
GUI security session does not establish unattended access. Verify `notarytool
history` uses `DuneCityNotarization` from this keychain without prompts. The actual
stable job remains the final check. Retry the failed job after the workflow
completes; an environment repair does not require moving the release tag or
rebuilding successful platform jobs.

### Interactive notarization credential setup

Run `python3 scripts/setup-macos-notarization.py` from Terminal. The mini's
Desktop `DuneCity-Automatic-Updates-Setup.command` invokes an installed copy in
the protected signing directory. The helper checks email/password formatting,
removes copied surrounding whitespace/quotes and accepts Apple's generated
four-group password through a hidden prompt. It supplies that value to
notarytool's hidden TTY prompt, never as a process argument or log field.

After Apple validates the credential, it stores `DuneCityNotarization` in the
separate `DuneCity-Notarization.keychain-db` and in the signing keychain used by
the existing release workflow. The separate keychain's random local password is
kept as `notarization-keychain-password` beside the signing setup files. Preserve
this keychain when repairing the code-signing identity. No plaintext Apple
password is saved by the helper. Keep a user-managed credential backup in a
password manager as well.

`notarization-setup-status.json` contains only a fixed diagnostic stage and time,
allowing support to distinguish a format problem, Apple's 401 rejection and
successful stored-profile validation without collecting terminal contents or
passwords. A 401 after the format check still needs investigation; it is not a
reason to rebuild or replace the local signing keychain.
