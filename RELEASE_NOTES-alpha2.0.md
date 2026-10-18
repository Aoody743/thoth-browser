# Thoth Browser alpha2.0

An early macOS preview of Thoth Browser, a workspace-first browser interface
built on the Ladybird engine.

## Highlights

- Workspaces with independent page lists, colors, and symbols.
- A vertical sidebar for favourites, folders, pinned pages, and temporary pages.
- Split view, session restoration, and recovery for saved sidebar data.
- Simplified internal pages, settings, and interface language selection.
- Updated Thoth branding throughout the macOS application.

## macOS build

- Apple Silicon (arm64)
- macOS 14.4 or later
- Bundle version: `2.0.0` (`alpha2.0`)

This alpha uses the Ladybird browser engine, which is still under active
development. Some websites and browser features may not work as expected.
Native Windows builds are not included in this release.

## Validation

The release candidate passes the project release gate covering focused model
tests, available AddressSanitizer tests, WebDriver lifecycle and navigation,
downloads and internal pages, restart restoration, and packaged application
smoke checks.

The release DMG is signed with Developer ID and notarized by Apple.

Download `Thoth-Browser-alpha2.0-macos-arm64.dmg` from the GitHub release.

SHA-256:

```text
f01d2fa64b9f1e8138f11a3a29f438b96c343a70c2874fbc0933093dcf38b65d
```

## License

Thoth Browser is an independent project distributed under the BSD 2-Clause
license. Ladybird and third-party components retain their respective licenses.
