"""Verify the recovered 640x480 SHAPED main-screen pixel geometry."""

import sys

from PIL import Image


path = sys.argv[1]
image = Image.open(path).convert("RGB")
errors = []

if image.size != (640, 480):
    errors.append(f"client is {image.width}x{image.height}, expected 640x480")


def expect(x, y, colour, description):
    actual = image.getpixel((x, y))
    if actual != colour:
        errors.append(f"{description} at ({x},{y}) is {actual}, expected {colour}")


if image.size == (640, 480):
    # Original editor: 560x480 workspace, 80x480 button strip.
    expect(559, 200, (0, 130, 130), "workspace right divider")
    expect(280, 200, (0, 130, 130), "upper view divider")
    expect(200, 240, (0, 130, 130), "left view divider")
    expect(300, 300, (0, 0, 0), "lower-right status field")

    # Three 280x240 grids, ten-pixel minor pitch, centered red axes.
    expect(10, 30, (0, 130, 0), "minor grid line")
    expect(15, 25, (0, 0, 0), "grid cell interior")
    expect(140, 100, (255, 0, 0), "front vertical axis")
    expect(100, 120, (255, 0, 0), "front horizontal axis")
    expect(420, 100, (255, 0, 0), "side vertical axis")
    expect(140, 360, (255, 0, 0), "top-view vertical axis")

    # Twenty-four exact 80x20 rows in the original right-hand button strip.
    expect(639, 19, (0, 255, 255), "first button bottom/right border")
    expect(600, 20, (0, 255, 255), "first button bottom border")
    expect(639, 479, (0, 255, 255), "last button bottom/right border")
    expect(630, 18, (0, 0, 255), "button fill")

    # The binary's static button flags drive the original empty-shape colours.
    enabled = "CCCCRRRRRRRCRRCRCRRRCCCC"
    for row, state in enumerate(enabled):
        pixels = [
            image.getpixel((x, y))
            for y in range(row * 20 + 1, (row + 1) * 20)
            for x in range(561, 639)
        ]
        wanted = (0, 255, 255) if state == "C" else (255, 0, 0)
        if wanted not in pixels:
            errors.append(f"button row {row} does not contain its recovered {'enabled' if state == 'C' else 'disabled'} text colour")

    # Fastgraph ASCII strike and the original 275x19 status field.
    title_cyan = sum(
        image.getpixel((x, y)) == (0, 255, 255)
        for y in range(20)
        for x in range(560)
    )
    if title_cyan != 1505:
        errors.append(f"embedded Fastgraph title mask has {title_cyan} cyan pixels, expected 1505")
    expect(282, 431, (0, 0, 255), "status field upper-left")
    expect(556, 449, (0, 0, 255), "status field lower-right")
    expect(557, 449, (0, 0, 0), "status field right margin")

if errors:
    print("SHAPED layout verification failed:")
    for error in errors:
        print(f"- {error}")
    raise SystemExit(1)

print("SHAPED layout verified: 640x480, 560x480 workspace, 3x 280x240 views, 24x 80x20 sidebar")
