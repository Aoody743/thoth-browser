# Thoth Browser 0.9.2 preview

The Qt shell uses a left sidebar with the address editor and navigation controls,
a Favourite icon grid, nested folders and pinned pages, and temporary pages.
There is no horizontal tab strip or bookmarks toolbar in ordinary windows. The
native macOS menu and bookmark data remain available.

On native Windows, the address editor and navigation controls move into a
full-width toolbar above the sidebar and page area. Minimize, maximize and close
controls sit at the right end when client-side window decorations are enabled.

## Pages and split groups

Command-Shift-S opens a chooser for a second existing or new page. A split owns
exactly two real renderer pages but occupies one sidebar row, with a chip for each
pane. Click a chip or page to route the shared address editor and browser commands.
Drag the divider to resize; the group menu can swap or separate the pages.

Command-W closes the whole logical item; Command-Shift-T restores a closed item
with its grouping. A pane chip's context menu can close just that pane. Closing a
pinned item unloads its renderer while retaining its sidebar entry. Unpinning moves
it to Temporary. Closing the last temporary item leaves a fresh New Tab window.
Control-Tab cycles logical items. Command-L expands the sidebar and edits the
active pane's URL. Hold a navigation button to inspect its history.

Right-click a page to pin it, add a single page to Favourites, or move it into a
folder. One Browser button in the footer opens the browser commands, appearance
options, page organization and workspace actions. Drag items to reorder or drop
onto a folder to nest; drop a page onto the middle of another page row to create
a split. Section names are not shown: a thin divider separates pinned items from
temporary pages. Empty Favourites take no space. They appear as a drop target
during a drag and remain visible after the first item is added; there is no add
icon. Folder rows use a chevron without a folder icon.

## Workspaces

The workspace name below Favourites opens its settings. The bottom dock switches
spaces; its plus button creates one. Spaces support renaming, six restrained color
palettes plus a custom color picker, four vector symbols and reordering. Each
workspace's color is visible in its dock icon. Removing a space transfers its pages
to another space, and the last space cannot be removed.

## Appearance and personalization

The browser UI has its own appearance controls in Settings. The first palette
set offers Lavender, Ocean, Sage, Rose, Sand, Graphite, Plum and Terracotta;
the same accent adapts for light and dark appearance. Interface typefaces include
the system font, Bricolage Grotesque, Space Grotesk and Fraunces. The icon setting
changes the weight of Thoth's vector chrome icons. These choices affect browser
controls and the sidebar only; websites keep their own styling. The three bundled
typefaces are distributed under SIL Open Font License 1.1, with their license
texts beside the font files.

The ongoing personalization goal is to make the browser shell feel like the
user's own space: broaden the typeface and icon choices, make palettes easy to
preview, and keep the choices available from one clear Settings section.

## Desktop tools

The macOS **Thoth** menu and the sidebar's Browser menu expose these tools:

- **Find Open Page** (`Command-K`) searches page titles, URLs and workspace
  names, including inactive workspaces and both panes of a split. Enter switches
  to the existing page; it does not create a duplicate. Opening a sleeping page
  reloads it.
- **Downloads** appears alongside the sidebar navigation controls after a
  download is added. It uses the existing progress/popover and downloads-page
  backend, including pause, resume and cancel. Private and ordinary downloads
  remain separate.
- **Copy Bookmarks to Sidebar** copies a bookmark or nested folder into the
  current workspace, or a single bookmark into Favourites. **Save Copy to
  Bookmarks** in a page's context menu performs the reverse operation. These are
  independent copies: rename/delete does not propagate. HTML bookmark import
  and export remain available in `about:bookmarks`.
- **Export/Import Workspaces** uses versioned JSON, includes grouping and
  workspace metadata, and omits closed-page history. Import appends fresh IDs
  without replacing the current window's pages or selection. Imported pages
  load on demand. The file limit is 8 MiB. Export is disabled in private windows.
