#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Create and verify a self-contained macOS alpha application."""
import argparse
import plistlib
import re
import shutil
import subprocess
import sys

from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent
EXPECTED_BUNDLE_IDENTIFIERS = {"thoth.aoody", "org.thothbrowser.Thoth"}


def run(*command):
    return subprocess.run([str(x) for x in command], check=True, capture_output=True, text=True).stdout


def find_macdeployqt():
    candidates = [
        shutil.which("macdeployqt"),
        "/opt/homebrew/opt/qtbase/bin/macdeployqt",
        "/usr/local/opt/qtbase/bin/macdeployqt",
    ]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise RuntimeError("macdeployqt was not found; add Qt's bin directory to PATH")


def macho_files(app):
    magic = {b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xca\xfe\xba\xbf"}
    for path in (app / "Contents").rglob("*"):
        if path.is_file() and not path.is_symlink():
            with path.open("rb") as stream:
                if stream.read(4) in magic:
                    yield path


def entitlements_for(path, build_root, app):
    if path == app / "Contents/MacOS/Thoth Browser":
        return SOURCE / "Meta/Entitlements.plist"

    helper_entitlements = (
        build_root / "Build/release/Services" / path.name / f"{path.name}.entitlements"
    )
    return helper_entitlements if helper_entitlements.is_file() else None


def sign_command(identity, path, entitlements=None):
    command = ["codesign", "--force", "--sign", identity]
    if identity != "-":
        command += ["--options", "runtime", "--timestamp"]
    if entitlements:
        command += ["--entitlements", entitlements]
    command.append(path)
    return command


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, default=Path.home() / ".cache/thoth-browser")
    parser.add_argument("--output", type=Path, default=SOURCE / "Build/0.9.1/Thoth Browser.app")
    parser.add_argument(
        "--signing-identity",
        help="Developer ID Application identity; omit for a local ad-hoc signed package",
    )
    parser.add_argument("--replace", action="store_true")
    args = parser.parse_args()
    build_root = args.build_root.expanduser().resolve()
    source = build_root / "Build/release/bin/Thoth Browser.app"
    output = args.output.expanduser().absolute()
    if not (source / "Contents/MacOS/Thoth Browser").is_file():
        parser.error("Build Thoth Browser before packaging")
    if output.resolve() == source.resolve() or output.is_symlink():
        parser.error("Output must be a separate application directory")
    output = output.resolve()
    if output.exists():
        info = plistlib.loads((output / "Contents/Info.plist").read_bytes())
        if not args.replace or info.get("CFBundleIdentifier") not in EXPECTED_BUNDLE_IDENTIFIERS:
            parser.error("Output exists; use --replace only for a previous Thoth package")
        shutil.rmtree(output)
    shutil.copytree(source, output, symlinks=True,
                    ignore=lambda path, names: ["lib"] if Path(path).name == "Contents" else [])
    deploy = find_macdeployqt()
    helpers = ["-executable=" + str(p) for p in (output / "Contents/MacOS").iterdir()
               if p.is_file() and p.name != "Thoth Browser"]
    run(deploy, output, "-always-overwrite", "-no-codesign", *helpers)
    binaries = sorted(macho_files(output), key=lambda path: len(path.parts), reverse=True)
    for path in binaries:
        commands = run("otool", "-l", path)
        for rpath in re.findall(r"cmd LC_RPATH\n\s*cmdsize \d+\n\s*path (.*?) \(offset", commands):
            if rpath.startswith(("/Users/", "/opt/homebrew/")):
                run("install_name_tool", "-delete_rpath", rpath, path)
        own_ids = run("otool", "-D", path).splitlines()[1:]
        dependencies = run("otool", "-L", path).splitlines()[1:]
        for line in dependencies:
            dependency = line.strip().split(" (")[0]
            if dependency in [item.strip() for item in own_ids]:
                continue
            if dependency.startswith(("/Users/", "/opt/homebrew/")):
                raise RuntimeError(f"External dependency remains: {path}: {dependency}")
    identity = args.signing_identity or "-"
    for path in binaries:
        run(*sign_command(identity, path, entitlements_for(path, build_root, output)))
    frameworks = sorted((output / "Contents/Frameworks").glob("*.framework"),
                       key=lambda path: len(path.parts), reverse=True)
    for framework in frameworks:
        run(*sign_command(identity, framework))
    run(*sign_command(identity, output, SOURCE / "Meta/Entitlements.plist"))
    run("codesign", "--verify", "--deep", "--strict", "--verbose=2", output)
    if args.signing_identity:
        signature = subprocess.run(
            ["codesign", "--display", "--verbose=4", output],
            check=True,
            capture_output=True,
            text=True,
        )
        details = signature.stdout + signature.stderr
        if "Authority=Developer ID Application:" not in details:
            raise RuntimeError("The selected identity is not a Developer ID Application certificate")
        if not re.search(r"flags=.*\(.*runtime.*\)", details):
            raise RuntimeError("The app signature is missing the hardened runtime")
    print(run(output / "Contents/MacOS/Thoth Browser", "--version").strip())
    signature_kind = "Developer ID signed" if args.signing_identity else "ad-hoc signed for local use"
    print(f"Verified self-contained {signature_kind} application: {output}")


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        print(error.stderr, file=sys.stderr)
        sys.exit(error.returncode)
