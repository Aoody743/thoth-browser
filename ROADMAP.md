# Thoth Browser Roadmap

This roadmap is the working plan for taking Thoth Browser from the 0.9.1
frontend prototype to a browser that can be used and released with confidence.
Each stage has an explicit completion gate; passing unit tests alone does not
prove a stage that includes real browser behavior.

## Current status

### Daily-use readiness review — October 23, 2026

The desktop preview is usable for reading and organizing public pages. Treating
it as the only daily browser still needs work on interaction correctness,
resource scheduling and authentication/complex-site compatibility. The
[readiness audit below](#daily-use-readiness-audit--october-23-2026) separates
reproduced defects, source-confirmed gaps and unverified workflows.

### Focus as of October 22, 2026

Active work returns to the macOS desktop browser. The unfinished iOS/iPadOS
experiment, including its shared-engine and build-system changes, is preserved
in local branch `codex/archive-ios-ipados-2026-10-22` at `32038cfb00`. The desktop
working tree has returned to release baseline `b84f154c92`. The source chat has
also been archived. See the [archive record](Documentation/iOS-iPadOS-Archive.md)
for the port's validation limits and recovery instructions.

### Platform support

The current macOS build floor is 14.4 because it uses Qt 6.12. CMake, vcpkg,
and the app's generated `LSMinimumSystemVersion` now use the same target. The
previous executable declared 14.0 even though its QtWidgets framework requires
14.4, so the effective runtime floor was already 14.4. Qt 6.10 and 6.11 support
macOS 13, but neither reaches Catalina.

Catalina (10.15) support is out of scope for this version. It would require
replacing `QCocoaWindow::contentLayer()` (introduced in Qt 6.10), auditing
Ladybird and every bundled dependency against Catalina, choosing a maintainable
Qt version, and testing an x86_64 build on an actual Catalina machine. The
current build helper targets the host CPU, so a future Catalina-compatible
release would also need an Intel slice. Do not advertise 10.15 support without
that work.

Native Windows compilation is plausible but unverified for Thoth. The UI CMake
has a Windows renderer and Qt runtime deployment path, and the inherited
Windows preset uses Clang-CL; upstream still labels native Windows experimental
with limited runtime support. `Meta/thoth.py` and the self-contained packaging
helper are macOS-only. The upstream `Meta/ladybird.py build` route can be tried
from a Visual Studio developer prompt on a Windows host, but release support
needs a successful Windows CI build plus browser runtime coverage. WSL2 is a
supported development route, not a native Windows app. Windows is not claimed
as a supported Thoth release platform yet.

### Stages 1-3: Alpha2 foundation — complete

- Sidebar identity, folders, favourites, pinned and temporary pages.
- Workspaces, split groups, ordering and close/reopen behavior.
- Schema validation, atomic persistence and backup recovery.
- Dedicated Thoth tests, smoke checks, ASan coverage and packaging scripts.

Verified commands:

```sh
python3 Meta/thoth.py test --no-build
ctest --test-dir Build/ThothTests --output-on-failure
ctest --test-dir Build/ThothTestsASan --output-on-failure
```

The full macOS release rebuild still depends on a configured `Build/release`
directory with a working Ninja generator and a sufficiently new macOS SDK.

## Stage 4: Real-browser stability

**Goal:** prove that the sidebar model and the real `BrowserWindow` lifecycle
remain consistent during normal use and restart.

### Work items

1. Add a browser-level E2E harness for a dedicated temporary profile.
2. Cover startup, navigation, multiple pages, workspaces and split groups.
3. Close and relaunch the browser, then verify page identity, order, active
   pane, workspace and split ratio.
4. Cover closing one pane and restoring it with Command-Shift-T.
5. Cover multiple windows and window removal from the persisted snapshot.
6. Verify that private windows never write Thoth sidebar state.
7. Define behavior for renderer crashes and failed URL restoration.

### Completion gate

- The E2E suite passes against a real Thoth Browser bundle.
- A restart never duplicates a restored page.
- A failed page restore leaves the remaining snapshot usable.
- Private browsing leaves no private window data in `thoth-sidebar.json`.

## Stage 5: Daily-use browser features

**Goal:** make the Thoth shell practical for regular browsing.

### Work items

1. Unify downloads popover, downloads page and sidebar actions.
2. Connect sidebar favourites and bookmark import/export with clear ownership
   rules. `about:bookmarks` already supports HTML import and export.
3. Extend address-bar discovery to open pages and workspaces. History/bookmark
   suggestions and search engine selection already exist.
4. Complete external links, popup windows and “open in new workspace” flows.
5. Add per-site permission visibility for camera, microphone, location and
   notifications.
6. Improve access to privacy controls and add per-site data management. Global
   cache, history, download-history and site-data clearing already exists.
7. Test long URLs, long titles, narrow windows, dark mode and keyboard-only use.

### Completion gate

- A new user can browse, download, bookmark, search history and manage common
  permissions without falling back to upstream-only UI.
- All new user-facing flows have at least one automated regression test.

## Stage 6: Productization and release

**Goal:** make releases repeatable and maintainable.

### Work items

1. Add upstream sync and conflict-reporting automation.
2. Make release builds reproducible from a clean checkout.
3. Complete macOS signing, notarization and versioned artifact generation.
4. Add crash-report diagnostics and a documented support bundle.
5. Add accessibility checks for VoiceOver, keyboard navigation and larger text.
6. Define release notes, upgrade behavior and rollback guidance.

### Completion gate

- A clean checkout can produce, test and package the application using the
  documented commands.
- The packaged app passes signature, dependency, version and smoke checks.
- A tagged release has a reproducible artifact and release notes.

## Desktop feature review — October 22, 2026

The following table records the source review before this desktop iteration.
The implemented scope and validation results are recorded below it.

| Priority | Addition or improvement | Evidence and useful first slice |
| --- | --- | --- |
| P0 | Complete session and failure recovery coverage | Split/workspace snapshot restoration is tested, but UI-driven split creation, pane close/reopen, multiple windows, private-state exclusion and renderer failure still need real-browser coverage. Preserve unaffected pages when one restore fails. |
| P1 | Downloads in the sidebar | `Tab::configure_sidebar_toolbar()` reparents the downloads button under a hidden widget. The downloader, popover and `about:downloads` already exist. Add a visible progress/count entry that opens the existing popover, with pause/resume/cancel and a route to all downloads. Verify the anchor for the popover in expanded and collapsed layouts. |
| P1 | Find and switch to open pages across workspaces | The address bar already searches history/bookmarks and offers remote suggestions, but the Thoth sidebar has no page-search interface. Start with a keyboard-accessible switcher that matches title/URL/workspace and activates the correct split pane without opening a duplicate page. |
| P1 | Connect bookmarks, favourites and workspace backup | Sidebar favourites live in `SidebarModel`; bookmarks use `BookmarkStore` and already support HTML import/export. Add an explicit way to place an existing bookmark/folder in the sidebar, define rename/delete ownership, and export/import Thoth workspace organization separately with schema validation. |
| P2 | Per-site information and permission management | The permission callback in `Tab.cpp` presents an Allow/Block dialog. Settings currently expose autoplay exceptions and a global geolocation toggle. Add a current-site panel for permissions the engine actually supports, including revoke/reset, and a per-site data-clearing action. Global browsing-data clearing already exists. |
| P2 | Measured background-page resource control | Switching workspaces retains live page state; there is no Thoth user-facing page-sleep control in the sidebar. Measure memory/process use first, then offer manual sleep with a clear reload indicator. Audio, downloads and unsaved input must prevent automatic sleep. |

Separate login profiles are a later product decision: workspaces currently share
the window's browsing profile and cookies. A workspace should not imply account
isolation. Password management, extension compatibility, sync and automated AI
page actions need separate designs and broader engine/security work; they are
not part of the next small desktop iteration. A later AI experiment could start
with an explicit user request to summarize the active page and navigate back to
its source, after the core desktop flows above are reliable.

## Implemented desktop iteration — October 22, 2026

The five requested areas are implemented in the desktop working tree:

1. Expanded restart coverage to independent windows and a malformed saved URL;
   added an accessible split chooser and a native close-active-pane command.
2. Restored the download progress button to the visible sidebar navigation row,
   using the existing download popover and download-page actions.
3. Added Command-K page search across workspaces and split panes, with activation
   of the original page and wake-on-open for sleeping pages.
4. Added explicit bookmark/sidebar copy actions and versioned workspace
   export/import. Import remaps all identities and appends without replacement.
5. Added a per-origin microphone permission panel with persistence, reset and
   live capture revocation, plus scoped cookie/Web Storage clearing and manual
   background-page sleep with conservative input/media/download protection.

The macOS Thoth menu mirrors the tools for keyboard and assistive access. Core
new command labels also have Simplified Chinese translations. The
[desktop tools documentation](UI/Qt/Thoth/README.md#desktop-tools) describes the
copy ownership rules, private-session behavior and data-clearing boundaries.

Validation on this iteration:

- Full macOS browser rebuild succeeded.
- Standalone Qt tests passed 4/4, and rebuilt ASan tests passed 4/4. New cases
  cover search, workspace import identity remapping and atomic rejection,
  permission persistence/origin boundaries/private lifetime/write failures, and
  distinguishing page edits from navigation shortcuts for sleep protection.
- `python3 Meta/thoth_release_check.py --skip-package` passed, including real
  browser download/DOM checks, split/workspace restoration, independent-window
  restoration and malformed-URL fallback without losing the remaining pages.
  The multi-window restart fixture is seeded between processes because the
  inherited WebDriver new-window endpoint ignores its OS-window type hint.
- Native UI checks in disposable profiles passed for split creation, pane
  close/reopen, cross-workspace search, bookmark copy, workspace file export and
  import, microphone Block/reset persistence, real ordinary/private window
  creation and closure, exclusion of private URLs from sidebar persistence,
  and renderer-failure recovery while the other split pane remained usable.
- Native sleep checks confirmed one eligible renderer unloaded while its page
  identity/URL survived, search woke the page, and a subsequently edited draft
  was protected. The UI now reports the reasons for skipping protected pages.
- A separate self-contained local preview was generated at
  `Build/desktop-preview/Thoth Browser.app`. Packaging and the signed-bundle
  smoke check passed. It is ad-hoc signed for local use, not a newly notarized
  public release, and retains version 0.9.1.

Camera and notification grants are not implemented by the engine. The panel
labels them unsupported, and links to existing location/autoplay controls.
The per-site data action covers cookies and local/session storage; it does not
claim to clear IndexedDB or HTTP cache. Actual microphone hardware capture was
not exercised during these checks. Sleep is manual and does not serialize live
JavaScript or form state.

## Follow-up release coverage

The E2E gate now restores two pages as a split in a saved workspace and checks
page/item identities, active pane, ratio and workspace metadata. It prepares
that split by editing a temporary profile snapshot between browser runs, so
this does not prove the interactive split/workspace creation flow.

1. Automate the native acceptance cases above, including pane close/reopen and
   private-window actions, instead of treating manual acceptance as an E2E gate.
2. Add physical-device microphone acceptance and wider accessibility/appearance
   coverage before promoting this preview to a public release.
3. Run the model tests, ASan tests, E2E tests and package smoke test for each
   relevant release change.

Do not mark Stage 4 complete from model-only tests. The stage is complete only
after a real browser restart and persistence cycle has been exercised.

## Desktop update and camera preview — October 23, 2026

The 0.9.2 preview (`20261023.1`) adds macOS Sparkle updates, with a signed
archive feed hosted in this repository's GitHub Releases. Update controls include
automatic checks/downloads, a ready notification, Later and Restart and Install.
Ordinary workspace metadata is checkpointed before relaunch; active downloads
postpone restart. Packaging stages and verifies a candidate before preserving the
previous package. See `Documentation/Desktop-Updates.md` for publication and
rollback instructions. Existing 0.9.1 installations need one manual upgrade.

This iteration fixes Command-T address focus by stopping the first accessibility
tree from reclaiming focus after the address editor has it. Starting a download
no longer replaces the source document's title with the attempted navigation URL.
Restoration allocates only the active logical item, including both split panes;
eight saved entries requested one or two fixture pages in runtime acceptance.
The sidebar now exposes playing/muted pages and mute/unmute actions, Return
activation, localized controls and named accessible rows. Stable accessibility
row identities replace stale Qt tree-cell caching during list reconstruction.

macOS camera capture is connected through the page-owned IPC permission gate,
AVFoundation, MediaStream video tracks, HTMLVideoElement and Canvas frame reads.
Real-device acceptance passed first-frame events, preview pixels, pause/freeze,
disabled-track opaque black frames, cloned-track lifetime, stop and response
Permissions-Policy denial. Camera grants are rechecked by the frontend after
navigation, so renderer permission caching cannot reuse an old capture owner.
The first capture path samples frames at 10 Hz, normally 640×480. Cross-origin
camera delegation, WebRTC calling, recording and complete constraint negotiation
are not included. Media response restrictions are an explicitly limited subset
of Permissions Policy, not a claim of full policy support.

Five Qt suites pass, including stale accessibility-row queries after rebuilding
the tree. Full browser end-to-end checks cover download identity, split/workspace
and multi-window restoration. Native Chinese acceptance confirmed Command-T then
Chinese paste without Command-L, playing/mute state and visible named page rows.
The separate updater fixture downloaded a signed archive and replaced version 1
with version 2; tampering with the archive failed Ed25519 verification. Public
site login, DRM/passkeys and comprehensive VoiceOver coverage remain additional
compatibility work.

## Daily-use readiness audit — October 23, 2026

This review used the current `Build/desktop-preview/Thoth Browser.app`, fresh
profiles, local HTTP fixtures, public unauthenticated pages and the native Qt
interface. It did not use the ordinary browsing profile. Runtime artifacts and
probe scripts are in the ignored `Build/usability-audit/` directory.

### Reproduced findings and priorities

| Priority | Finding | Optimization and acceptance condition |
| --- | --- | --- |
| P0 | In the fresh Chinese-language native check, Command-T leaves focus in the new `about:blank` web area. A subsequent Command-L correctly focuses the address field. | Make address focus survive the new view's asynchronous activation. Test Command-T followed immediately by typing/paste, including Chinese text, collapsed sidebar and a current split. The first characters must arrive in the address field without an extra shortcut. |
| P0 | A Content-Disposition download keeps the original document title and URL, but persists the download URL as that page's sidebar title. | Keep the source page's identity/title/history stable when starting, finishing or cancelling a download; update only the download UI. Add a browser regression that compares the DOM, chrome and saved sidebar state. |
| P1 | Restart eagerly requested all eight pages in a simple active-workspace snapshot, including pages the user had not selected after restart. Native tab creation also used a separate WebContent/ImageDecoder pair per live page, plus a spare pair in this sample. The sidebar polls/serializes the model every 500 ms and rebuilds the whole tree on a render-state change. | Restore sidebar metadata immediately and load the active page/split first. Load the other pages on selection. Audit process allocation while preserving site isolation, prefer incremental/event-driven sidebar updates, and measure real native create/restart/sleep paths rather than relying on WebDriver tab creation alone. |
| P1 | Passkeys and WebRTC are absent in the runtime. Credential retrieval exposes an API but rejects with an explicit not-implemented error. Widevine negotiation rejects as unsupported; source supports Clear Key only. | Maintain a real-site compatibility matrix covering sign-in, editing, upload and media, and provide an explicit Open in System Browser route for unsupported tasks. Plan password/autofill integration and upstream engine work separately. Do not advertise support from API presence alone. |
| P1 | Fresh ordinary/private windows use the configured new-window page; the source default is the external `https://limestart.cn`. Settings loading also treats an explicitly saved `about:newtab` as unset. | Offer a local start page by default, respect the user's saved choice, and keep startup/private-window creation useful without network access. Align the sidebar new-page prompt, Command-T and new-window behavior. |
| P2 | Audio state/mute controls are still attached to the hidden upstream tab bar. The Thoth sidebar does not expose the corresponding speaker/mute action. | Show the playing-page indicator in the real sidebar, support mute/unmute there and make the action keyboard-accessible. Reuse the existing engine mute state. |
| P2 | A Chinese-language native site panel still shows English policy choices and explanatory text. Sidebar accessibility snapshots expose an outline without usable named page rows, with table-row errors in the runtime log. Source click handlers also need explicit keyboard activation coverage. | Complete dynamic-message and combo-box localization; add named rows and real keyboard/VoiceOver acceptance. These are audited gaps, not a claim that the entire accessibility surface has been tested. |
| P2 | The preview and existing release both identify themselves as 0.9.1 / `thoth.aoody`; there is no implemented application auto-update path in this checkout. | Give previews a visible build identity; establish signed update/rollback and upstream regression gates before a wider daily-use release. |

### Local runtime evidence

`results.json` reproduced the download-title mismatch: the live document and
WebDriver title remained `Audit /one`, while the page's saved sidebar title
became the fixture's `/download` URL. The downloaded bytes were correct.

A controlled local-only repeat (`local_restart.json`) used the same eight
simple URLs and waited for all eight actual page requests, excluding favicons:

| Path | Sum of process RSS | WebContent / ImageDecoder processes |
| --- | --- | --- |
| Eight same-origin tabs created by WebDriver, settled 3 seconds | 616.1 MiB | 1 / 1 |
| Eight restored tabs, settled 3 seconds | 1170.7 MiB | 8 / 8 |
| Eight restored tabs, settled 8 seconds | 1170.5 MiB | 8 / 8 |

A separate native Command-T/Command-L/paste run with eight active-workspace
pages sampled 1388.9 MiB RSS and 9/9 WebContent/ImageDecoder processes, including
one spare pair. That native session had earlier open/close exercises and is not
a pristine cold benchmark. The WebDriver measurements include WebDriver itself;
the native measurement does not. RSS sums double-count shared mappings and are
not unique physical RAM consumption. The process topology difference is clear,
but these samples do not prove a memory leak or justify bypassing isolation to
share processes. The useful next step is lazy restoration and a native-path
resource budget with comparable fresh-profile measurements.

Local navigation/session creation took about 1.25 seconds in the first probe;
the controlled eight-page restart reached eight handles and all eight fixture
requests in about 1.48 seconds. These are local fixture results, not real-site
interactive performance scores.

### Public pages and platform capabilities

- Example Domain, the Wikipedia Web browser article and the public Thoth GitHub
  repository loaded with document titles/content. GitHub's one cold navigation
  took about 14.6 seconds to the navigation completion condition; this is not a
  measured first paint or input-ready time. Login, editing and posting were not
  exercised, and a single load is not whole-site compatibility certification.
- Google search timed out, then inspection showed an unusual-network-traffic
  verification page. A separate network control could fetch a response. This
  search result is **inconclusive for engine compatibility**, not proof that
  Google Search is unsupported. No verification challenge was solved.
- IndexedDB passed a local write/read round trip. `navigator.credentials.get`
  rejected with an explicit not-implemented error; `PublicKeyCredential` and
  `RTCPeerConnection` were undefined. Widevine negotiation returned
  `NotSupportedError`. Notification/EME/clipboard API presence was recorded but
  is not treated as proof that their full workflows work.
- Native Unicode paste into a form field preserved `中文输入测试 abc 123`.
  This is a text round-trip check, not a complete Chinese IME composition test.

### Recommended next desktop iteration

1. Fix Command-T focus and the download/source-title mismatch, with regressions
   that exercise the native chrome and persisted sidebar state.
2. Add active-page-first lazy restoration and native-path resource measurement;
   preserve active splits, closed-item undo and private-state exclusion.
3. Make the default new-window page local and honor the user's configured start
   page. Add an explicit system-browser fallback for unsupported web workflows.
4. Broaden real-site acceptance to account login, editor/file upload, video/audio,
   PDF handling, IME and accessibility before promising a primary-browser role.
   Then prioritize password management, visible audio controls, full per-site
   storage reset and release/update polish from those results.

The standalone Qt test suite passes 4/4 in this review, and the packaged preview's
signature smoke check passes. Those checks support the existing tested slices;
they do not negate the defects or cover the untested workflows above.

## Current execution record

On October 17, 2026, the release build was configured with the full Xcode
macOS SDK, rebuilt successfully, packaged successfully and launched with
computer use. Manual verification covered:

- `about:newtab` navigation and address-bar editing.
- Navigation to `https://example.com`.
- Real page rendering and accessibility-tree exposure.
- Sidebar rendering, active-page selection and workspace controls.
- Bookmark dialog submission and bookmark toggle state.
- A local `Content-Disposition` download, the completed download entry in
  `about:downloads`, and its file in `~/Downloads`.
- The `about:history` page, including the local fixture visit and existing
  history entries.

The repeatable non-interactive pre-package gate is:

```sh
python3 Meta/thoth_release_check.py --skip-package
```

The release gate includes the Thoth Qt tests, optional ASan tests, the
WebDriver session lifecycle regression, real-browser navigation/download/
internal-page checks, a restart that restores both open pages in their saved
sidebar order, and the unpackaged bundle smoke check. Omitting `--skip-package`
also requires the generated application package and verifies its signature.

On October 18, 2026, that restart check exposed a startup placeholder being
closed after the restored page was created. The WebDriver current handle then
pointed at a closed page. The sidebar now reuses the startup view for the active
restored page, and the two-process restart check passes. On October 18, 2026,
the E2E scenario was extended to keep two distinct pages open across a browser
restart and verify both page membership and saved sidebar order. The browser
returns WebDriver window handles in a different order, so the check compares
that unordered handle set separately from the persisted sidebar item order.
Stage 4 remains in progress because split ratio/active-pane, workspace,
multi-window, private-mode and failure-recovery coverage is still outstanding.

The packaged artifact is also checked with `codesign --verify --deep --strict`.
The current alpha package uses ad-hoc signing; Developer ID signing and Apple
notarization remain release-environment steps rather than source-tree
automation.

Upstream maintenance now has a read-only report command:

```sh
python3 Meta/thoth_upstream_report.py
```

The October 17, 2026 report found 147 upstream-only commits and 17 overlapping
working-tree files requiring manual review; it does not merge or rewrite the
checkout.

On October 22, 2026, after archiving the mobile experiment, the desktop audit
passed `python3 Meta/thoth_release_check.py --skip-package`: rebuilt standalone
Qt model/container tests (2/2), existing ASan tests (2/2), WebDriver session
deletion, real-browser navigation/DOM/download/internal-page checks, split and
workspace snapshot restoration across restart, and the development-bundle smoke
check (0.9.1). The existing macOS release bundle was used; the full engine was
not rebuilt and package/signature checks were intentionally excluded by this
command. This is not a new release or proof that Stage 4 is complete.
