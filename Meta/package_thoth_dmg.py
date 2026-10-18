#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Create a drag-to-Applications macOS disk image for Thoth Browser."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent
DEFAULT_APP = SOURCE / "Build/0.9.1/Thoth Browser.app"
DEFAULT_OUTPUT = SOURCE / "Build/Thoth-Browser-0.9.1-macos-arm64.dmg"


def run(*command: str | Path) -> str:
    result = subprocess.run(
        [str(item) for item in command], check=True, capture_output=True, text=True
    )
    return result.stdout


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, default=DEFAULT_APP)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--volume-name", default="Thoth Browser")
    parser.add_argument("--signing-identity", help="Developer ID Application identity for the disk image")
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()

    app = args.app.expanduser().resolve()
    output = args.output.expanduser().absolute()
    if not app.is_dir() or app.suffix != ".app":
        parser.error(f"application bundle does not exist: {app}")
    if output.suffix != ".dmg":
        parser.error("output filename must end in .dmg")
    if output.exists():
        if not args.replace:
            parser.error(f"output exists; pass --replace to replace this disk image: {output}")
        if output.is_dir() or output.is_symlink():
            parser.error("refusing to replace a directory or symlink")
        output.unlink()

    run("codesign", "--verify", "--deep", "--strict", app)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="thoth-dmg-") as temporary_directory:
        staging = Path(temporary_directory)
        shutil.copytree(app, staging / app.name, symlinks=True)
        (staging / "Applications").symlink_to("/Applications", target_is_directory=True)
        run(
            "hdiutil",
            "create",
            "-volname",
            args.volume_name,
            "-srcfolder",
            staging,
            "-ov",
            "-format",
            "UDZO",
            output,
        )

    run("hdiutil", "verify", output)
    if args.signing_identity:
        run("codesign", "--force", "--sign", args.signing_identity, "--timestamp", output)
        run("codesign", "--verify", "--verbose=2", output)
    print(f"Created drag-to-Applications disk image: {output}")
    if not args.signing_identity:
        print("This image is not Developer ID signed or notarized.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
