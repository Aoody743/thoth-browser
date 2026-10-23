# Desktop update and regression workflow

Thoth 0.9.2 previews display a separate, increasing build number. The initial
build is `2610.23.1` (`YYMM.DD.sequence`, within Apple bundle-version component limits). Set `THOTH_BUNDLE_VERSION` for every subsequent published
build; changing the marketing version alone is not sufficient.

macOS uses Sparkle 2.10.0. Its distribution download is pinned by SHA-256 in
`Meta/CMake/ThothUpdater.cmake`; an existing distribution can be supplied with
`THOTH_SPARKLE_ROOT` for offline builds. Sparkle is embedded in the app, including
its installer and XPC services. Its upstream license is bundled as `Contents/Resources/Sparkle-LICENSE`.

The Browser menu and the native Help menu expose Updates. Automatic checking
and downloading default to enabled and remain user-configurable. Downloaded
updates are verified before extraction. When an update is ready, the browser
presents Restart and Install Update and Later; the menu retains the restart
command. Ordinary file downloads postpone restarting. Ordinary workspace state
is saved before relaunch. Private page state is never persisted. JavaScript and
unsaved forms are not serialized; application update does not create a full
in-memory browsing checkpoint.

## Publishing

The stable feed URL is:

`https://github.com/Aoody743/thoth-browser/releases/download/updates/appcast.xml`

The `updates` release hosts the feed; versioned preview releases host immutable
archives. Keep those archives available for existing clients. Upload and verify
the versioned archive before replacing the public feed. The first updater-enabled
preview must be installed manually because older builds do not include an updater.

Configure the build with `-DTHOTH_UPDATE_FEED_URL=<HTTPS URL>` (or an empty value
for a development build without network update checks). `THOTH_UPDATE_PUBLIC_KEY`
is the Ed25519 public key; its private counterpart is in the signing machine's
login Keychain under the Sparkle account `thoth.aoody`. Never export it into the
source tree or publish it. A replacement key requires a planned key rotation.

```sh
python3 Meta/thoth.py build -j 6
python3 Meta/package_thoth.py --signing-identity 'Developer ID Application: …'
python3 Meta/thoth_release_check.py --skip-asan
python3 Meta/thoth_updates.py \
  --app 'Build/0.9.2/Thoth Browser.app' \
  --output Build/updates \
  --download-prefix 'https://github.com/Aoody743/thoth-browser/releases/download/<version-tag>/'
```

Notarize the signed application/archive using the release operator's stored
notarytool profile and staple the result before generating the final archive and
appcast. The preparation script verifies the bundle signature and public-key
match and delegates archive signatures to Sparkle's `generate_appcast`. It does
not upload anything. Preserve a copy of each previously published appcast.

`package_thoth.py --replace` prepares a separate staging app, verifies it, then
retains the existing package as `Thoth Browser.previous.app`. It refuses to
overwrite an existing previous package. A failed staging/signing step leaves the
original app in place. This backup is a local packaging backup, not a promise of
an automatic rollback of every installed browser.

For emergency rollback, stop advertising a broken archive and publish the last
known-good source as a new, higher build number with fresh signatures. Sparkle
must not automatically downgrade to an older build. For local recovery, quit the
browser and reinstall a previously verified, signed release. Back up the browser
profile before a downgrade across data-schema changes.

## Required checks

- Qt model, split-container, site-policy and sleep-input tests.
- `thoth_smoke_test.py`: version/build identity, resources and bundle checks.
- `thoth_e2e_test.py`: navigation/history, completed download bytes, source
  document/sidebar title preservation, split and workspace restoration.
- `thoth_restore_test.py`: eight saved entries load only the current page or
  the two current split panes; sidebar metadata is retained.
- Build and run the `TestCameraConstraints` target: native format selection,
  exact/min/max/ideal handling, arbitrary downsampling, advanced sets and 8K modes.
- `thoth_camera_test.py`: invalid constraints, remembered denial and response
  Permissions-Policy restrictions. Run `--hardware` separately to test real
  preview/Canvas pixels, pause, disabled-track black frames, cloned track
  lifetime and stop, actual 480p/720p/1080p and arbitrary smaller output, live
  constraint changes, capability reporting and unsupported constraint rejection.
  This requires explicit local camera authorization.
- `thoth_update_fixture.py`: the production updater in a disposable native app,
  with version 1 installed and a signed version 2 served locally. Verify the
  ready notification, Later, restart command, replacement and relaunched version.
  Test tampered archives separately: their Ed25519 signature must fail validation.

For every preview record a public-site regression matrix: Example Domain,
Wikipedia, GitHub, search, form/IME entry, upload/download, audio/mute, camera,
permission revocation and native keyboard/accessibility behavior. Login, WebRTC,
DRM and passkeys require separate evidence. Camera preview does not establish
support for WebRTC calls; cross-origin camera delegation remains unavailable.

## Camera resolution preview: 2610.23.2

The signed application passed 11 camera constraint unit tests and local hardware
acceptance at 640×480, 1280×720, 1920×1080 and 853×479. Both enumerated local
camera sources delivered 1920×1080. The default camera produced non-black pixels.
15fps configuration, invalid required constraints, Canvas reads, paused preview,
disabled tracks, clone lifetime, stopping, camera enumeration privacy and response
Permissions-Policy checks passed. No camera images are saved by the fixture.
The five Qt suites and the desktop navigation/download/window regression gate passed.
Native 8K selection is covered by a synthetic format test, not 8K hardware acceptance.

## Close-control alignment: 2610.23.3

macOS window controls now retain AppKit's standard frames. The previous observer
moved each button after native titlebar layout, which could separate its visual
button from hover glyph placement. Window resizing and fullscreen transitions
now use the system layout without deferred per-button offsets.

Sidebar close controls use one viewport rectangle for both painting and pointer
hit testing. The former full-row-height strip beyond the visible button no
longer closes a page. Geometry regression tests cover narrow, nested, scrolled
and split rows; all six Qt suites pass.
