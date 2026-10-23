# iOS / iPadOS port archive

Archived on October 22, 2026 at the user's request. Active development returns
to the macOS browser. This is an unfinished experiment, not a supported release.

## Preserved work

- UIKit iPhone/iPad shell in `UI/iOS`, backed by the Ladybird engine.
- iOS device and arm64 simulator vcpkg triplets, dependency overlays and CMake
  integration.
- Experimental in-process engine services, Darwin thread/IPC changes, CoreText
  font fallback and thread-local service state.
- All working-tree changes present when the port was archived, including the
  dependency-manifest changes made during the experiment.

The archive branch is `codex/archive-ios-ipados-2026-10-22`, based on macOS
release commit `b84f154c92`. The archive is local; it has not been published.

## Last known validation

The simulator target compiled and linked. Prior launch checks encountered
event-loop initialization, thread identity and font-plugin/service assertions;
fixes were attempted. The last edit removed early font-plugin installation from
the embedded services, and its Xcode build completed successfully. Successful
foreground display, page rendering, text input and general browsing were not
verified after that edit. There is no verified working iPhone/iPad release,
physical-device acceptance result or App Store submission in this archive.

The source chat is `codex://threads/01a15f4f-c9cb-75a0-a7a7-fca06cef1ae5`
(“移植 Ladybird 浏览器到 iOS”). The decision to archive follows the user's
current distribution priorities; this record is not an independent assessment
of App Store eligibility.

## Existing local build artifacts

These generated files remain outside Git and were not deleted or rebuilt:

- `~/.cache/thoth-browser/Build/ios-simulator`
- `Build/ios-probe`
- `Build/ios-simulator`

Their presence is not a reproducibility or runtime-success guarantee. The most
recent simulator build command in the source chat was:

```sh
xcodebuild -quiet \
  -project "$HOME/.cache/thoth-browser/Build/ios-simulator/ladybird.xcodeproj" \
  -target ladybird -configuration Release -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO
```

To resume intentionally, check out the archive branch in a separate directory
and revalidate toolchains, signing and runtime behavior. Do not merge its shared
engine changes into the desktop baseline without review.
