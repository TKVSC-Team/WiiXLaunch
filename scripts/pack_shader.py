#!/usr/bin/env python3
"""Compiles a GLSL vertex/fragment shader pair with Nintendo's ShaderConverter.exe
and repacks the output into the minimal "sead binary" format NvnOverlay::GetBnshProgram
expects (see nvn_overlay.hpp and docs/switch-nvn-findings.md).

Recipe was reverse-engineered by diffing ShaderConverter.exe's raw --separable .bnsh
output against the hand-built include/rainbow_sead_bin.hpp that's proven to render
correctly in-game (see docs/switch-nvn-findings.md, "Rainbow quad rendered solid
black" section and onward). Concretely:

  - ShaderConverter's raw archive contains two blocks tagged with GLSLC's
    control-section magic (0x98761234, little-endian bytes 34 12 76 98) - one
    real, distinct control blob per stage, NOT duplicates of each other (an
    earlier version of this tool assumed they were byte-identical and reused
    just the first for both stages; that produced a control blob for the
    fragment stage with the wrong "stage bucket" flag and a content
    fingerprint matching the vertex stage's code instead of its own, which
    the driver rejects at bind time - see docs/switch-nvn-findings.md's "real
    root cause" section for the full byte-level diff that found this).
  - Each stage's compiled SASS machine code is tagged with a DATA magic
    (0x12345678, little-endian bytes 78 56 34 12). For a --separable two-stage
    (vertex + pixel) compile, the first occurrence is vertex code, the second is
    fragment code - confirmed by locating the EXACT byte sequence already embedded
    in rainbow_sead_bin.hpp's code sections inside the raw compiler output.
  - Each code section's file offset must be 256-byte aligned (confirmed by
    comparing this tool's own unaligned first-attempt output against the
    aligned, working hand-built file).
  - At load time (NvnOverlay::GetBnshProgram in nvn_overlay.hpp), only two control
    fields are patched at runtime: the version word and the two device-ISA words -
    everything else in the control blob is used exactly as baked in here.

Usage:
    python pack_shader.py <vertex.glsl> <fragment.glsl> <output_name> [--out include/]

Produces include/<output_name>_sead_bin.hpp, in the same style as
include/rainbow_sead_bin.hpp - #include it and pass g_<Name>SeadBin /
k<Name>SeadBinSize to GetBnshProgram's memcpy the same way g_RainbowSeadBin is
used today.
"""
import argparse
import pathlib
import struct
import subprocess
import sys
import tempfile

SHADER_CONVERTER = pathlib.Path(
    r"C:\Users\dylan\repos\NVN\NvnTools\GraphicsTools\ShaderConverter.exe"
)

CTRL_MAGIC = bytes.fromhex("34127698")  # 0x98761234 little-endian
DATA_MAGIC = bytes.fromhex("78563412")  # 0x12345678 little-endian


def find_all(buf: bytes, pattern: bytes) -> list[int]:
    out = []
    i = 0
    while True:
        i = buf.find(pattern, i)
        if i == -1:
            break
        out.append(i)
        i += 1
    return out


