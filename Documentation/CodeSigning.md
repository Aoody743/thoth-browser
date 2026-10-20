# Code signing

Thoth Browser currently publishes unsigned Windows binaries. Unsigned installers
trigger reputation-based warnings (Windows SmartScreen) and cannot prove that a
download came from this repository, so we are applying for a free open-source
code signing certificate.

## Status

- Application target: [SignPath Foundation](https://signpath.org/apply), which
  provides free Authenticode signing for OSS projects through
  [SignPath.io](https://about.signpath.io) and a certificate issued to the
  SignPath Foundation.
- Application: prepared, not submitted yet.
- Windows artifacts in the current release: unsigned. SHA-256 checksums are
  published with each release so downloads can be verified.

## Project facts for the application

| Field | Value |
| --- | --- |
| Project | Thoth Browser |
| Repository | <https://github.com/Aoody743/thoth-browser> |
| Website | <https://github.com/Aoody743/thoth-browser> |
| License | BSD 2-Clause (`LICENSE`) |
| Languages | C++23, Rust, Qt 6 |
| Maintainer | [Aoody743](https://github.com/Aoody743) |
| Time zone | Asia/Hong_Kong |
| Release cadence | Tag-driven GitHub releases (macOS DMG, Windows installer) |
| Artefacts to sign | `ThothBrowser-*-win64-setup.exe`, `bin/Thoth.exe`, `bin/WebContent.exe`, `bin/RequestServer.exe`, `bin/Compositor.exe`, `bin/WasmCompiler.exe` |

Thoth Browser is a fork of the Ladybird browser engine. Ladybird is BSD-2-Clause
and is published from <https://github.com/LadybirdBrowser/ladybird>; Thoth adds
the workspace-first user interface, the Qt shell integration, and the Windows
build. Only binaries built from this repository are submitted for signing.

## Release workflow

- macOS: `Meta/thoth.py` / `Meta/package_thoth_dmg.py`, signed with Developer ID
  and notarized by Apple.
- Windows: MSVC 2022 Build Tools with clang-cl/lld-link, CMake, Ninja, vcpkg and
  prebuilt Qt 6.10.3; packaged with `package_windows.ps1` and NSIS
  (`installer/windows/ThothBrowser.nsi`).
- Build documentation: [WindowsBuild.md](WindowsBuild.md).

SignPath verifies that signed artifacts are automated builds of the repository,
so a GitHub Actions workflow for the Windows build is a prerequisite for
signing. The signing step will use the SignPath GitHub Action once a project,
signing policy and certificate handle are assigned.

## Team roles

- Committers and reviewers: [Aoody743](https://github.com/Aoody743)
- Approvers for signing requests: [Aoody743](https://github.com/Aoody743)

## Alternatives considered

- [SignPath Foundation](https://signpath.org/): free, cloud HSM, no hardware
  token, requires an OSS application and CI-verified builds. Preferred.
- [Certum Open Source Code Signing](https://certum.eu/): free certificate for
  OSS projects, but requires identity/project verification and either a
  cryptographic card or a paid cloud signing subscription.
- [Azure Trusted Signing](https://learn.microsoft.com/azure/trusted-signing/):
  cheap (~US$10/month) managed signing with identity verification; not free.

## Privacy

Thoth Browser does not collect telemetry. Any network access is made on behalf
of the user's browsing and downloads, or to fetch the optional content-blocker
list. This program will not transfer any information to other networked systems
unless specifically requested by the user or the person installing or operating
it.
