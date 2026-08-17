#!/usr/bin/env python3
"""Compiles a GLSL vertex/fragment shader pair with Nintendo's ShaderConverter.exe
and repacks the output into the minimal "sead binary" format NvnOverlay::GetBnshProgram
expects (see nvn_overlay.hpp and docs/switch-nvn-findings.md).

Recipe was reverse-engineered by diffing ShaderConverter.exe's raw --separable .bnsh
output against the hand-built include/rainbow_sead_bin.hpp that's proven to render
correctly in-game (see docs/switch-nvn-findings.md, "Rainbow quad rendered solid
black" section and onward). Concretely:

  - ShaderConverter's raw archive contains one or more blocks tagged with GLSLC's
    control-section magic (0x98761234, little-endian bytes 34 12 76 98). The FIRST
    such block, taken up to (but not including) the SECOND occurrence of that same
    magic, is a real, self-consistent, driver-accepted control blob - copied
    verbatim and reused unmodified for BOTH the vertex and fragment control
    sections (proven safe: the currently-deployed rainbow shader uses this exact
    control blob, byte-identical for both stages - stage identity comes from
    argument position in nvnProgramSetShaders's NVNshaderData array, not anything
    inside the control blob).
  - Each stage's compiled SASS machine code is tagged with a DATA magic
    (0x12345678, little-endian bytes 78 56 34 12). For a --separable two-stage
    (vertex + pixel) compile, the first occurrence is vertex code, the second is
    fragment code - confirmed by locating the EXACT byte sequence already embedded
    in rainbow_sead_bin.hpp's code sections inside the raw compiler output.
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


def extract_control_template(bnsh: bytes) -> bytes:
    hits = find_all(bnsh, CTRL_MAGIC)
    if len(hits) < 2:
        sys.exit(
            f"Expected >=2 control-magic (0x98761234) occurrences, found {len(hits)} - "
            "compiler output shape may have changed, needs re-verification against "
            "docs/switch-nvn-findings.md before trusting this tool's output."
        )
    start, end = hits[0], hits[1]
    return bnsh[start:end]


def extract_code_regions(bnsh: bytes) -> tuple[bytes, bytes]:
    hits = find_all(bnsh, DATA_MAGIC)
    if len(hits) < 2:
        sys.exit(
            f"Expected >=2 data-magic (0x12345678) occurrences (vertex + fragment code), "
            f"found {len(hits)}."
        )
    vert_start, frag_start = hits[0], hits[1]
    # ShaderConverter lays each stage's compiled code out in a fixed-size,
    # aligned slot (confirmed empirically: two independent compiles of the same
    # source both placed vertex/fragment data-magic markers exactly 0x200 bytes
    # apart). Reuse that same slot size for the LAST stage too, rather than
    # scanning to EOF - the raw archive has real, non-zero trailing sections
    # after the code (debug/variation bookkeeping, even without --reflection)
    # that a naive "trim trailing zeros up to EOF" scan would wrongly scoop up.
    slot_size = frag_start - vert_start
    if len(hits) >= 3:
        slot_size = min(slot_size, hits[2] - frag_start)

    def trim(buf: bytes) -> bytes:
        last = len(buf)
        while last > 4 and buf[last - 1] == 0:
            last -= 1
        # pad back up to a 32-byte alignment, generous margin past the last
        # real instruction word, matching the spirit of the existing project's
        # "generously padded past every known-touched offset" convention.
        padded = ((last + 31) // 32) * 32
        return buf[:padded].ljust(padded, b"\x00")

    vert_raw = bnsh[vert_start:vert_start + slot_size]
    frag_raw = bnsh[frag_start:frag_start + slot_size]
    return trim(vert_raw), trim(frag_raw)


def pack_sead_binary(control_template: bytes, vert_code: bytes, frag_code: bytes) -> bytes:
    header_size = 0x10
    vert_ctrl_off = header_size
    frag_ctrl_off = vert_ctrl_off + len(control_template)
    vert_code_off = frag_ctrl_off + len(control_template)
    frag_code_off = vert_code_off + len(vert_code)

    header = struct.pack("<IIII", vert_ctrl_off, frag_ctrl_off, vert_code_off, frag_code_off)
    return header + control_template + control_template + vert_code + frag_code


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

    control_template = extract_control_template(bnsh)
    vert_code, frag_code = extract_code_regions(bnsh)
    print(f"control template: {len(control_template)} bytes")
    print(f"vertex code: {len(vert_code)} bytes, fragment code: {len(frag_code)} bytes")

    packed = pack_sead_binary(control_template, vert_code, frag_code)

    out_file = args.out / f"{args.output_name.lower()}_sead_bin.hpp"
    emit_header(packed, args.output_name, out_file)


if __name__ == "__main__":
    main()
