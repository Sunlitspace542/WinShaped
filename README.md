# Shaped native Windows reconstruction

Version 1.0.1

For complete illustrated operating instructions, see the
**[SHAPED Native 1.0 User Guide](docs/USER_GUIDE.md)**.

This workspace contains a clean native Win32 reconstruction of Argonaut's DOS
`SHAPED.EXE` (1991–1992), based on the user-supplied executable, debugger symbol
table, debugger assembly listing, extracted strings, and behavioral observation.

## Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The self-contained executable is written to `build/dist/Shaped.exe`. MinGW
support libraries are statically linked; its remaining imports are Windows
system components (GDI, User, Shell, Kernel, and the OS Universal CRT API sets).

## Current reconstruction map

- Pixel-matched 640x480 main layout: 560x480 editor, three 280x240 grids,
  lower-right status/preview field, and the original 24-row 80x20 sidebar
- Recovered orthographic pane mapping and orientation: upper-left Z/Y,
  upper-right X/Y, and lower-left Z/X with the original inverted X vertical
  direction; picking, insertion, Move, Copy, and Rotate share those projections
- Recovered the persistent three-dimensional cursor shared across panes, its
  dominant-motion complementary-projection state, five-screen-pixel arrow-key
  origin movement (including shifted grid/zero axes), Home reset, and full-cursor
  Add Dot placement
- Recovered EGA palette values, 10-pixel default grid pitch, exact title string,
  embedded original Fastgraph 8x14 glyph bytes, static button enable colours,
  and the original blue status field
- Recovered editor primitive presentation: polygons use fixed EGA 6/7 for
  unselected/selected passes (independent of their export colour), while dots
  use fixed EGA 4/5 and the original 2x2 rectangle with a one-pixel origin bias;
  both primitives draw unselected slots first and selected slots second, and
  Show-all retains each animation frame's stored dot-selection bits. One-vertex
  polygons retain the DOS zero-length closing edge instead of being culled
- Recovered live menu state rendering (bright-green Plane/Grid/Type/group/palette
  selections) and the idle-mouse yellow X/Y/Z coordinate strip with exact
  orthographic projection, number format, pixel bounds, and Fastgraph text
- Recovered power-of-two Zoom Up/Down scale changes and Auto Zoom's active-dot
  bounding-range fit, zero-inclusive bounds, and integer midpoint recentering
- Vertex and polygon editing, ordered dot-selection lists, click-order polygon
  construction (including deselect/reappend semantics), the recovered `Select` /
  `Add Dot` mouse-mode toggle with grid-snapped selected insertion, left-select /
  right-deselect picking with the original nearest-versus-average 3D test,
  inactive-slot deletion followed by explicit `Compact`, and single-level swap
  undo with selection-order restoration
- Polygon type editing reproduces the inert base `Poly` row, derives menu state
  from the first selected line-or-polygon face in fixed-slot order, and toggles
  only the five original attribute bits on eligible selected faces
- Recovered transform interaction: pixel-accurate projected Move, all-frame Copy,
  and Rotate's centre-click/angle-drag/`Add image` workflow; Move/Copy use the
  original snapped-current-minus-snapped-start displacement and Rotate/Size use
  the DOS signed-integer truncation semantics. Copy and Rotate add-image preserve
  the original `active extent + source index` layout (including holes), source
  selection, temporary flag propagation, and their distinct all-frame versus
  current-frame activation rules
- Original in-client blue/cyan `QUIT to DOS`, `Delete all`, Rotate `Add image`,
  five-option `Mirror Shape`, six-row `Size Shape`, and six-action
  `Animation Frames` dialogs, plus the eleven-row count-aware `Select Polygons`
  panel and one-row colour editors, with their recovered 640x480 pixel geometry,
  editable fields, option defaults, row order, and button ordering
