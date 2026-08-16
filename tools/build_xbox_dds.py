"""Build Xbox runtime DDS textures from the portable BMP source assets.

The game sources remain BMP for the Win32 build.  Xbox uses standard DDS files
with DXT1/DXT5 blocks and a complete mip chain, so it needs far less disk and
UMA memory bandwidth at runtime.
"""

from __future__ import annotations

import io
import struct
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "assets"
OUTPUT = SOURCE / "xbox"

# DDS header flag/caps values from the public DDS file format.
DDSD_MIPMAPCOUNT = 0x00020000
DDSCAPS_COMPLEX = 0x00000008
DDSCAPS_MIPMAP = 0x00400000


def compressed_level(image: Image.Image, pixel_format: str) -> tuple[bytes, bytes]:
    """Return a DDS header and compressed payload for one mip level."""
    stream = io.BytesIO()
    image.save(stream, format="DDS", pixel_format=pixel_format)
    data = stream.getvalue()
    if data[:4] != b"DDS ":
        raise RuntimeError("Pillow did not produce a DDS file")
    return data[:128], data[128:]


def build_mipped_dds(source_name: str, output_name: str, size: int, pixel_format: str) -> None:
    source = Image.open(SOURCE / source_name).convert("RGBA")
    base = source.resize((size, size), Image.Resampling.LANCZOS)
    levels: list[bytes] = []
    image = base
    header: bytes | None = None
    mip_count = 0

    while True:
        level_header, payload = compressed_level(image, pixel_format)
        if header is None:
            header = bytearray(level_header)
        levels.append(payload)
        mip_count += 1
        if image.width == 1 and image.height == 1:
            break
        image = image.resize((max(1, image.width // 2), max(1, image.height // 2)), Image.Resampling.LANCZOS)

    assert header is not None
    flags = struct.unpack_from("<I", header, 8)[0] | DDSD_MIPMAPCOUNT
    caps = struct.unpack_from("<I", header, 108)[0] | DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    struct.pack_into("<I", header, 8, flags)
    struct.pack_into("<I", header, 28, mip_count)
    struct.pack_into("<I", header, 108, caps)

    destination = OUTPUT / output_name
    destination.write_bytes(header + b"".join(levels))
    print(f"{destination.relative_to(ROOT)}: {size}x{size}, {pixel_format}, {mip_count} mips, {destination.stat().st_size} bytes")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    build_mipped_dds("plane_livery.bmp", "plane_livery.dds", 1024, "DXT1")
    build_mipped_dds("terrain_grass.bmp", "terrain_grass.dds", 512, "DXT1")
    build_mipped_dds("runway_asphalt.bmp", "runway_asphalt.dds", 512, "DXT1")
    build_mipped_dds("sky_clouds.bmp", "sky_clouds.dds", 512, "DXT5")


if __name__ == "__main__":
    main()