def compile_bnsh(vertex_glsl: pathlib.Path, fragment_glsl: pathlib.Path, tmp_out: pathlib.Path) -> bytes:
    if not SHADER_CONVERTER.exists():
        sys.exit(f"ShaderConverter.exe not found at {SHADER_CONVERTER}")
    cmd = [
        str(SHADER_CONVERTER),
        "-s", "Glsl",
        "-c", "Binary",
        "-a", "Nvn",
        "--separable",
        "--vertex-shader", str(vertex_glsl),
        "--pixel-shader", str(fragment_glsl),
        "-o", str(tmp_out),
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0 or not tmp_out.exists():
        sys.exit(
            "ShaderConverter.exe failed:\n"
            f"stdout: {result.stdout}\nstderr: {result.stderr}"
        )
    print(result.stdout.strip())
    return tmp_out.read_bytes()


def extract_control_templates(bnsh: bytes) -> tuple[bytes, bytes]:
    """Returns (vertex_control, fragment_control) - two DISTINCT blocks, not
    the same one reused. Earlier versions of this tool treated the two
    control-magic (0x98761234) occurrences in the raw compiler output as
    byte-identical duplicates and copied only the first for both stages -
    wrong, and the real cause of every "ProgramSetShaders result=0" bind
    failure this tool produced. Diffing the working, hand-built
    rainbow_sead_bin.hpp's real vertex vs. fragment control sections found 31
    real differing bytes: a per-stage "bucket" flag at relative offset 0x714/
    0x734 (matches docs/switch-nvn-findings.md's independently-decompiled
    "stage bucket" finding) and, most importantly, a 24-byte block at
    0x7d0-0x7e7 that looks like a per-stage content fingerprint/hash - and the
    raw compiler output's two control-magic blocks differ at those EXACT same
    31 byte offsets, with the exact same 0->1 pattern at 0x714/0x734. That's
    conclusive: the second block is the real, distinct fragment control
    section, not a duplicate - and blindly reusing the vertex one for
    fragment left the fragment section with the wrong stage bucket and a
    hash that matches the wrong stage's code, which the driver presumably
    checks against the actual code it's being handed.
    """
    hits = find_all(bnsh, CTRL_MAGIC)
    if len(hits) < 2:
        sys.exit(
            f"Expected >=2 control-magic (0x98761234) occurrences, found {len(hits)} - "
            "compiler output shape may have changed, needs re-verification against "
            "docs/switch-nvn-findings.md before trusting this tool's output."
        )
    vert_start, frag_start = hits[0], hits[1]
    block_len = frag_start - vert_start
    vert_control = bnsh[vert_start:frag_start]
    frag_control = bnsh[frag_start:frag_start + block_len]
    if len(frag_control) < block_len:
        sys.exit("Fragment control block runs past end of file - unexpected compiler output shape.")
    return vert_control, frag_control


# Real content for a stage (code, plus - for shaders with enough float/int
# literals - one or more separate inline constant-pool chunks further out,
# each with no marker of its own, separated from the code and from each
# other by real internal alignment gaps up to ~64 bytes wide, confirmed
# empirically) is never the LAST thing before a hard, confirmed boundary -
# there's always real, non-zero, unrelated container data (a debug string
# table - confirmed by finding the literal source-file basename inside it)
# starting comfortably further out (~2800-3500 bytes past the last stage's
# start on two real test shaders). A zero-run-length heuristic to tell
# "internal gap" from "true end" turned out fragile in both directions (a
# short threshold false-triggers inside an internal gap and truncates real
# data - confirmed on the plasma shader; a long threshold can run out of room
# before an early stage's hard boundary and falsely include trailing padding
# - confirmed on rainbow's vertex stage, which only has ~192 zero bytes
# before the next stage starts). Simpler and exact instead: whenever a stage
# has a REAL next boundary (the next stage's own data-magic marker - stages
# are packed back-to-back) use that directly and just take the last non-zero
# byte before it - unambiguous, no heuristic. Only the LAST stage lacks such
# a marker; there, use SAFE_CAP, a fixed distance comfortably smaller than
# every observed real "start of container data" and comfortably larger than
# every observed real content length - not a proof for arbitrary future
# shaders, but a wide, checked margin on the two data points available.
SAFE_CAP = 0x800  # 2048 bytes


def region_length(bnsh: bytes, start: int, bound: int) -> int:
    """Distance from `start` to the last non-zero byte before `bound`, plus
    one. See the SAFE_CAP comment above for what `bound` must be."""
    last = bound - 1
    while last >= start and bnsh[last] == 0:
        last -= 1
    return (last - start + 1) if last >= start else 0


def extract_code_regions(bnsh: bytes) -> tuple[bytes, bytes]:
    hits = find_all(bnsh, DATA_MAGIC)
    if len(hits) < 2:
        sys.exit(
            f"Expected >=2 data-magic (0x12345678) occurrences (vertex + fragment code), "
            f"found {len(hits)}."
        )
    vert_start, frag_start = hits[0], hits[1]
    vert_bound = frag_start  # exact: the next stage's own real marker

    later_data_hits = [h for h in hits[2:] if h > frag_start]
    frag_bound = min(later_data_hits[0], frag_start + SAFE_CAP) if later_data_hits else frag_start + SAFE_CAP
    frag_bound = min(frag_bound, len(bnsh))

    def trim_and_pad(region_start: int, length: int) -> bytes:
        # pad up to a 32-byte alignment as a small, harmless safety margin past
        # the exact real length found - not load-bearing, just tidy.
        padded = ((length + 31) // 32) * 32
        return bnsh[region_start:region_start + padded].ljust(padded, b"\x00")

    vert_len = region_length(bnsh, vert_start, vert_bound)
    frag_len = region_length(bnsh, frag_start, frag_bound)
    return trim_and_pad(vert_start, vert_len), trim_and_pad(frag_start, frag_len)


CODE_ALIGNMENT = 0x100  # 256 bytes


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def pack_sead_binary(vert_control: bytes, frag_control: bytes, vert_code: bytes, frag_code: bytes) -> bytes:
    # Each code section's file offset must be 256-byte aligned - confirmed by
    # comparing this tool's own first-attempt output (unaligned, code placed
    # immediately after the two control blocks with no padding) against the
    # known-good, hand-built rainbow_sead_bin.hpp: nvnProgramSetShaders bound
    # the unaligned layout successfully for ZERO shaders (including a repack
    # of the exact same rainbow source the hand-built file uses), while the
    # 256-aligned hand-built file always worked. Real GPU shader code commonly
    # requires this kind of alignment for instruction fetch; the hand-built
    # file's 0x1200/0x1400 code offsets are both exact multiples of 0x100.
    header_size = 0x10
    vert_ctrl_off = header_size
    frag_ctrl_off = vert_ctrl_off + len(vert_control)
    vert_code_off = align_up(frag_ctrl_off + len(frag_control), CODE_ALIGNMENT)
    frag_code_off = align_up(vert_code_off + len(vert_code), CODE_ALIGNMENT)

    header = struct.pack("<IIII", vert_ctrl_off, frag_ctrl_off, vert_code_off, frag_code_off)
    buf = bytearray(header + vert_control + frag_control)
    buf += b"\x00" * (vert_code_off - len(buf))
    buf += vert_code
    buf += b"\x00" * (frag_code_off - len(buf))
    buf += frag_code
    return bytes(buf)


def emit_header(data: bytes, name: str, out_path: pathlib.Path) -> None:
    array_name = f"g_{name}SeadBin"
    size_name = f"k{name}SeadBinSize"
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "#include <cstddef>",
        "",
        f"// Generated by scripts/pack_shader.py - do not hand-edit.",
        f"constexpr size_t {size_name} = {len(data)};",
        f"alignas(4096) inline uint8_t {array_name}[] = {{",
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("    " + " ".join(f"0x{b:02x}," for b in chunk))
    lines.append("};")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out_path} ({len(data)} bytes, array={array_name}, size={size_name})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("vertex_glsl", type=pathlib.Path)
    parser.add_argument("fragment_glsl", type=pathlib.Path)
    parser.add_argument("output_name", help="PascalCase name, e.g. Plasma -> include/plasma_sead_bin.hpp")
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("include"))
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_bnsh = pathlib.Path(tmpdir) / "out.bnsh"
        bnsh = compile_bnsh(args.vertex_glsl, args.fragment_glsl, tmp_bnsh)

    vert_control, frag_control = extract_control_templates(bnsh)
    vert_code, frag_code = extract_code_regions(bnsh)
    print(f"vertex control: {len(vert_control)} bytes, fragment control: {len(frag_control)} bytes")
    print(f"vertex code: {len(vert_code)} bytes, fragment code: {len(frag_code)} bytes")

    packed = pack_sead_binary(vert_control, frag_control, vert_code, frag_code)

    out_file = args.out / f"{args.output_name.lower()}_sead_bin.hpp"
    emit_header(packed, args.output_name, out_file)


if __name__ == "__main__":
    main()