- **Site Settings** stores microphone Ask/Allow/Block decisions per HTTP(S)
  origin and profile. Reset/revoke updates the AudioServer permission and reloads
  this origin's open pages. Private decisions are memory-only and disappear
  after the private session's tabs are destroyed. On macOS, camera permission
  also gates AVFoundation capture and local MediaStream video preview; notifications
  remain unsupported; location/autoplay controls link to existing Settings.
  The site-data action clears cookies matching the current host (including
  shared-domain cookies) and this origin's local/session storage. It does not
  claim to clear IndexedDB or HTTP cache.
- **Sleep Background Pages** unloads eligible renderers while preserving their
  sidebar identities, URLs and order. Sleeping pages are marked in the sidebar
  and search. Current pages/splits, loading or crashed pages, active audio,
  microphone access, page interaction, editing history and unload handlers are
  protected. Active downloads prevent sleep. Navigation shortcuts and Escape
  do not count as editing. The result lists why pages were protected. Sleep is
  manual; JavaScript and form state are not serialized.

Split creation has a keyboard-accessible chooser. **Close Active Split Pane**
closes one side; `Command-Shift-T` rejoins the restored pane to its group.

For native acceptance checks in a temporary profile with local fixture pages:

```sh
python3 Meta/thoth_desktop_fixture.py \
  "$HOME/.cache/thoth-browser/Build/release/bin/Thoth Browser.app"
```

This creates its own profile and server, and cleans them up when the browser
exits. It never selects the ordinary browsing profile.

Dragging the sidebar divider to its minimum width collapses it. The expanded
sidebar has no separate collapse button. In the collapsed state, a narrow grip
remains at the left edge; hovering there reveals the sidebar as a floating
overlay, and moving back to the page hides it again. Click the grip or press
Command-Shift-L to expand or collapse the sidebar.

Favourites are shared across spaces in each window. Folders, pinned items and
temporary items belong to a single space; a split moves as a unit. Use the item's
context menu or drag it onto a dock icon to move it. Each space remembers its last
active item. Command-Option-Left/Right cycles spaces; Command-Option-1 through 9
selects directly. A horizontal swipe over the sidebar also cycles spaces.

Each window keeps its own organization. Spaces share that window's browsing
profile and cookies; they are not isolated login profiles. Live pages retain their
state while switching; inactive spaces load on demand after restart. Model schema
2 migrates the previous single-space schema without discarding page identities.

## Persistence and upstream boundary

`SidebarModel` owns UUIDs, folders, grouping, order and closed-item snapshots;
`Sidebar` maps them to the existing Qt `Tab` and `SplitView` objects. The hidden
upstream tab bar remains an internal flat-page adapter for existing window APIs.
No rendering-engine or helper-process protocol changes are needed for alpha2.

Profile-local `data/thoth-sidebar.json` stores versioned window snapshots using
atomic replacement and a previous-valid `.bak`. It saves URLs, titles, favicons,
active panes, split ratios, sidebar width, folder expansion and workspace metadata. Restart loads only the current page or both current split panes; other entries
retain their titles and URLs and load on selection; in-page JavaScript and form values are not restored.
Temporary pages survive restart and have no timed expiry. Private windows never
write sidebar state. Existing alpha1 recovered pages migrate as flat temporary
items; bookmarks and the upstream database remain intact.

## Build

From the repository root:

```sh
python3 Meta/thoth.py build -j 6
python3 Meta/thoth.py run --no-build
```

Builds use `~/.cache/thoth-browser/Build/release` by default because upstream
OpenSSL scripts do not support spaces in build paths. Source files stay in this
workspace. Override the cache root with `--build-root /path/without/spaces`.
The wrapper uses the installed Xcode SDK by default; override it with `--sdk`.
The selected SDK must define `VM_MEMORY_APPLICATION_SPECIFIC_2`, which is absent
from this machine's older Command Line Tools SDK.

The macOS executable is `~/.cache/thoth-browser/Build/release/bin/Thoth Browser.app/Contents/MacOS/Thoth Browser`.
The displayed version includes `0.9.2 preview` and a separate increasing build number, initially `2610.23.1`.
Profiles use `Thoth Browser/Profiles`, separate from Ladybird's profiles.
The underlying engine user-agent remains unchanged for site compatibility.

