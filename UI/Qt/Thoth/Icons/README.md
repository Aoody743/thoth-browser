# Browser UI icons

Lucide (`lucide-static` 1.61.0), https://lucide.dev/ and https://github.com/lucide-icons/lucide.
The upstream ISC license and complete asset manifest are bundled in the application resources.

The checked-in SVGs are library assets, not generated icon geometry. PNG alpha masks are rasterized from those SVGs with resvg 2.6.2 at 96px. Qt applies theme colors and scales them for 1x/2x/3x displays; it does not draw custom paths for the icons. Fine/regular/expressive variants change only the library SVG stroke width. `star-filled` uses the upstream star with a currentColor fill to distinguish saved bookmarks.

Regenerate with the pinned command documented in `Meta/import_lucide_icons.cjs`, then run that script from the repository root. Runtime code uses bundled assets and requires no network or SVG plugin.

Website favicons and existing Thoth brand artwork are separate identity assets. Native macOS window controls remain system provided.
