# Recovered Shaped file formats

This document records formats recovered directly from the named routines in the
Zortech debug overlay. Numeric input uses the DOS C runtime's `fscanf` behavior;
whitespace below is therefore flexible unless punctuation is shown.

Coordinate reads are immediately converted to the original program's signed
16-bit representation: truncate toward zero, then retain the low 16 bits.

## `3DG1` — M3D

```text
3DG1
<dot-count>
<x> <y> <z>                         repeated dot-count times
<vertex-count> <indices...> <colour> repeated until EOF
```

Coordinates are loaded as doubles and stored as signed integers by `_ReadM3D`.
`_SaveM3d` emits them as decimal integers. Polygon size is limited to 16.
Import creates one frame, assigns the current group to its dots and polygons,
and assigns the current default polygon type (initially `7`).

## `3DCG` — internal editor format

```text
3DCG
<dot-span> <frame-count>
<x> <y> <z>,<active>                 dot-span rows for every frame
<vertex-count> <indices...> ,<colour> 0x<flags> 0x<type>  until EOF
```

Inactive dot slots are written as `0 0 0,0`. This is the only recovered format
that preserves all per-polygon metadata and every animation frame. `_ReadInt`
searches the entire fixed polygon array and retains the last free slot, so
successive file records occupy descending slots. Once compacted or saved, their
observable polygon order is therefore the reverse of the input record order.

## `3DAN` — animation

```text
3DAN
<dot-count> <frame-count>
<x> <y> <z>                          dot-count rows for every frame
<vertex-count> <indices...> <packed-colour>  repeated until EOF
```

The low byte of the packed value is the colour; the polygon type is its signed
right shift by one (falling back to the current default when zero). Imported
dots and polygons receive the current group.

## `3DA1` — SAMS animation

```text
3DA1
POINTS:<fixed-count>
<x> <y> <z>                          fixed-count rows
ANIM:<animated-count> <frame-count>
FRAME:<number>
<x> <y> <z>                          animated-count rows per frame
FACES:<face-count>
<vertex-count> <indices...> <packed-colour>  face-count records
```

The SAMS importer reverses polygon index order and negates Y coordinates.
Its polygon type is selected by `(packed-colour >> 1)` from the original table
`7,5,7,5,3,1,3,1,15,13,15,13,11,9,11,9`.

## Assembler exporters

The remaining Save-menu branches are textual macro streams rather than object
files. Their grammar was recovered from `_SaveASM`, `_SaveGZS`, `_SaveBSP`,
`_WriteDotsASM`, `_WriteBSP1`, `_WriteBSP2`, and the `WritePC*` routines.

- `ASM GZS` begins with the `DO_HDR` conditional and `ShapeHdr`, emits a
  `name_P` point block using `Pointsb`/`Pointsw` and `pb`/`pw`, then a `name_F`
  block using `Faces`, `FaceN`, `FendQ`, `endshape`, and `endc`. The label is
  the output leaf name without its extension and retains its original case.
  Output points negate X and Y. The point macro changes from byte to word when
  the maximum 3-D radius exceeds 127. GZS temporarily inserts the integer
  average center of each nonempty polygon group into the first available
  inactive dot slot (extending the array only when necessary). The center's
  coordinate sums wrap as signed 16-bit words after every vertex add before
  signed integer division; multiple groups
  additionally emit `Groups`, `GroupP`, and `GroupF`. Face normals are
  unit normals multiplied by 127. Adjacent points mirrored across X in every
  frame are folded into `PointsXb`/`PointsXw` runs. Changed runs emit `Frames`,
  one `jumptab` per frame, `.A<frame><run>` labels, and a shared `.EB<run>`
  exit; unchanged runs remain outside the frame tables. Before the face
  blocks, `Vizis`/`Viz`
  records associate every polygon of at least three vertices with its first
  triangle and normal. A `FaceN` record begins with editable colour followed
  by the generated visibility index (or `-1` for a line), then its 127-scaled
  normal and vertex indices.
