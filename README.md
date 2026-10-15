# Thoth Browser

An experimental desktop browser project based on [Ladybird](https://github.com/LadybirdBrowser/ladybird), exploring a [Zen Browser](https://github.com/zen-browser/desktop)-inspired interface.

## Current status

This repository is at the initial setup stage. It contains Ladybird's source code and full default-branch commit history. The application currently uses Ladybird's existing interface and branding; the Thoth-specific interface has not been implemented.

The intended first milestone is a macOS prototype with:

- A refined vertical tab sidebar.
- Workspaces and pinned tabs.
- Split-view browsing.
- Session persistence.
- A tested process for following upstream Ladybird updates.

These are planned features, not capabilities added by this repository yet. Zen is a design reference; no Zen source code has been imported.

## Build and run

Follow the retained [Ladybird build instructions](Documentation/BuildInstructionsLadybird.md). The upstream source overview is preserved in [README.upstream.md](README.upstream.md).

## Upstream development

This is an independent GitHub repository. Its relationship to Ladybird is recorded in Git history and documentation rather than GitHub's fork network.

- `main`: Thoth Browser development.
- `ladybird-upstream`: the unmodified Ladybird revision used to initialize the project.
- `upstream` Git remote: `https://github.com/LadybirdBrowser/ladybird.git`.

See [UPSTREAM.md](UPSTREAM.md) for the imported revision and update procedure. Automated synchronization and Thoth-specific CI have not been configured. GitHub Actions are initially disabled so inherited upstream workflows do not run unintentionally.

## Credits and license

Thoth Browser builds on the work of the Ladybird developers and is released under the [BSD 2-Clause license](LICENSE). Existing upstream copyright notices are retained, and Thoth-specific contributions are covered by the project copyright notice. Third-party dependencies retain their respective licenses.

Thoth Browser is an independent project and is not an official Ladybird or Zen Browser release.
