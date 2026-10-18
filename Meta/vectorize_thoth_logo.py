#!/usr/bin/env python3
# Copyright (c) 2026, Thoth Browser contributors.
# SPDX-License-Identifier: BSD-2-Clause
"""Extract the transparent Thoth emblem SVGs from the approved artwork.

Requires Pillow, SciPy and VTracer. The traced SVG files are checked in so the
browser build does not need these image-processing dependencies.
"""
from pathlib import Path
import re
import tempfile

from PIL import Image, ImageFilter
from scipy import ndimage
import numpy as np
import vtracer

ROOT = Path(__file__).resolve().parent.parent
BRANDING = ROOT / "UI/Qt/Thoth/Branding"
OUTPUT = ROOT / "Base/res/icons"


def vectorize(variant: str, background_tolerance: int) -> None:
    source_path = BRANDING / "originals" / f"thoth-{variant}.png"
    with Image.open(source_path) as source:
        image = source.convert("RGB").filter(ImageFilter.GaussianBlur(0.25))

    pixels = np.asarray(image).astype(np.int16)
    background = pixels[0, 0]
    near_background = np.max(np.abs(pixels - background), axis=2) <= background_tolerance
    components, _ = ndimage.label(near_background, structure=np.ones((3, 3), dtype=np.uint8))
    border_components = np.concatenate((components[0], components[-1], components[:, 0], components[:, -1]))
    ids, counts = np.unique(border_components[border_components > 0], return_counts=True)
    if ids.size == 0:
        raise RuntimeError(f"Could not identify the outer background in {source_path}")
    outer_background = components == ids[counts.argmax()]
    rgba = np.dstack((np.asarray(image), np.where(outer_background, 0, 255).astype(np.uint8)))

    with tempfile.TemporaryDirectory(prefix="thoth-vector-") as temporary_directory:
        trace_input = Path(temporary_directory) / f"thoth-{variant}.png"
        trace_output = Path(temporary_directory) / f"thoth-{variant}.svg"
        Image.fromarray(rgba).save(trace_input)
        vtracer.convert_image_to_svg_py(
            str(trace_input),
            str(trace_output),
            colormode="color",
            hierarchical="stacked",
            mode="spline",
            filter_speckle=12,
            color_precision=4,
            layer_difference=16,
            corner_threshold=60,
            length_threshold=5.0,
            max_iterations=10,
            splice_threshold=45,
            path_precision=2,
        )

        traced = trace_output.read_text()

    # VTracer may emit empty paths for colors removed with the background mask.
    traced = re.sub(r'<path d=""[^>]*/>\s*', "", traced)
    paths = re.findall(r"<path\b.*?</path>|<path\b[^>]*/>", traced, re.DOTALL)
    svg = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1254 1254" '
        'role="img" aria-labelledby="title description">\n'
        f'  <title id="title">Thoth Browser emblem ({variant} theme)</title>\n'
        '  <desc id="description">A gold crescent, orbiting planets, and a stylized ibis head.</desc>\n'
        '  <g shape-rendering="geometricPrecision">\n'
        + "\n".join(f"    {path}" for path in paths)
        + "\n  </g>\n</svg>\n"
    )
    destination = OUTPUT / f"thoth-mark-{variant}.svg"
    destination.write_text(svg)
    print(f"Wrote {destination} ({destination.stat().st_size:,} bytes, {len(paths)} paths)")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    vectorize("light", 24)
    vectorize("dark", 16)


if __name__ == "__main__":
    main()
