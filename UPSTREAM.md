# Following Ladybird upstream

## Initial import

- Source: https://github.com/LadybirdBrowser/ladybird
- Source branch: `master`
- Imported revision: [`86a57aa4b0194823c6f2c8ee5b9a473c6e03e19f`](https://github.com/LadybirdBrowser/ladybird/commit/86a57aa4b0194823c6f2c8ee5b9a473c6e03e19f)
- Import date: 2026-10-15
- GitHub repository: https://github.com/Aoody743/thoth-browser

The import preserves the complete default-branch history. The `ladybird-upstream` branch points to this unmodified initial revision; `main` adds Thoth project documentation, application branding and the experimental Qt split-view frontend.

## Local setup

```sh
git clone https://github.com/Aoody743/thoth-browser.git
cd thoth-browser
git remote add upstream https://github.com/LadybirdBrowser/ladybird.git
git fetch upstream
```

The local checkout used to initialize the repository already has both remotes configured.

## Manual update procedure

Start with a clean working tree. Integrate updates on a separate branch so `main` can keep a known working revision:

```sh
git fetch upstream
git switch main
git switch -c integrate-ladybird-update
git merge upstream/master
```

Resolve conflicts, build the application, and check navigation, tabs, split views, input methods, and session restore as those features are implemented. Merge the integration branch into `main` after validation.

To advance the unmodified upstream reference on GitHub:

```sh
git push origin refs/remotes/upstream/master:refs/heads/ladybird-upstream
```

Future automation should perform this integration and validation process. Automatic synchronization, continuous integration, and application auto-updates are not implemented by the initial repository setup.

## Keeping updates manageable

Keep Thoth-specific UI changes focused, preserve upstream engine and helper-process code where possible, and update the engine and its helper processes together. Retain upstream copyright and license notices.

## Current Thoth integration seams

- `UI/Qt/Thoth/`: split container, its tests, branding assets and frontend notes.
- `UI/Qt/TabBar.*` and `BrowserWindow.cpp`: tab ownership, active-pane routing and split actions.
- Qt and macOS build resources: application name, bundle identifier, icon and displayed version.
- `LibWebView` profile/crash-report paths and product-facing strings: separate Thoth storage and identity.
- `Meta/thoth.py` and `Meta/package_thoth.py`: developer builds and self-contained macOS packaging.
- `FileDownloader.cpp`: skip the default-directory startup sweep when no persisted download directories exist; covered by `TestFileDownloader`.

Rendering, JavaScript, network behavior and helper-process protocols have not been
changed for this alpha. The frontend still uses upstream internal APIs; updates can
require adapting these integration seams. A successful Git merge alone is not a
compatibility check.
