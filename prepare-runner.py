#!/usr/bin/env python3
"""Prepare a separate DWProton runner with the NIKKE split-display fixes."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


TOOL = "DWProton-11.0-15-nikke-split-r7"
ORIGINALS = {
    "files/lib/wine/x86_64-unix/win32u.so":
        "d1350fb713b468d75767dce23e4748a171c73e27efa189afbf233abe9816b572",
    "files/lib/wine/x86_64-unix/winewayland.so":
        "bcbd5535aad66ab9b4bf12e93faf31c8fa43fc0dd4cba0532805c52778fb2f1b",
    "files/lib/wine/x86_64-windows/user32.dll":
        "26b500666ab2dcada42fe88ec2c717da1ac8be55e04d02c2c89985111d257557",
    "files/lib/wine/i386-windows/user32.dll":
        "6c6ffb1370ac21fc9b8a62e76da7ed8312254ee2982416590891cb7a3c69040b",
}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def matches(path):
    try:
        return all(digest(path / name) == expected for name, expected in ORIGINALS.items())
    except OSError:
        return False


def find_original():
    home = Path.home()
    roots = (
        home / ".local/share/Steam/compatibilitytools.d",
        home / ".steam/root/compatibilitytools.d",
        home / ".var/app/com.valvesoftware.Steam/data/Steam/compatibilitytools.d",
        home / ".local/share/bottles/runners",
        home / ".var/app/com.usebottles.bottles/data/bottles/runners",
        Path("/usr/share/steam/compatibilitytools.d"),
    )
    for root in roots:
        if root.is_dir():
            for candidate in sorted(root.iterdir()):
                if candidate.is_dir() and matches(candidate):
                    return candidate
    raise ValueError("DWProton 11.0-15 was not found; pass its folder as the first argument.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, help="original DWProton 11.0-15 folder")
    parser.add_argument("--destination", type=Path,
                        default=Path.home() / ".local/share/Steam/compatibilitytools.d" / TOOL)
    args = parser.parse_args()
    source = (args.source or find_original()).expanduser().resolve(strict=True)
    if not matches(source):
        raise ValueError("The source modules do not match unmodified DWProton 11.0-15.")
    modules = Path(__file__).resolve().parent / "modules"
    manifest = json.loads((modules / "sha256.json").read_text())
    if set(manifest) != set(ORIGINALS):
        raise ValueError("The replacement-module manifest is incomplete.")
    for name, expected in manifest.items():
        if digest(modules / name) != expected:
            raise ValueError(f"Replacement module checksum mismatch: {name}")
    destination = args.destination.expanduser().absolute()
    if os.path.lexists(destination):
        raise ValueError(f"Destination already exists: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".nikke-split-", dir=destination.parent) as temp:
        stage = Path(temp) / TOOL
        subprocess.run(["cp", "-a", "--reflink=auto", "--", str(source), str(stage)], check=True)
        for name in manifest:
            target = stage / name
            target.unlink()
            shutil.copyfile(modules / name, target)
            target.chmod(0o755)
        # The distinct runner path makes Proton refresh its builtin DLL links.
        # Zero keeps this test runner behind released runners for automatic selection.
        (stage / "version").write_text(f"0 {TOOL}\n")
        vdf = stage / "compatibilitytool.vdf"
        vdf.write_text(vdf.read_text().replace("dwproton-11.0-15-x86_64", TOOL))
        if os.path.lexists(destination):
            raise ValueError(f"Destination already exists: {destination}")
        stage.rename(destination)
    print(f"Created {destination}")
    print(f"Select {TOOL} for NIKKE, keep split display, and clear the X11 executable field.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
