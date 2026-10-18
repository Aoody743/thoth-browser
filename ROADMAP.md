# Thoth Browser Roadmap

This roadmap is the working plan for taking Thoth Browser from the alpha2.0
frontend prototype to a browser that can be used and released with confidence.
Each stage has an explicit completion gate; passing unit tests alone does not
prove a stage that includes real browser behavior.

## Current status

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
2. Add bookmark/favourite import and export with clear ownership rules.
3. Improve history search and address-bar search engine selection.
4. Complete external links, popup windows and “open in new workspace” flows.
5. Add per-site permission visibility for camera, microphone, location and
   notifications.
6. Add explicit privacy controls for cookies, site data and browsing history.
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

## Immediate execution plan

The one-page and two-page restart slices are covered by the browser E2E gate.
Continue Stage 4 with broader session-restoration and failure cases:

1. Restore split groups and verify item identity, active pane and split ratio.
2. Restore workspaces, then cover closing and reopening a pane.
3. Cover multiple windows and prove private windows never enter the snapshot.
4. Exercise renderer crashes and failed URL restoration without losing other pages.
5. Run the model tests, ASan tests, E2E tests and smoke test as one gate.

Do not mark Stage 4 complete from model-only tests. The stage is complete only
after a real browser restart and persistence cycle has been exercised.

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
