<p align="center">
  <img src="Base/res/icons/thoth.png" alt="Thoth Browser" width="112">
</p>

<h1 align="center">Thoth Browser</h1>

<p align="center"><em>A quieter, more considered place to browse.</em></p>

<p align="center">
  <a href="https://github.com/Aoody743/thoth-browser/releases"><img src="https://img.shields.io/badge/preview-alpha%202.0-c49b59" alt="Preview alpha 2.0"></a>
  <img src="https://img.shields.io/badge/macOS-14.4%2B-252c35" alt="macOS 14.4 or later">
  <img src="https://img.shields.io/badge/Apple%20Silicon-arm64-252c35" alt="Apple Silicon">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-BSD--2--Clause-252c35" alt="BSD 2-Clause license"></a>
</p>

<p align="center">
  <a href="https://github.com/Aoody743/thoth-browser/releases">Releases</a>
  &nbsp;·&nbsp;
  <a href="https://github.com/Aoody743/thoth-browser/issues/new">Report an issue</a>
  &nbsp;·&nbsp;
  <a href="https://ladybird.org/">Ladybird engine</a>
</p>

---

Thoth is an experimental browser for macOS, built on the
[Ladybird](https://github.com/LadybirdBrowser/ladybird) engine. Its vertical
sidebar brings workspaces and pages together in one calm, compact place so the
web can take the space it needs.

## A different way to organize the web

- **Workspaces** keep separate projects and their pages together, each with its
  own color and symbol.
- **A vertical sidebar** holds favourites, folders, pinned pages, and temporary
  pages without adding another row above the web page.
- **Split view** keeps two pages side by side under one sidebar entry.
- **Session restore** brings workspaces and open pages back after a restart.
- **A focused address bar** follows the page you are working in.

## Download

The first macOS release targets **Apple Silicon (arm64)** and **macOS 14.4 or
later**. A disk image with Developer ID signing and Apple notarization is being
prepared; it will be available from
[GitHub Releases](https://github.com/Aoody743/thoth-browser/releases) once published.

To install, open the downloaded `.dmg` and drag **Thoth Browser** into
**Applications**.

## Quick controls

| Action | Shortcut |
| --- | --- |
| Focus the address bar | `⌘L` |
| Toggle split view | `⌘⇧S` |
| Close the current page or group | `⌘W` |
| Restore the last closed page or group | `⌘⇧T` |

## Build from source

Follow the prerequisites in the
[Ladybird build instructions](Documentation/BuildInstructionsLadybird.md), then
build and run Thoth on macOS:

```sh
python3 Meta/thoth.py build -j 6
python3 Meta/thoth.py run --no-build
```

To make a self-contained local application bundle:

```sh
python3 Meta/package_thoth.py
```

The packaged application is written to `Build/alpha2.0/Thoth Browser.app`.
Local packages are ad-hoc signed for development. Public distribution requires
a Developer ID signature and Apple notarization.

## Project status

This is an early preview. Ladybird is an actively developing browser engine, so
some websites and browser features may not work as expected. This release is
for trying the interface and providing feedback.

Native Windows builds are experimental upstream and have not been verified for
Thoth. Windows packaging is not part of this macOS release.

## Credits and license

Thoth is an independent project based on the work of the Ladybird developers.
It is distributed under the [BSD 2-Clause license](LICENSE); third-party
components retain their respective licenses. Thoth Browser is not an official
Ladybird release.
