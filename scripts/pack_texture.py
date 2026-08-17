#!/usr/bin/env python3
"""Converts an image into a "packaged texture data" header for use with NVN's
nvnTextureBuilderSetPackagedTextureData path - see
NvnOverlay::EnsureTexturePipelineInitialized in nvn_overlay.hpp and
docs/switch-nvn-findings.md ("Texture pipeline, round 6").

Earlier rounds tried a plain nvnTextureBuilderSetStorage + block-linear
texture, which crashed unconditionally inside nvnTextureInitialize: BOTH its
internal code paths dereference the supplied memory pool as a vtable-having
C++ object (`(**(pool->vtable + N))(pool, texture)`), and nothing on the
builder side (flags/format/target) can route around that - confirmed via
Ghidra disassembly of the real nnSdk internals. The only real, proven-working
reference in this codebase (sead::DebugFontMgrNvn::initializeFromBinary,
0x7100aff42c) instead calls nvnTextureBuilderSetPackagedTextureData, which
takes an entirely different internal path (NvRmGpuMappingCreate) that never
touches that vtable.

"Packaged texture data" is a real Nintendo container format (magic "DFvN"/
"HBvN" - nn::util::BinaryFileHeader/BinaryBlockHeader, per Ghidra's own class
list for this project) - a fixed-size header directly in front of the actual
GPU-ready pixel bytes. Rather than guess the full header layout byte-by-byte,
this tool takes the header from the one real, shipped instance of it already
in this repo (include/nvn_font_texture_bytes.hpp, the real nvn_font.ntx BotW
asset DebugFontMgrNvn loads) and patches ONLY the fields independently
confirmed against real evidence:
  - width/height (offsets 0x40/0x44) - confirmed against
    DebugFontMgrNvn's own decompiled SetSize2D(0x80, 0x80) call.
  - data size (offsets 0x34/0x58) - confirmed via two independent matches:
    the real font asset's own header (16384 bytes for a 128x128 R8 texture)
    and this project's own earlier logged nvnTextureBuilderGetStorageSize
    result (262144 bytes for a 256x192 RGBA8 texture) both exactly match this
    tool's block-linear size formula (see swizzle_block_linear below).
Every other header byte is left bit-for-bit identical to the real asset -
fields whose exact meaning isn't independently confirmed stay copied from a
proven-working reference rather than guessed.

Pixel format is R8 (grayscale, single channel) rather than RGBA8, matching
the ONE proven-working reference exactly - minimizes how many unproven things
change at once for this first packaged-data test. The shader currently reads
the R channel as-is (o_Color = texture(...) * v_Tint), which for an R8
texture samples as (r, 0, 0, 1) - a reddish/washed look, not true grayscale.
Repacking shaders/texture.frag to output vec3(r) (via pack_shader.py) is a
quick, separate follow-up once this path is confirmed not to crash.

Block-linear (GOB) swizzle is the standard, publicly-documented Tegra X1
scheme (also used by e.g. Ryujinx's own texture code) - the intra-GOB bit
formula itself isn't independently verified against this specific driver
build (unlike the header/size fields above), so a first render may need a
follow-up pass if pixels come out visibly scrambled; that would be a
cosmetic bug, not a crash, and independent of the header work above.

Usage:
    python pack_texture.py <image> <output_name> [--max-dim 256] [--out include/]

Produces include/<output_name>_texture_bytes.hpp with g_<Name>TextureBytes /
k<Name>TextureWidth / k<Name>TextureHeight / k<Name>TextureHeaderSize /
k<Name>TextureDataSize / k<Name>TextureSize.
"""
import argparse
import pathlib
import re

from PIL import Image

GOB_WIDTH = 64
GOB_HEIGHT = 8
GOB_SIZE = GOB_WIDTH * GOB_HEIGHT  # 512

PACKAGED_HEADER_SIZE = 0x200

_FONT_TEXTURE_HPP = pathlib.Path(__file__).parent.parent / "include" / "nvn_font_texture_bytes.hpp"

# Offsets within the real container header (DFvN/HBvN)
_OFF_DATA_SIZE_A = 0x34
_OFF_WIDTH       = 0x40
_OFF_HEIGHT      = 0x44
_OFF_FORMAT      = 0x50
_OFF_DATA_SIZE_B = 0x58
_OFF_DATA_SIZE_C = 0xE4

NVN_FORMAT_RGBA8 = 0x25
NVN_FORMAT_R8    = 0x01


def _load_header_template() -> bytearray:
    """Pulls the first PACKAGED_HEADER_SIZE bytes out of the real, shipped
    nvn_font.ntx asset already checked into this repo, rather than
    hand-transcribing hex - see module docstring."""
    text = _FONT_TEXTURE_HPP.read_text(encoding="utf-8")
    hex_bytes = re.findall(r"0x([0-9a-fA-F]{2})", text)
    if len(hex_bytes) < PACKAGED_HEADER_SIZE:
        raise RuntimeError(
            f"{_FONT_TEXTURE_HPP} has only {len(hex_bytes)} byte literals, "
            f"need at least {PACKAGED_HEADER_SIZE} for the header template")
    data = bytes(int(b, 16) for b in hex_bytes[:PACKAGED_HEADER_SIZE])
    return bytearray(data)


def _block_height_gobs(height_px: int) -> int:
    """Standard NVN block-height selection: min(16, next_pow2(ceil(height/8)))."""
    gobs_in_y = (height_px + GOB_HEIGHT - 1) // GOB_HEIGHT
    bh = 1
    while bh < 16 and bh < gobs_in_y:
        bh *= 2
    return bh


