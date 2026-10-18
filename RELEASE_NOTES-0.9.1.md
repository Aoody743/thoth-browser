# Thoth Browser 0.9.1

An updated macOS preview of Thoth Browser, a workspace-first browser interface
built on the Ladybird engine.

## What's new

- macOS system proxy chains are now respected. Safe requests retry the next
  configured proxy after a connection failure, without silently switching to a
  direct connection.
- Proxy connection attempts fail over sooner, and page cards show a rotating
  loading outline while navigation is in progress.
- Workspace symbols remain transparent vector icons, with no filled selection
  background behind the active icon.

## macOS build

- Apple Silicon (arm64)
- macOS 14.4 or later
- App and bundle version: `0.9.1`

This alpha uses the Ladybird browser engine, which is still under active
development. Some websites and browser features may not work as expected.
Native Windows builds are not included in this release.

## Validation

Proxy configuration and macOS proxy mapping tests pass. A local request-server
test confirms that a failed primary proxy retries through the configured
fallback proxy. The project release gate passes focused tests, AddressSanitizer
checks, WebDriver lifecycle and navigation, downloads and internal pages,
restart restoration, and app smoke checks.

The release DMG is signed with Developer ID and notarized by Apple.

Download `Thoth-Browser-0.9.1-macos-arm64.dmg` from the GitHub release.

## License

Thoth Browser is an independent project distributed under the BSD 2-Clause
license. Ladybird and third-party components retain their respective licenses.
