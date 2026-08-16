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
ASSETS = ROOT / "assets"
SOURCE = ASSETS / "source"
OUTPUT = ASSETS / "xbox"

# DDS header flag/caps values from the public DDS file format.
DDSD_MIPMAPCOUNT = 0x00020000
DDSCAPS_COMPLEX = 0x00000008
DDSCAPS_MIPMAP = 0x00400000


def encode_level(image: Image.Image, pixel_format: str | None) -> tuple[bytes, bytes]:
    """Return a DDS header and payload for one mip level."""
    stream = io.BytesIO()
    options = {"pixel_format": pixel_format} if pixel_format else {}
    image.save(stream, format="DDS", **options)
    data = stream.getvalue()
    if data[:4] != b"DDS ":
        raise RuntimeError("Pillow did not produce a DDS file")
    return data[:128], data[128:]


def remove_green_screen(image: Image.Image) -> Image.Image:
    """Turn the supplied cloud-sheet's green screen into soft transparency."""
    image = image.convert("RGBA")
    pixels = image.load()
    for y in range(image.height):
        for x in range(image.width):
            red, green, blue, _ = pixels[x, y]
            neutral = max(red, blue)
            alpha = 255 if green <= neutral else max(0, min(255, (neutral - 8) * 255 // max(1, green - 8)))
            # The transparent edge previously contained green.  Neutralise it
            # before BC3 compression so it cannot create a green halo in sky.
            pixels[x, y] = (red, min(green, neutral), blue, alpha)
    return image


def build_mipped_dds(source_name: str, output_name: str, size: int, pixel_format: str | None,
                     preprocess=None) -> None:
    source = Image.open(SOURCE / source_name).convert("RGBA")
    if preprocess:
        source = preprocess(source)
    base = source.resize((size, size), Image.Resampling.LANCZOS)
    levels: list[bytes] = []
    image = base
    header: bytes | None = None
    mip_count = 0

    while True:
        level_header, payload = encode_level(image, pixel_format)
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
    format_name = pixel_format or "A8R8G8B8"
    print(f"{destination.relative_to(ROOT)}: {size}x{size}, {format_name}, {mip_count} mips, {destination.stat().st_size} bytes")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    # The livery has fine red pinstripes and panel lines.  Keep it raw so DXT1
    # 4x4 colour blocks cannot soften the original high-resolution UV atlas.
    build_mipped_dds("plane_livery_source.png", "plane_livery.dds", 1024, None)
    build_mipped_dds("terrain_grass_source.png", "terrain_grass.dds", 512, "DXT1")
    build_mipped_dds("runway_asphalt_source.png", "runway_asphalt.dds", 512, "DXT1")
    build_mipped_dds("sky_clouds_green_source.png", "sky_clouds.dds", 512, "DXT5", remove_green_screen)


if __name__ == "__main__":
    main()
