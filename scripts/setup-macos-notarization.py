#!/usr/bin/env python3
"""Interactive local notarization setup; never log or persist the Apple password."""
import errno
import getpass
import json
import os
from pathlib import Path
import pty
import re
import select
import secrets
import signal
import subprocess
import termios
import time

TEAM = "34X7AYJZ93"
PROFILE = "DuneCityNotarization"
ROOT = Path.home() / "Library/Application Support/DuneCity Signing" / TEAM
SIGNING = Path.home() / "Library/Keychains/DuneCity-Signing.keychain-db"
NOTARIZATION = SIGNING.with_name("DuneCity-Notarization.keychain-db")


def record_status(stage):
    """Record only a fixed diagnostic stage, never input or Apple output."""
    path = ROOT / "notarization-setup-status.json"
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as stream:
        json.dump({"stage": stage, "time": time.time()}, stream)


def normalize_password(value):
    value = "".join(c for c in value if not c.isspace() and c not in "\u200b\ufeff")
    value = value.strip("\"'“”‘’")
    if re.fullmatch(r"[a-z]{4}(?:-[a-z]{4}){3}", value):
        return value
    if re.fullmatch(r"[a-z]{16}", value):
        return "-".join(value[i:i + 4] for i in range(0, 16, 4))
    raise ValueError("Paste only Apple's generated password: four groups of four lowercase letters. "
                     "Do not paste its name, your iCloud password, or a verification code.")


def security(arguments):
    result = subprocess.run(["/usr/bin/security", *map(str, arguments)],
                            capture_output=True, timeout=20)
    if result.returncode:
        raise RuntimeError("Local keychain step failed: " + arguments[0] +
                           " (code " + str(result.returncode) + ").")


def private_password_file(path):
    if not path.exists():
        fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w") as stream:
            stream.write(secrets.token_hex(16) + "\n")
    return path.read_text().strip()


def secure_prompt_command(command, password, timeout=90):
    """Supply the password to a hidden TTY prompt, never a process argument."""
    pid, master = pty.fork()
    if pid == 0:
        try:
            attrs = termios.tcgetattr(0)
            attrs[3] &= ~(termios.ECHO | termios.ECHONL)
            termios.tcsetattr(0, termios.TCSANOW, attrs)
            os.execv(command[0], command)
        except BaseException:
            os._exit(127)
    output = bytearray()
    sent = False
    finished = False
    try:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            ready, _, _ = select.select([master], [], [], max(0, min(1, deadline - time.monotonic())))
            if not ready:
                continue
            try:
                data = os.read(master, 8192)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                break
            if not data:
                break
            output.extend(data)
            if len(output) > 65536:
                raise RuntimeError("Apple's setup tool produced unexpected output; stopped safely.")
            if not sent and re.search(rb"App-specific password for [^\r\n]*: ?$", output):
                os.write(master, password.encode() + b"\n")
                sent = True
        else:
            raise RuntimeError("Apple's validation timed out. No password was written to a log.")
        _, status = os.waitpid(pid, 0)
        finished = True
        message = output.decode(errors="replace").replace(password, "[password hidden]")
        return os.waitstatus_to_exitcode(status), message
    finally:
        if not finished:
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(pid, 0)
        os.close(master)


def store_credentials(keychain, email, password):
    code, message = secure_prompt_command(
        ["/usr/bin/xcrun", "notarytool", "store-credentials", PROFILE,
         "--apple-id", email, "--team-id", TEAM, "--keychain", str(keychain)], password)
    if code:
        if "401" in message:
            record_status("apple-rejected-formatted-credentials")
            raise RuntimeError("Apple returned HTTP 401 after receiving a password with the expected format. "
                               "The keychain is working. Keep this window open and tell Codex: "
                               "Apple 401, format checked. Do not generate another password yet.")
        error_lines = [line for line in message.splitlines() if "error" in line.lower()]
        raise RuntimeError("Apple setup failed (code " + str(code) + "): " +
                           (" ".join(error_lines) or "No error detail was supplied."))


def main():
    record_status("awaiting-user-input")
    print("\nDuneCity Apple notarization setup\n")
    print("Use the generated app-specific password you already have. Do not generate another.")
    email = input("Apple account email [icloudlogin@fastmail.com]: ").strip() or "icloudlogin@fastmail.com"
    if not re.fullmatch(r"[^\s@]+@[^\s@]+\.[^\s@]+", email):
        raise ValueError("The account email contains an unexpected space or character.")
    while True:
        try:
            password = normalize_password(getpass.getpass("Paste Apple's generated password (hidden), then Return: "))
            break
        except ValueError as error:
            print(str(error))
    print("Password format checked. Contacting Apple for " + email + "...")
    record_status("format-checked")
    security(["unlock-keychain", "-p", (ROOT / "signing-keychain-password").read_text().strip(), SIGNING])
    notarization_password = private_password_file(ROOT / "notarization-keychain-password")
    if not NOTARIZATION.exists():
        security(["create-keychain", "-p", notarization_password, NOTARIZATION])
        NOTARIZATION.chmod(0o600)
    security(["unlock-keychain", "-p", notarization_password, NOTARIZATION])
    # Keep a dedicated credential copy so future signing-key repairs cannot erase it.
    store_credentials(NOTARIZATION, email, password)
    record_status("apple-accepted-separate-credential-saved")
    print("Apple accepted the login; saved in the separate notarization keychain.")
    # The already-tagged release workflow uses this compatibility location.
    store_credentials(SIGNING, email, password)
    result = subprocess.run(["/usr/bin/xcrun", "notarytool", "history", "--keychain-profile", PROFILE,
                             "--keychain", str(SIGNING), "--output-format", "json"],
                            capture_output=True, timeout=30)
    if result.returncode:
        raise RuntimeError("Credentials were saved, but the final authentication check failed.")
    record_status("complete-verified")
    print("\nSETUP COMPLETE. Saved credentials verified. Tell Codex: setup complete.")


if __name__ == "__main__":
    try:
        main()
    except (EOFError, KeyboardInterrupt):
        print("\nSetup cancelled.")
    except (ValueError, RuntimeError) as error:
        print("\n" + str(error))
    except Exception:
        print("\nLocal setup failed unexpectedly. Keep this window open and tell Codex.")
    finally:
        try:
            input("\nPress Return to close.")
        except (EOFError, KeyboardInterrupt):
            pass