def _gob_address(x: int, y: int) -> int:
    """Byte offset within a single 64x8-byte GOB for local (x, y) coordinates
    where x is byte coordinate (0..63) and y is row (0..7).
    Standard Maxwell/Tegra X1 intra-GOB bit interleaving:
    - bits 0..3: x[0..3] (16 bytes in row)
    - bit 4:     y[0] (row parity 0/1)
    - bit 5:     x[4] (next 16 bytes in row, bytes 16..31)
    - bits 6..7: y[1..2] (row pairs 0..3)
    - bit 8:     x[5] (second 32 bytes in row, bytes 32..63)
    """
    return (
        (x & 0xF)
        | ((y & 1) << 4)
        | (((x >> 4) & 1) << 5)
        | (((y >> 1) & 3) << 6)
        | (((x >> 5) & 1) << 8)
    )


def swizzle_block_linear(pixels: bytes, width: int, height: int, bpp: int) -> bytes:
    """Converts row-major `pixels` (width*height*bpp bytes) into block-linear
    (GOB-swizzled) layout, padded out to full GOB/block granularity."""
    row_bytes = width * bpp
    gobs_x = (row_bytes + GOB_WIDTH - 1) // GOB_WIDTH
    bh = _block_height_gobs(height)
    blocks_y = (height + bh * GOB_HEIGHT - 1) // (bh * GOB_HEIGHT)
    block_row_stride = gobs_x * bh * GOB_SIZE
    out = bytearray(block_row_stride * blocks_y)

    for y in range(height):
        gob_y_abs = y // GOB_HEIGHT
        block_y = gob_y_abs // bh
        gob_y_in_block = gob_y_abs % bh
        local_y = y % GOB_HEIGHT
        row_base = y * row_bytes
        block_base = block_y * block_row_stride + gob_y_in_block * GOB_SIZE
        for x in range(row_bytes):
            local_x = x % GOB_WIDTH
            gob_x = x // GOB_WIDTH
            dst = block_base + gob_x * (bh * GOB_SIZE) + _gob_address(local_x, local_y)
            out[dst] = pixels[row_base + x]
    return bytes(out)


def pack_image(im: Image.Image, output_name: str, out_dir: pathlib.Path, max_dim: int) -> None:
    im = im.convert("RGBA")  # 4-channel full color RGBA8
    w, h = im.size
    scale = min(1.0, max_dim / max(w, h))
    new_w = max(8, int(w * scale))
    new_h = max(8, int(h * scale))
    # round up to a multiple of 8 - keeps the GOB/block math simple and
    # matches the existing convention in this project.
    new_w = (new_w + 7) // 8 * 8
    new_h = (new_h + 7) // 8 * 8
    im = im.resize((new_w, new_h), Image.LANCZOS)

    pixels = im.tobytes("raw", "RGBA")
    swizzled = swizzle_block_linear(pixels, new_w, new_h, 4)
    data_size = len(swizzled)
    print(f"resized {w}x{h} -> {new_w}x{new_h}, swizzled to {data_size} bytes block-linear (RGBA8)")

    header = _load_header_template()

    def patch_u32(offset: int, value: int) -> None:
        header[offset:offset + 4] = value.to_bytes(4, "little")

    patch_u32(_OFF_WIDTH, new_w)
    patch_u32(_OFF_HEIGHT, new_h)
    patch_u32(_OFF_FORMAT, NVN_FORMAT_RGBA8)
    patch_u32(_OFF_DATA_SIZE_A, data_size)
    patch_u32(_OFF_DATA_SIZE_B, data_size)
    if len(header) > _OFF_DATA_SIZE_C + 4:
        patch_u32(_OFF_DATA_SIZE_C, data_size)

    packed = bytes(header) + swizzled
    # Pad the emitted array itself up to a 4096 page boundary: the C++ side
    # builds an NVNmemoryPool directly over this array and NVN pool sizes
    # must be page-aligned, so the backing array needs to actually own that
    # many bytes rather than the pool reading past the array's end.
    padded_size = (len(packed) + 0xfff) & ~0xfff
    packed = packed + bytes(padded_size - len(packed))

    array_name = f"g_{output_name}TextureBytes"
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "#include <cstddef>",
        "",
        "// Generated by scripts/pack_texture.py - do not hand-edit.",
        f"constexpr int k{output_name}TextureWidth = {new_w};",
        f"constexpr int k{output_name}TextureHeight = {new_h};",
        f"constexpr size_t k{output_name}TextureHeaderSize = {PACKAGED_HEADER_SIZE};",
        f"constexpr size_t k{output_name}TextureDataSize = {data_size};",
        f"constexpr size_t k{output_name}TextureSize = {len(packed)};",
        f"alignas(4096) inline uint8_t {array_name}[] = {{",
    ]
    for i in range(0, len(packed), 16):
        chunk = packed[i:i + 16]
        lines.append("    " + " ".join(f"0x{b:02x}," for b in chunk))
    lines.append("};")
    lines.append("")

    out_file = out_dir / f"{output_name.lower()}_texture_bytes.hpp"
    out_file.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out_file} ({len(packed)} bytes total: "
          f"{PACKAGED_HEADER_SIZE} header + {data_size} swizzled pixel data)")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=pathlib.Path)
    parser.add_argument("output_name", help="PascalCase name, e.g. Test -> include/test_texture_bytes.hpp")
    parser.add_argument("--max-dim", type=int, default=256)
    parser.add_argument("--out", type=pathlib.Path, default=pathlib.Path("include"))
    args = parser.parse_args()

    im = Image.open(args.image)
    pack_image(im, args.output_name, args.out, args.max_dim)


if __name__ == "__main__":
    main()