- `ASM BSP` has two distinct forms. The common acyclic case is a single ordered
  `name_f1 Faces` leaf terminated by `Fend` and `EndShape`; it contains no
  `BSPInit` at all. `_DoBSP1` derives pairwise front/back flags first and only
  creates a partition node for conflicting `0x6000` ordering constraints, so
  neither an ordinary cube nor one mutually crossing polygon pair is split.
  A true tree uses `BSPInit`, `BSP`, `BSPNULL`, `BSPEND`, `BSPE`, per-node face
  labels, and `FendQ`. `_WriteBSP1` numbers face and branch labels with one
  shared depth-first counter (a front branch reserves an extra number), while
  `_WriteBSP2` repeats the traversal and opens the corresponding `name_fN`
  face blocks. Candidate partition planes are weighted by raw normal magnitude
  and polygon type bit `0x20` multiplies that score by 25. The batch `-b` path
  performs `_CompactFunct` before `_DoBSP` and `_SaveBSP`.
- `ASM PC` is a word-oriented renderer command stream. Recovered commands
  include `CMD_COORDS_*`, `CMD_ICOORDS_*`, `CMD_VISIBILITIES`,
  `CMD_VERTICES`, `CMD_VERTICES_RX`, `CMD_INTENSITIES`, `CMD_LINE_FV`, the
  `CMD_POLYGON_*` family, `CMD_SWITCH`, `CMD_JUMP`, `CMD_BLANK`, and
  `CMD_QUIT`. Before writing, PC coordinates negate Y. Static X/Y/Z
  components are deduplicated; matching positive/negative pairs use
  `CMD_COORDS_RX/RY/RZ`, remaining values use `CMD_COORDS_X/Y/Z`, and each
  component occupies a six-byte coefficient offset beginning at 8. Vertex
  records refer to those offsets rather than storing raw coordinates. Normal
  components are scaled to 32767 and use two-byte `CMD_ICOORDS_*` offsets;
  `CMD_INTENSITIES` records combine three such offsets and polygon commands
  reference the resulting intensity-record offsets. `CMD_VISIBILITIES`
  records likewise store three vertex offsets. Polygon type bits select `FV`,
  `IV`, `F`, `I`, `FVZ`, `IVZ`, `FZ`, or `IZ` for mode values 0, 2, 4, 6,
  8, 10, 12, and 14 respectively; polygon colours below `0x200` gain the
  renderer's `0x200` bias.
  Animated native exports put unchanged vertices before the animation switch,
  fold unchanged adjacent X-mirrored pairs through `CMD_VERTICES_RX`, and put
  only changed vertices in the per-frame `CMD_SWITCH,obj_anim` alternatives.
  The final number in each vertex comment is its generated renderer-memory
  offset, not its editable dot index. Visibility and primitive records use the
  resulting compacted source-dot-to-memory map. Coplanar polygons share one
  two-byte visibility record; opposite winding selects its adjacent direction
  byte. A flat leaf jumps directly to one `name_f1` primitive block; genuine
  partition trees use `CMD_BSP_NODE` records referencing `name_bspN` branch
  labels and numbered `name_fN` blocks. Coordinate-only switches and
  `CMD_BLANK,MAX_COE_OF-N` keep every frame's coefficient layout the same size.

The native port exposes deterministic `--export-gzs`, `--export-bsp`, and
`--export-pc` command-line modes for regression testing these streams.

## Evidence

- `_ReadM3D` / `_SaveM3d`: `reverse/functions/0d79_001f__ReadM3D.asm`
  and `0d79_1229__SaveM3d.asm`
- `_ReadSams`: `reverse/functions/0d79_0424__ReadSams.asm`
- `_ReadAnim`: `reverse/functions/0d79_0762__ReadAnim.asm`
- `_ReadInt` / `_SaveInt`: `reverse/functions/0d79_0abf__ReadInt.asm`
  and `0d79_13c7__SaveInt.asm`
- Assembler exporters: `reverse/functions/0d79_15c0__WriteDotsASM.asm`,
  `0d79_26aa__SaveASM.asm`, `0d79_290f__SaveGZS.asm`,
  `0d79_3320__SaveBSP.asm`, and `131b_3762__SavePCFunct.asm`
