"""Headless regression gates for recovered SHAPED formats and exporters."""

from __future__ import annotations

import pathlib
import subprocess
import sys
import tempfile


EXE = pathlib.Path(sys.argv[1]).resolve()
ROOT = pathlib.Path(sys.argv[2]).resolve()
FIXTURES = ROOT / "tests"


def export(
    mode: str,
    source: str,
    destination: pathlib.Path,
    cwd: pathlib.Path | None = None,
) -> bytes:
    result = subprocess.run(
        [str(EXE), mode, str(FIXTURES / source), str(destination)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
        cwd=cwd,
    )
    if result.returncode:
        raise AssertionError(
            f"{mode} {source} exited {result.returncode}: "
            f"{result.stdout!r} {result.stderr!r}"
        )
    return destination.read_bytes()


def classic(source: str, destination: pathlib.Path) -> bytes:
    result = subprocess.run(
        [str(EXE), "-b", str(FIXTURES / source), str(destination)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
    )
    if result.returncode:
        raise AssertionError(f"-b {source} exited {result.returncode}")
    return destination.read_bytes()


def contains(data: bytes, *needles: bytes) -> None:
    for needle in needles:
        if needle not in data:
            raise AssertionError(f"missing exporter fragment {needle!r}")


with tempfile.TemporaryDirectory(prefix="shaped-regression-") as temp_name:
    temp = pathlib.Path(temp_name)

    callback_report = temp / "editor-callbacks.txt"
    callback_result = subprocess.run(
        [str(EXE), "--test-editor-callbacks", str(callback_report)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
    )
    if callback_result.returncode:
        raise AssertionError(
            f"editor callback regression exited {callback_result.returncode}: "
            f"{callback_result.stdout!r} {callback_result.stderr!r}"
        )
    assert callback_report.read_bytes() == (
        b"palette-ui-isolation=1\n"
        b"palette-number-command=1\n"
        b"type-first=1\n"
        b"line-edit=1\n"
        b"mirror-zero-axis=1\n"
        b"mirror-option-coupling=1\n"
        b"mirror-clone-selection=1\n"
        b"preview-initial=1\n"
        b"preview-spread=1\n"
        b"preview-rotate=1\n"
        b"preview-reset=1\n"
        b"preview-frames=1\n"
        b"preview-exit=1\n"
        b"show-all-order=1\n"
        b"render-one-vertex=1\n"
        b"frame-selection-remap=1\n"
        b"frame-actions=1\n"
        b"frame-capacity=1\n"
    )

    # ReadInt fills the last free fixed polygon slot, so a loaded 3DCG's
    # polygon records are exposed in reverse order when saved again.
    animated = export("--export-internal", "cube-animated.3dcg", temp / "animated.3dcg")
    animated_source = (FIXTURES / "cube-animated.3dcg").read_bytes().splitlines()
    assert animated == b"\n".join(animated_source[:-6] + animated_source[-6:][::-1]) + b"\n"
    m3d = export("--export-3dg1", "cube.3dg", temp / "cube.3dg")
    assert m3d == (FIXTURES / "cube.3dg").read_bytes()
    wrapped = export("--export-internal", "coords-wrap.3dg", temp / "wrapped.3dcg")
    assert wrapped == (
        b"3DCG\n2 1\n1 -1 -32768,1\n32767 1 -1,1\n"
        b"2 0 1 ,7 0x1 0x7\n"
    )

    # The expanded animation limit accepts and preserves all 128 frames.
    frame_source = temp / "128-frames.3dcg"
    frame_bytes = (
        b"3DCG\n1 128\n"
        + b"".join(f"{frame} 0 0,1\n".encode("ascii") for frame in range(128))
    )
    frame_source.write_bytes(frame_bytes)
    assert export(
        "--export-internal", str(frame_source), temp / "128-frames-roundtrip.3dcg"
    ) == frame_bytes

    # The other two accepted DOS interchange signatures have intentionally
    # different coordinate, winding, colour, and packed-type rules.
    imported_anim = export(
        "--export-internal", "cube-animation.3dan", temp / "animation.3dcg"
    )
    assert imported_anim == (
        b"3DCG\n8 2\n"
        b"-50 -50 -50,1\n50 -50 -50,1\n50 50 -50,1\n-50 50 -50,1\n"
        b"-50 -50 50,1\n50 -50 50,1\n50 50 50,1\n-50 50 50,1\n"
        b"-70 -35 -35,1\n70 -35 -35,1\n70 35 -35,1\n-70 35 -35,1\n"
        b"-70 -35 35,1\n70 -35 35,1\n70 35 35,1\n-70 35 35,1\n"
        b"4 0 1 2 3 ,10 0x1 0x5\n4 4 7 6 5 ,11 0x1 0x5\n"
        b"4 0 4 5 1 ,12 0x1 0x6\n4 1 5 6 2 ,13 0x1 0x6\n"
        b"4 2 6 7 3 ,14 0x1 0x7\n4 3 7 4 0 ,15 0x1 0x7\n"
    )
    imported_sams = export(
        "--export-internal", "cube-sams.3da1", temp / "sams.3dcg"
    )
    assert imported_sams == (
        b"3DCG\n8 2\n"
        b"-50 -50 -50,1\n50 -50 -50,1\n50 50 -50,1\n-50 50 -50,1\n"
        b"-50 -50 50,1\n50 -50 50,1\n50 50 50,1\n-50 50 50,1\n"
        b"-50 -50 -50,1\n50 -50 -50,1\n50 50 -50,1\n-50 50 -50,1\n"
        b"-70 -30 30,1\n70 -30 30,1\n70 30 30,1\n-70 30 30,1\n"
        b"4 0 1 2 3 ,10 0x1 0x1\n4 4 7 6 5 ,11 0x1 0x1\n"
        b"4 0 4 5 1 ,12 0x1 0x3\n4 1 5 6 2 ,13 0x1 0x3\n"
        b"4 2 6 7 3 ,14 0x1 0x1\n4 3 7 4 0 ,15 0x1 0x1\n"
    )

    # Twist uses the DOS metric: squared plane distances divided by the raw
    # first-three-vertex cross-product magnitude.  This fixture is exactly on
    # the strict 0.01 selection boundary.
    twist = export("--test-twist", "twisted-quad.3dg", temp / "twist.txt")
    assert twist == b"Avg twist 1.000000%\nSelected\n"

    # The assembler point writer folds reflected pairs and isolates changed runs.
    gzs = export("--export-gzs", "cube.3dg", temp / "cube.gzs")
    contains(gzs, b"cube_P\n\tPointsXb\t4\n", b"\tPointsb\t1\n", b"\tVizis\t6\n")
    animated_gzs = export("--export-gzs", "cube-animated.3dcg", temp / "animated.gzs")
    contains(
        animated_gzs,
        b"\tFrames\t2\n",
        b"\tjumptab\t.A0A\n",
        b".A1A\tPointsXb\t4\n",
        b"\tjump\t.EB0\n",
    )

    # Temporary group centers consume inactive slots before extending the array.
    group_hole = export("--export-gzs", "group-hole.3dcg", temp / "groups.gzs")
    contains(group_hole, b"\tPointsb\t6\n", b"\tGroupP\t1\t;0\n", b"\tGroupP\t5\t;1\n")
    group_overflow = export(
        "--export-gzs", "group-overflow.3dcg", temp / "group-overflow.gzs"
    )
    contains(group_overflow, b"\tpw\t-8154,-3,3\t;3\n")

    # The DOS builder emits an ordered flat leaf for an ordinary model. It does
    # not synthesize a partition plane for every polygon.
    bsp = export("--export-bsp", "cube.3dg", temp / "cube.bsp")
    contains(bsp, b"cube_f1\tFaces\n", b"\tFend\n\tEndShape\n")
    assert b"\tBSPInit\t" not in bsp
    assert b"\tBSP\t" not in bsp
    assert bsp.count(b"\tFace4\t") == 6

    # The convenience CLI derives .asm beside the source and must remain safe
    # when the containing path needs Windows command-line quoting.
    auto_dir = temp / "BSP source with spaces"
    auto_dir.mkdir()
    auto_source = auto_dir / "cube.3dg"
    auto_source.write_bytes((FIXTURES / "cube.3dg").read_bytes())
    auto_result = subprocess.run(
        [str(EXE), "--bsp", str(auto_source)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
    )
    assert auto_result.returncode == 0
    auto_output = auto_source.with_suffix(".asm")
    auto_bytes = auto_output.read_bytes()
    auto_output.unlink()
    explicit_bytes = export("--export-bsp", str(auto_source), auto_output)
    assert auto_bytes == explicit_bytes

    classic_path = temp / "classic.asm"
    modern_classic = export("--export-bsp", "cube.3dg", classic_path)
    classic_result = subprocess.run(
        [str(EXE), "-b", str(FIXTURES / "cube.3dg"), str(classic_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=20,
        check=False,
    )
    assert classic_result.returncode == 0
    assert classic_path.read_bytes() == modern_classic

    # This body was emitted by the supplied DOS executable for the same batch
    # input.  Only the first source-path comment is machine-specific.
    dos_named = classic("bsp-crossing.3dcg", temp / "DOS.ASM")
    assert dos_named.split(b"\n", 1)[1] == (
        FIXTURES / "oracle-crossing-batch-body.asm"
    ).read_bytes()

    # Coplanar polygons stay together in the flat BSP face block.
    coplanar_bsp = export("--export-bsp", "coplanar-visibility.3dg", temp / "coplanar.bsp")
    contains(coplanar_bsp, b"coplanar_f1\tFaces\n", b"\tFend\n\tEndShape\n")
    assert coplanar_bsp.count(b"\tFace3\t") == 3
    assert b"\tBSP\t" not in coplanar_bsp

    # A negative COLTAB value enables the original per-vertex smooth normals block.
    smooth_dir = temp / "smooth-data"
    smooth_dir.mkdir()
    (smooth_dir / "COLTABS.DAT").write_bytes(b"COLTAB SMOOTH -16\n")
    smooth_bsp = export(
        "--export-bsp", "cube.3dg", smooth_dir / "smooth.asm", cwd=smooth_dir
    )
    contains(smooth_bsp, b"smooth_VN\t\t;Vertex normals\n", b"\tVNORMALS\t8\n")
    assert smooth_bsp.count(b"\tVN\t") == 8
    smooth_line = export(
        "--export-bsp", "smooth-line.3dcg", smooth_dir / "smooth-line.asm", cwd=smooth_dir
    )
    contains(smooth_line, b"\tVN\t0,-0,-127\t;0-(2)\n", b"\tFace2\t2,-1,0,0,0,0,1\n")
    smooth_gzs = export(
        "--export-gzs", "cube.3dg", smooth_dir / "smooth-gzs.asm", cwd=smooth_dir
    )
    contains(smooth_gzs, b"smooth-gzs_VN\t\t;Vertex normals\n", b"\tVNORMALS\t9\n")
    smooth_group = export(
        "--export-gzs",
        "group-hole.3dcg",
        smooth_dir / "smooth-group.asm",
        cwd=smooth_dir,
    )
    contains(smooth_group, b"\tPointsb\t6\n", b"\tVNORMALS\t6\n")

    # Two-point faces are line primitives, not disposable degenerate polygons.
    # In particular, BSP output must retain them even with no planar BSP root.
    line_gzs = export("--export-gzs", "two-point-face.3dcg", temp / "line.gzs")
    contains(line_gzs, b"\tFace2\t12,-1,0,0,0,0,1\n")
    line_bsp = export("--export-bsp", "two-point-face.3dcg", temp / "line.bsp")
    contains(
        line_bsp,
        b"line_f1\tFaces\n",
        b"\tFace2\t12,-1,0,0,0,0,1\n",
        b"\tFend\n\tEndShape\n",
    )
    assert b"\tBSPInit\t" not in line_bsp
    line_pc = export("--export-pc", "two-point-face.3dcg", temp / "line.pc")
    contains(line_pc, b"\tDW\tCMD_LINE_FV,12")

    # A single mutually crossing pair is still a DOS flat leaf. DoBSP reports
    # cuts diagnostically but never manufactures intersection geometry.
    crossing_bsp = export("--export-bsp", "bsp-crossing.3dcg", temp / "crossing.bsp")
    assert crossing_bsp.count(b"\tFace4\t") == 2
    assert b"\tBSPInit\t" not in crossing_bsp
    assert b"\tpb\t0,5,0\t;8\n" not in crossing_bsp

    # A three-plane ordering cycle is the smallest verified case that promotes
    # the 0x6000 candidate to a genuine DOS partition node. The classic path's
    # compact pass also exposes ReadInt's reverse fixed-slot polygon order.
    cycle_bsp = classic("bsp-cycle.3dcg", temp / "cycle-classic.asm")
    contains(
        cycle_bsp,
        b"\tBSPInit\tcycle-classic_EBSP\n",
        b"\tBSP\t2,cycle-classic_f1,.bsp2\n",
        b"\tBSPE\tcycle-classic_f3\n",
        b".bsp2\tBSPE\tcycle-classic_f4\n",
        b"\tFace4\t1,2,127,0,0,0,1,11,10\n",
        b"\tFace4\t3,0,0,0,127,5,4,6,7\n",
        b"\tFace4\t2,1,0,-127,0,2,3,9,8\n",
    )
    plane_bsp = classic("bsp-plane.3dcg", temp / "plane-classic.asm")
    assert b"\tBSPInit\t" not in plane_bsp
    first = plane_bsp.index(b"\tFace4\t2,1,")
    second = plane_bsp.index(b"\tFace4\t3,0,")
    third = plane_bsp.index(b"\tFace4\t1,2,")
    assert first < second < third
    crossing_pc = export("--export-pc", "bsp-crossing.3dcg", temp / "crossing.pc")
    assert crossing_pc.count(b"CMD_POLYGON_FV,") == 2
    assert b"\tDW CMD_BSP_NODE," not in crossing_pc
    animated_crossing_bsp = export(
        "--export-bsp", "bsp-crossing-animated.3dcg", temp / "crossing-animated.bsp"
    )
    assert animated_crossing_bsp.count(b"\tFace4\t") == 2
    assert b"\tpb\t0,5,0\t;8\n" not in animated_crossing_bsp
    animated_crossing_pc = export(
        "--export-pc", "bsp-crossing-animated.3dcg", temp / "crossing-animated.pc"
    )
    assert animated_crossing_pc.count(b"CMD_POLYGON_FV,") == 2
    assert b"\tDW CMD_BSP_NODE," not in animated_crossing_pc

    # Static PC streams use coefficient, intensity, and visibility offsets.
    pc = export("--export-pc", "cube.3dg", temp / "cube.pc")
    contains(
        pc,
        b"MAX_COE_OF\tEQU\t44\n",
        b"\tDW CMD_ICOORDS_RX,1\n",
        b"\tDW CMD_INTENSITIES\n",
        b"\tDW CMD_VISIBILITIES\n",
        b"\tDW CMD_VERTICES_RX\n",
        b"\tDW\t8,20,32\t; (-50,50,-50),(50,50,-50), 44\n",
        b"\tDW CMD_JUMP,cube_f1\n",
        b"cube_f1\tlabel word\n",
        b"\tDW\tCMD_POLYGON_I,116,522,104,4,44,50,56,62\n",
    )
    assert b"\tDW CMD_BSP_NODE," not in pc

    # Coplanar faces share the DOS writer's two-byte visibility test record.
    coplanar_pc = export("--export-pc", "coplanar-visibility.3dg", temp / "coplanar.pc")
    assert coplanar_pc.count(b"\tDW\t32,26,20\t; 44\n") == 1
    contains(
        coplanar_pc,
        b"\tDW\tCMD_POLYGON_F,44,515,3,32,26,20\n",
        b"\tDW\tCMD_POLYGON_F,45,514,3,20,32,38\n",
        b"\tDW\tCMD_POLYGON_F,45,513,3,20,26,32\n",
    )

    # The three renderer option bits select all eight recovered command forms.
    type_matrix_pc = export("--export-pc", "pc-type-matrix.3dcg", temp / "types.pc")
    for command in (b"FV", b"IV", b"F", b"I", b"FVZ", b"IVZ", b"FZ", b"IZ"):
        assert type_matrix_pc.count(b"CMD_POLYGON_" + command + b",") == 1

    # Animated PC streams must not silently collapse to the first frame.
    animated_pc = export("--export-pc", "cube-animated.3dcg", temp / "animated.pc")
    contains(
        animated_pc,
        b"\tDW CMD_SWITCH,obj_anim,4\n",
        b"animated_c0\tlabel word\n",
        b"\tDW CMD_BLANK,MAX_COE_OF-44\n",
        b"MAX_COE_OF\tEQU\t44\n",
        b"animated_v0\tlabel word\n",
        b"animated_v1\tlabel word\n",
        b"\tDW\t8,20,32\t; (-80,30,-30), 44\n",
    )

print("SHAPED headless regression suite passed")