## Model and container tests

These test the actual split container without compiling the engine:

```sh
cmake -S UI/Qt/Thoth/Tests -B Build/thoth-tests -G Ninja \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qtbase
cmake --build Build/thoth-tests
ctest --test-dir Build/thoth-tests --output-on-failure
```

They cover split visibility/resizing and model identity, nested folders, reordering,
pinning, grouped/pane closure, undo, serialization, atomic saves and backup recovery. Real webpage rendering, native focus, input methods and session behavior
require validation in the full browser. Use normal startup for visual checks:
`--test-mode` intentionally replaces generic web fonts with the engine's test font.

## Branding assets

The paired `Base/res/icons/thoth-light.png` and `thoth-dark.png` files use the
new vectorized emblem on light and dark rounded backplates. Original artwork
stays in `Branding/originals/`, and the rasterized vector marks used by the icon
generator stay in `Branding/marks/` (export each `thoth-mark-*.svg` with
`sips -s format png` before running the generator). `Branding/GenerateIcon.cpp`
uses a full-size rounded backplate and insets the emblem to 78% of the canvas.
The normal Qt build regenerates the PNGs and macOS icon from the mark sources
before compiling UI resources and copying the app icon, so rebuilt bundles use
the same artwork. Run it from the repository root when preparing the sources.

## Package and validation

```sh
python3 Meta/package_thoth.py
# Replace a previous generated package:
python3 Meta/package_thoth.py --replace
```

The self-contained result is `Build/0.9.2/Thoth Browser.app`. Packaging copies
Qt and engine dependencies, removes build-machine library search paths, signs the
application and verifies its nested signatures and version output.

Validation uses `TestProfile`, `TestFileDownloader`, `ThothSplitView` and
`ThothSidebarModel`, plus native application checks on a separate QA profile.
The package is intended for 0.9.2 preview validation and is separate from any upstream Ladybird build.

## Updates and camera acceptance

Browser → Updates offers automatic checking/downloading and Restart and Install
Update. macOS packages embed Sparkle and verify signed updates before extraction.
The first updater-enabled preview must be installed manually. See
[the publishing, recovery and regression workflow](../../../Documentation/Desktop-Updates.md).

The sidebar exposes playing/muted pages with keyboard-focusable mute controls;
the page menu provides the same action per split pane. Return activates a selected
page. Native accessibility rows resolve current model IDs instead of retaining
QTreeWidgetItem pointers across rebuilds.

Camera acceptance scripts use temporary profiles and a localhost fixture. Run
`Meta/thoth_camera_test.py <WebDriver>` for denial/constraint/policy checks; use
`--hardware` only when local camera capture is intended. Hardware acceptance
covers video metadata, Canvas pixels, pause, disabled-track black frames, clones,
stopping, and live changes through 640×480, 1280×720, 1920×1080 and arbitrary
smaller dimensions. `--hardware --allow-site-camera` explicitly remembers the
localhost camera grant in the disposable test profile; macOS permission still applies.

Camera formats and frame-rate ranges are enumerated from AVFoundation rather
than a fixed preset. Resolution, aspect ratio, frame rate, device ID and resize
mode honor exact/min/max and ideal constraints, including ordered advanced sets.
Native output is preferred where possible; crop-and-scale supports smaller output
sizes without upscaling. Defaults prefer 1080p30, falling back to actual device
capabilities. `getCapabilities()`, `getSettings()`, `getConstraints()` and
`applyConstraints()` expose and change the camera configuration. A successful
change waits for a frame of the requested size. Unsupported required constraints
reject without interrupting the existing capture. Native frame allocation is
bounded to 256 MiB (including stride), supporting native 4K/8K dimensions.

This does not provide RTCPeerConnection/WebRTC calling. Cross-origin frame camera
delegation, recording and other device controls remain separate engine work.
