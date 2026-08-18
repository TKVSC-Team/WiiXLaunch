#!/usr/bin/env python3
"""Converts resource files (PNG images and Wavefront OBJ meshes) in src/resources/
into binary payloads for Cemu Graphic Pack runtime loading via Coreinit Filesystem.
"""
import os
import pathlib
import struct
import sys
from PIL import Image


def convert_image_to_bin(image_path: pathlib.Path, out_bin: pathlib.Path):
    img = Image.open(image_path).convert("RGBA")
    width, height = img.size
    rgba_bytes = img.tobytes()

    # 16-byte header: width, height, format (0=RGBA8), dataSize
    header = struct.pack(">IIII", width, height, 0, len(rgba_bytes))
    out_bin.parent.mkdir(parents=True, exist_ok=True)
    with open(out_bin, "wb") as f:
        f.write(header)
        f.write(rgba_bytes)

    print(f"[Resource] Packaged image {image_path.name} -> {out_bin} ({width}x{height}, {len(rgba_bytes) + 16} bytes)")


def convert_obj_to_bin(obj_path: pathlib.Path, out_bin: pathlib.Path):
    positions = []
    normals = []
    tris = []

    for line in obj_path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        tag = parts[0]
        if tag == "v":
            positions.append(tuple(float(c) for c in parts[1:4]))
        elif tag == "vn":
            normals.append(tuple(float(c) for c in parts[1:4]))
        elif tag == "f":
            corners = parts[1:]
            if len(corners) != 3:
                continue
            face = []
            for corner in corners:
                fields = corner.split("/")
                pos_idx = int(fields[0]) - 1
                nrm_idx = int(fields[2]) - 1 if len(fields) >= 3 and fields[2] else None
                face.append((pos_idx, nrm_idx))
            tris.append(face)

    # Flatten into 8 floats per vertex: (x, y, z, 1.0, nx, ny, nz, 0.0)
    vertex_bytes = bytearray()
    vertex_count = len(tris) * 3

    for face in tris:
        for pos_idx, nrm_idx in face:
            px, py, pz = positions[pos_idx]
            nx, ny, nz = normals[nrm_idx] if nrm_idx is not None else (0.0, 1.0, 0.0)
            # PowerPC big-endian floats
            vertex_bytes.extend(struct.pack(">ffffffff", px, py, pz, 1.0, nx, ny, nz, 0.0))

    # 16-byte header: vertex_count, float stride (8), dataSize, 0
    header = struct.pack(">IIII", vertex_count, 8, len(vertex_bytes), 0)
    out_bin.parent.mkdir(parents=True, exist_ok=True)
    with open(out_bin, "wb") as f:
        f.write(header)
        f.write(vertex_bytes)

    print(f"[Resource] Packaged mesh {obj_path.name} -> {out_bin} ({vertex_count} vertices, {len(vertex_bytes) + 16} bytes)")


def process_resources(resources_dir: pathlib.Path, output_dir: pathlib.Path):
    if not resources_dir.exists():
        return

    output_dir.mkdir(parents=True, exist_ok=True)

    for item in resources_dir.iterdir():
        if item.suffix.lower() in [".png", ".jpg", ".jpeg"]:
            out_path = output_dir / f"{item.stem}.bin"
            convert_image_to_bin(item, out_path)
        elif item.suffix.lower() == ".obj":
            out_path = output_dir / f"{item.stem}.bin"
            convert_obj_to_bin(item, out_path)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python pack_resources.py <src_resources_dir> <output_content_dir>")
        sys.exit(1)
    process_resources(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