- Original Mirror behavior: reflection around world zero, selected-dot/selected-polygon
  propagation, temporary flag retention, extent-preserving add-image operation
  across animation frames, cloned point flags, and the original unconditional
  single winding reversal even when zero or two reflection axes are enabled;
  disabling `Add image` also forcibly disables `Selected only` as in DOS, and
  added-image dots retain the copied `0x0100` selection flag after only the
  temporary `0x0200` marker is cleared
- Recovered fixed capacities of 500 dot slots and 500 polygon slots
- Grid-sensitive topology compaction: the original `max(Grid / 8, 1)` inclusive
  merge radius, repeated polygon vertices, cyclic duplicate polygons, inactive-slot
  packing, and the fixed/animated plus mirrored/unpaired dot ordering pass
- Recovered polygon Sort ordering by line/type class and the `frexp` exponent of
  polygon normal magnitude squared times vertex count, plus the original reverse
  selected-suborder produced by `Polys to End` and each command's exact single-
  undo participation
- Original full-screen, five-column file selector for every interactive load/save
  path, including directory traversal, paging, editable Path/Name fields, and the
  recovered `OK`, `CANCEL`, `<<`, and `>>` controls
- Recovered `3DG1` M3D and `3DCG` internal output, including the full per-frame
  dot flag word and the original rule that visiting a frame clears and reapplies the global
  ordered selection into that frame's `0x0100` bits, as well as polygon selection
  in bit `0x0100` of each polygon flag word; `3DCG` loading also reproduces
  `_ReadInt`'s last-free-slot allocation, which reverses file polygon order
- Recovered distinction between `Internal` (compaction-checked quick-save to the
  loaded input path) and `3DCG` (explicit `Save Internal` selector)
- Recovered interactive M3D save contract: `Save M3d` is always selected explicitly,
  multi-frame animation and shapes with inactive-dot gaps are rejected with the
  original status messages, and existing paths are never silently overwritten
- Native loaders for all four original signatures: `3DG1`, `3DCG`, `3DAN`, `3DA1`
  including their distinct default/packed polygon-type decoding, one-frame M3D
  allocation, current-group activation, SAMS winding reversal, and Y negation
- Recovered load/save error contract, including retained input paths, the
  `Alien file format` message, and `File <...>?` reporting
- Multi-frame animation editing up to 128 frames, insertion/deletion/copying,
  key loading, the
  original inline-value Frames workflow, and one-pass Show-all dot overlay in
  cyclic next-frame order with the current frame drawn last; the
  recovered `Shift An` command rotates selected-dot and selected-polygon-vertex
  records forward by one frame, including the original temporary `0x0200` flags,
  while Key Frame performs the DOS inverse-square weighted deformation from up
  to 16 animated control points rather than replacing the edited mesh
- Native GZS, BSP, and PC assembler exporters plus BSP diagnostics, with
  two-point `Face2` line primitives retained even in line-only BSP output; the recovered
  modal Preview replaces the 560x480 editor while preserving the sidebar, uses
  the original Euler matrix/perspective and EGA selection passes, and supports
  the DOS arrow/Page Up/Page Down/angle/distance/hidden-surface/Home/frame/exit
  key set with persistent angles and hidden-surface state; entering Preview also
  performs the original `_SpreadFlags` propagation into every animation frame
- SNES `COLTABS.DAT` dynamic table/palette/texture menus, BGR555 palette use in
  the embedded 3D-system view (without recolouring the EGA editor), exact
  palette-number command bytes, recovered transfer addresses, `GRID.DAT`
  template substitution, smooth vertex-normal output, and SNES BSP
  transfer-source reconstruction
- Original title/status presentation and command-line file loading
- Recovered `CalcMenuMask` conditions for active dots, polygons, selected dots,
  selected polygons, animation frames, and the original `sdemo.rom`-driven
  SNES mode (`0x2000`); these now drive both main-menu and submenu
  colours/clickability
- Recovered global keyboard dispatch from all 122 button records, including the
  original letter/punctuation commands, F1-F10 group/polygon controls, numeric
  group display, Page Up/Down zoom, and enable-mask gating

Reverse-engineering evidence is retained under `reverse/`; the untouched DOS
binary is retained under `original/`. The native implementation does not execute
or embed DOS code.

