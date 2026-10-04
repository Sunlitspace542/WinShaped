"""Direct-runnable command-line frontend for quick SHAPED export tests.

Run either ``python python/shaped/frontend.py`` from the repository or the
installed ``shaped-frontend`` command. It has no GUI or third-party dependency.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

if __package__:
    from .core import FormatError, load, load_colour_tables, write
else:  # Supports: python python/shaped/frontend.py
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from shaped.core import FormatError, load, load_colour_tables, write


FORMATS = {
    "gzs": ".asm",
    "bsp": ".asm",
    "internal": ".3dcg",
    "3dg1": ".3dg",
    "twist": ".txt",
}


def _summary(model: object) -> str:
    active = sum(model.active(index) for index in range(len(model.dots)))
    groups = sorted({bit + 1 for polygon in model.polygons for bit in range(8) if polygon.flags & (1 << bit)})
    return (
        f"Dots: {len(model.dots)} ({active} active)\n"
        f"Polygons: {len(model.polygons)}\n"
        f"Frames: {model.frame_count}\n"
        f"Groups: {', '.join(map(str, groups)) or 'none'}"
    )


def _interactive() -> tuple[Path, str, Path]:
    source = Path(input("Model file (.3dg/.3dcg/.3dan/.3da1): ").strip().strip('"'))
    print("\nFormats: " + ", ".join(FORMATS))
    format_id = input("Export format [bsp]: ").strip().lower() or "bsp"
    if format_id not in FORMATS:
        raise ValueError(f"unknown format: {format_id}")
    suggested = source.with_suffix(FORMATS[format_id])
    output = input(f"Output file [{suggested}]: ").strip().strip('"')
    return source, format_id, Path(output) if output else suggested


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Quick frontend for the SHAPED Python library.")
    parser.add_argument("input", type=Path, nargs="?", help="3DG1, 3DCG, 3DAN, or 3DA1 input")
    parser.add_argument("format", choices=FORMATS, nargs="?", help="export format")
    parser.add_argument("output", type=Path, nargs="?", help="output path")
    parser.add_argument("--summary", action="store_true", help="print model information without exporting")
    args = parser.parse_args(argv)
    try:
        if args.input is None:
            source, format_id, output = _interactive()
        else:
            source = args.input
            if args.summary:
                format_id = ""
                output = Path()
            else:
                if not args.format:
                    parser.error("format is required when an input is supplied")
                format_id = args.format
                output = args.output or source.with_suffix(FORMATS[format_id])
        model = load(source)
        load_colour_tables(model, source.parent / "COLTABS.DAT")
        print(_summary(model))
        if not args.summary or args.input is None:
            write(model, output, format_id)
            print(f"\nSaved: {output}")
    except (FormatError, OSError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
