from __future__ import annotations

import hashlib
from pathlib import Path
import sys


EXCLUDED_FILES = {"checksums.sha256", ".dunecity-managed"}
NORMALIZED_TEXT_EXTENSIONS = {".ini", ".json", ".md"}


def normalize_text_file(path: Path) -> None:
    content = path.read_bytes()
    normalized = content.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    if path.suffix.lower() == ".ini":
        normalized = b"\n".join(line.rstrip(b" \t") for line in normalized.split(b"\n"))
        normalized = normalized.rstrip(b"\n") + b"\n"
    if normalized != content:
        path.write_bytes(normalized)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(64 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: rebuild_tornie_checksums.py <mods/Tornie>", file=sys.stderr)
        return 2

    root = Path(sys.argv[1]).resolve()
    if not (root / "mod.ini").is_file():
        print(f"Not a Tornie payload directory: {root}", file=sys.stderr)
        return 2

    for path in root.rglob("*"):
        if path.is_file() and path.suffix.lower() in NORMALIZED_TEXT_EXTENSIONS:
            normalize_text_file(path)

    files: list[tuple[str, Path]] = []
    portable_keys: set[str] = set()
    for path in root.rglob("*"):
        if path.is_symlink():
            print(f"Symbolic links are not allowed: {path}", file=sys.stderr)
            return 1
        if not path.is_file():
            continue

        relative = path.relative_to(root).as_posix()
        if relative in EXCLUDED_FILES:
            continue
        key = relative.lower()
        if key in portable_keys:
            print(f"Duplicate or case-colliding payload path: {relative}", file=sys.stderr)
            return 1
        portable_keys.add(key)
        files.append((relative, path))

    files.sort(key=lambda item: item[0])
    output = "".join(f"{sha256_file(path)}  {relative}\n" for relative, path in files)
    (root / "checksums.sha256").write_text(output, encoding="utf-8", newline="\n")
    print(f"Wrote checksums.sha256 with {len(files)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
