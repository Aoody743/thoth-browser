#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Run cheap smoke checks against a built Thoth Browser macOS bundle."""

from __future__ import annotations

import argparse
import hashlib
import plistlib
import re
import subprocess
import sys
from pathlib import Path


EXPECTED_VERSION = "0.9.2"
EXPECTED_IDENTIFIER = "thoth.aoody"
EXPECTED_EXECUTABLE = "Thoth Browser"
EXPECTED_MINIMUM_MACOS_VERSION = "14.4"


def fail(message: str) -> None:
    print(f"FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=Path.home() / ".cache/thoth-browser")
    parser.add_argument("--app", type=Path)
    parser.add_argument("--require-signature", action="store_true")
    parser.add_argument("--require-developer-id", action="store_true")
    args = parser.parse_args()

    app = (args.app or args.build_root / "Build/release/bin/Thoth Browser.app").expanduser().resolve()
    contents = app / "Contents"
    executable = contents / "MacOS" / EXPECTED_EXECUTABLE
    info_plist = contents / "Info.plist"

    if not app.is_dir():
        fail(f"bundle does not exist: {app}")
    if not executable.is_file():
        fail(f"main executable does not exist: {executable}")
    if not info_plist.is_file():
        fail(f"Info.plist does not exist: {info_plist}")

    try:
        info = plistlib.loads(info_plist.read_bytes())
    except (OSError, plistlib.InvalidFileException) as error:
        fail(f"cannot read Info.plist: {error}")

    checks = {
        "CFBundleIdentifier": EXPECTED_IDENTIFIER,
        "CFBundleExecutable": EXPECTED_EXECUTABLE,
        "CFBundleShortVersionString": EXPECTED_VERSION,

        "LSMinimumSystemVersion": EXPECTED_MINIMUM_MACOS_VERSION,
    }
    for key, expected in checks.items():
        if info.get(key) != expected:
            fail(f"{key} is {info.get(key)!r}, expected {expected!r}")

    try:
        result = subprocess.run([str(executable), "--version"], check=True, capture_output=True, text=True)
    except (OSError, subprocess.CalledProcessError) as error:
        fail(f"--version failed: {error}")
    if not re.fullmatch(r"\d+(?:\.\d+)*", info.get("CFBundleVersion", "")):
        fail("CFBundleVersion must be numeric and increase for each release")
    expected_display = f"Thoth Browser {EXPECTED_VERSION} preview ({info['CFBundleVersion']})"
    if result.stdout.strip() != expected_display:
        fail(f"unexpected --version output: {result.stdout.strip()!r}")

    try:
        build_info = subprocess.run(
            ["xcrun", "vtool", "-show-build", str(executable)], check=True, capture_output=True, text=True
        ).stdout
    except (OSError, subprocess.CalledProcessError) as error:
        fail(f"cannot inspect the app's macOS deployment target: {error}")
    minos_values = re.findall(r"^\s*minos\s+(\d+(?:\.\d+){1,2})\s*$", build_info, re.MULTILINE)
    if not minos_values:
        fail("the app executable has no LC_BUILD_VERSION minimum OS declaration")
    if any(tuple(map(int, value.split("."))) != tuple(map(int, EXPECTED_MINIMUM_MACOS_VERSION.split(".")))
           for value in minos_values):
        fail(f"executable minimum OS is {minos_values!r}, expected {EXPECTED_MINIMUM_MACOS_VERSION}")

    resources = contents / "Resources"
    required_resources = (
        resources / "app_icon.icns",
        resources / "icons" / "thoth.png",
        resources / "icons" / "thoth-mark-light.svg",
        resources / "icons" / "thoth-mark-dark.svg",
    )
    for resource in required_resources:
        if not resource.is_file():
            fail(f"required resource does not exist: {resource}")

    source_root = Path(__file__).resolve().parents[1]
    expected_icons = (
        (source_root / "UI/Qt/Thoth/Branding/app_icon.icns", resources / "app_icon.icns"),
        (source_root / "Base/res/icons/thoth.png", resources / "icons" / "thoth.png"),
    )
    for source_icon, bundled_icon in expected_icons:
        if not source_icon.is_file():
            fail(f"generated icon source does not exist: {source_icon}")
        source_hash = hashlib.sha256(source_icon.read_bytes()).hexdigest()
        bundled_hash = hashlib.sha256(bundled_icon.read_bytes()).hexdigest()
        if source_hash != bundled_hash:
            fail(f"bundled icon is stale: {bundled_icon} does not match {source_icon}")

    if args.require_signature or args.require_developer_id:
        try:
            subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True, capture_output=True, text=True)
        except (OSError, subprocess.CalledProcessError) as error:
            fail(f"code signature verification failed: {error}")

    if args.require_developer_id:
        try:
            signature = subprocess.run(
                ["codesign", "--display", "--verbose=4", str(app)],
                check=True,
                capture_output=True,
                text=True,
            )
            details = signature.stdout + signature.stderr
        except (OSError, subprocess.CalledProcessError) as error:
            fail(f"cannot inspect the distribution signature: {error}")
        if "Authority=Developer ID Application:" not in details:
            fail("application is not signed with a Developer ID Application certificate")
        if not re.search(r"flags=.*\(.*runtime.*\)", details):
            fail("application signature does not enable the hardened runtime")
        try:
            entitlements = subprocess.run(
                ["codesign", "--display", "--entitlements", ":-", str(app)],
                check=True,
                capture_output=True,
            ).stdout
            entitlement_values = plistlib.loads(entitlements)
        except (OSError, subprocess.CalledProcessError, plistlib.InvalidFileException) as error:
            fail(f"cannot inspect application entitlements: {error}")
        if entitlement_values.get("com.apple.security.get-task-allow"):
            fail("distribution signature contains the development-only get-task-allow entitlement")

    print(f"PASS: {app.name}, version {EXPECTED_VERSION}, bundle {info['CFBundleVersion']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
