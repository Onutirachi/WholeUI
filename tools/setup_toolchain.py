"""Fetch the pinned llvm-mingw toolchain into build/toolchain/ (research R1).

Reads tools/toolchain.json, downloads the archive (or reuses a copy already in
build/toolchain/), verifies its SHA-256, extracts it without the target folders WholeUI does
not compile for, and checks that clang reports the pinned LLVM version. Does nothing when the
pinned toolchain is already extracted. Python standard library only.

Usage: python tools/setup_toolchain.py
Prints the toolchain folder on success; exits non-zero on any failure.
"""

from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONFIG = REPO / "tools" / "toolchain.json"
TOOLCHAIN_DIR = REPO / "build" / "toolchain"
MARKER = ".wholeui-complete"
CHUNK = 1 << 20


def fail(message: str) -> None:
    print(f"setup_toolchain: {message}", file=sys.stderr)
    sys.exit(1)


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(CHUNK):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, target: Path, expected_sha: str) -> None:
    partial = target.with_name(target.name + ".part")
    print(f"downloading {url}")
    digest = hashlib.sha256()
    try:
        with urllib.request.urlopen(url, timeout=60) as response, partial.open("wb") as out:
            while chunk := response.read(CHUNK):
                out.write(chunk)
                digest.update(chunk)
    except OSError as error:
        partial.unlink(missing_ok=True)
        fail(f"download failed: {error}")
    if digest.hexdigest() != expected_sha:
        partial.unlink(missing_ok=True)
        fail(f"SHA-256 mismatch for {url}: got {digest.hexdigest()}, expected {expected_sha}")
    partial.replace(target)


def extract(archive: Path, root: str, skip_prefixes: list[str], destination: Path) -> None:
    # Extract in place: the completion marker is written only after every step succeeded, so
    # an interrupted extraction is redone from scratch on the next run. (Renaming a staging
    # folder is not reliable on Windows: antivirus scans keep the large executables open.)
    shutil.rmtree(destination, ignore_errors=True)
    base = destination.resolve()
    prefix = root + "/"
    with zipfile.ZipFile(archive) as bundle:
        for info in bundle.infolist():
            if not info.filename.startswith(prefix):
                fail(f"unexpected entry outside {root}/: {info.filename}")
            relative = info.filename[len(prefix):]
            if not relative or any(relative.startswith(p) for p in skip_prefixes):
                continue
            target = (base / relative).resolve()
            if not target.is_relative_to(base):
                fail(f"unsafe path in archive: {info.filename}")
            if info.is_dir():
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with bundle.open(info) as source, target.open("wb") as out:
                shutil.copyfileobj(source, out, CHUNK)


def check_version(destination: Path, llvm_version: str) -> None:
    clang = destination / "bin" / "clang++.exe"
    if sys.platform != "win32":
        print(f"not on Windows: skipping the {clang.name} version check")
        return
    try:
        output = subprocess.run(
            [str(clang), "--version"], capture_output=True, text=True, check=True
        ).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        fail(f"cannot run {clang}: {error}")
    first_line = output.splitlines()[0] if output else ""
    if f"clang version {llvm_version} " not in first_line + " ":
        fail(f"expected clang {llvm_version}, got: {first_line}")
    print(first_line)


def main() -> None:
    config = json.loads(CONFIG.read_text(encoding="utf-8"))["llvm_mingw"]
    destination = TOOLCHAIN_DIR / config["archive_root"]
    marker = destination / MARKER

    if marker.is_file() and marker.read_text(encoding="utf-8").strip() == config["sha256"]:
        print(destination)
        return

    TOOLCHAIN_DIR.mkdir(parents=True, exist_ok=True)
    archive = TOOLCHAIN_DIR / config["url"].rsplit("/", 1)[-1]
    if archive.is_file() and sha256_of(archive) == config["sha256"]:
        print(f"reusing {archive.name}")
    else:
        download(config["url"], archive, config["sha256"])

    print(f"extracting into {destination}")
    extract(archive, config["archive_root"], config["skip_prefixes"], destination)
    check_version(destination, config["llvm_version"])
    marker.write_text(config["sha256"] + "\n", encoding="utf-8")
    print(destination)


if __name__ == "__main__":
    main()