The public repository intentionally excludes the supplied proprietary DOS
binary, debugger-symbol archive, raw disassembly, and locally bundled third-party
toolchains. The MIT license applies only to this native-port implementation and
its authored documentation; it grants no rights to original Argonaut Software
material.

## DOS hardware translation boundary

The ordinary editor, modal Preview, model formats, and assembler writers are
native reconstructions. Two original menu paths were only front ends for
separate proprietary development tools and hardware: `Show > 3D sys` generated
`tmpshape.asm`, ran `asm2bin`, and invoked Argonaut's `DL` downloader; the SNES
palette, palette-number, texture, and shape senders likewise invoked `DL` at
addresses `0x5E00`, `0x6000`, and `0x8000`. Those external binaries, target
renderer, and hardware are not part of `SHAPED.EXE` and therefore cannot be
embedded from the supplied program.

The Windows port replaces the external 3D-system display with an embedded
filled/BSP-ordered view and keeps the recovered SNES source, BGR555 palette,
texture-selection, command-byte, address, and `GRID.DAT` contracts. `Send SNES`
offers the portable result as an assembler file rather than pretending to
perform a hardware transfer. The exact DOS modal `Preview` remains available
separately. See `reverse/SNES.md` for the disassembly evidence.

The recovery utilities under `tools/` reproduce the symbol, disassembly, string,
and menu inventories. `reverse/menus.json` currently decodes 16 original menus
and 122 button records, including shortcut keys and callback addresses.
`tools/verify_layout.py` checks the recovered main-screen dimensions, view splits,
axis/grid positions, palette, and all 24 sidebar rows against a native capture.

The headless exporter interface accepts `--export-gzs`, `--export-bsp`,
`--export-pc`, `--export-internal`, and `--export-3dg1`, followed by an input
model and output path. The two native text formats round-trip the regression
fixtures byte-for-byte. The original batch spelling, `Shaped -b input output`,
is also retained for BSP conversion.

For one-step BSP conversion, use:

```powershell
# Opens the recovered file selector, then writes <selected-name>.asm beside it.
.\Shaped.exe --bsp

# Non-interactive; writes C:\Models\cube.asm automatically.
.\Shaped.exe --bsp "C:\Models\cube.3dg"

# Explicit destination remains available.
.\Shaped.exe --bsp "C:\Models\cube.3dg" "C:\Output\cube.asm"
```

`--export-bsp input` supports the same automatic `.asm` naming. The legacy
`-b input [output]` form also supports omitted output and retains its original
pre-export compaction step. A failed load/export returns exit code 1; cancelling
the interactive selector returns 2.

Run `ctest --test-dir build -C Release --output-on-failure` for the registered
headless suite covering editor callback and interactive-preview edge cases, all
four import signatures with byte-exact normalized output, native round trips,
signed-coordinate wrapping, reflected and animated point blocks, group-center
slot reuse, BSP traversal, a DOS-generated classic BSP oracle, and PC
coefficient/animation records.

The recovered GZS writer now preserves assembler-label case, reproduces the
legacy X/Y coordinate flip and 127-unit byte/word threshold, writes 127-scaled
face normals, compresses adjacent reflected points with `PointsX`, emits
per-frame jump tables for animated runs, and writes the original temporary
group-center point and grouped face structure.

The PC writer reproduces the legacy Y flip, signed-pair coordinate and
light-normal coefficient tables, compacted six-byte vertex-component offsets,
shared directional visibility records, and all eight polygon command forms.
Animated inputs use frame-aware coefficient tables and switches. The recovered
BSP builder emits the original ordered single face leaf for ordinary, coplanar,
and two-polygon crossing cases instead of manufacturing a partition node per
polygon; only conflicting pairwise ordering constraints promote a polygon to a
real BSP splitter. Flat PC programs jump directly to their primitive block,
while true trees use numbered `CMD_BSP_NODE` branches. The classic `-b` path
also runs the original compact operation before BSP conversion.
