#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Run the repeatable Thoth validation gate for an existing macOS build."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent


def run(command: list[str]) -> None:
    print("+", " ".join(command))
    subprocess.run(command, cwd=SOURCE, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=Path.home() / ".cache/thoth-browser")
    parser.add_argument("--skip-asan", action="store_true")
    package_group = parser.add_mutually_exclusive_group()
    package_group.add_argument("--package", type=Path, default=SOURCE / "Build/alpha2.0/Thoth Browser.app")
    package_group.add_argument(
        "--skip-package",
        action="store_true",
        help="skip smoke and signature verification of the packaged application",
    )
    args = parser.parse_args()

    build_root = args.build_root.expanduser().resolve()
    bundle = build_root / "Build/release/bin/Thoth Browser.app"
    if not bundle.is_dir():
        parser.error(f"release bundle does not exist: {bundle}")
    webdriver = bundle / "Contents/MacOS/WebDriver"
    if not webdriver.is_file():
        parser.error(f"WebDriver executable does not exist: {webdriver}; build target WebDriver first")
    package = None
    if not args.skip_package:
        package = args.package.expanduser().resolve()
        if not package.is_dir():
            parser.error(f"packaged application does not exist: {package}; build it or pass --skip-package")

    run(["cmake", "--build", str(SOURCE / "Build/thoth-tests"), "--parallel", "6"])
    run(["ctest", "--test-dir", str(SOURCE / "Build/thoth-tests"), "--output-on-failure"])
    if not args.skip_asan and (SOURCE / "Build/ThothTestsASan/CTestTestfile.cmake").is_file():
        run(["ctest", "--test-dir", str(SOURCE / "Build/ThothTestsASan"), "--output-on-failure"])
    run([sys.executable, str(SOURCE / "Tests/LibWebView/test-webdriver-delete-session.py"), str(webdriver)])
    run([sys.executable, str(SOURCE / "Meta/thoth_e2e_test.py"), str(webdriver)])
    run([sys.executable, str(SOURCE / "Meta/thoth_smoke_test.py"), "--app", str(bundle)])
    if args.skip_package:
        print("SKIP: packaged application smoke and signature checks")
    else:
        run([sys.executable, str(SOURCE / "Meta/thoth_smoke_test.py"), "--app", str(package), "--require-signature"])
    print("PASS: Thoth release validation gate")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
