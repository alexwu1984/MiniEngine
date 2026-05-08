#!/usr/bin/env python3
"""Generate Resources/MiniEngine.ico (PNG-compressed Vista ICO: 16 + 32 + 48 px)."""
from __future__ import annotations

import struct
import zlib
from pathlib import Path


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def rgba_png(width: int, height: int, rgba_fn) -> bytes:
    """rgba_fn(x,y) -> (r,g,b,a) each 0-255."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type None per scanline
        for x in range(width):
            r, g, b, a = rgba_fn(x, y)
            raw.extend((r, g, b, a))
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(raw), 9)
    return sig + _png_chunk(b"IHDR", ihdr) + _png_chunk(b"IDAT", compressed) + _png_chunk(b"IEND", b"")


def pixel_me(w: int, h: int, x: int, y: int):
    """Dark slate bg + cyan facet + subtle inner glow (MiniEngine \"ME\" vibe)."""
    bg = (22, 32, 41, 255)
    cyan = (0, 232, 198, 255)
    cyan_dim = (0, 140, 120, 255)
    accent = (96, 180, 255, 255)

    # normalized coords
    fx, fy = x / max(w - 1, 1), y / max(h - 1, 1)

    # rounded rect mask (soft via distance)
    cx, cy = 0.5, 0.5
    dx = abs(fx - cx) - 0.38
    dy = abs(fy - cy) - 0.38
    corner_r = 0.08 * min(w, h) / max(w, h)
    q = max(dx, dy)
    inside = q < corner_r + 0.02

    # tilted quad / triangle facet (engine wedge)
    wedge = fy > (fx * 0.55 + 0.22) and fy < (fx * 1.05 + 0.52) and fx < 0.72 and fy < 0.78

    # center diamond pixel spark
    spark = abs(fx - 0.42) + abs(fy - 0.38) < 0.06

    if spark:
        return accent
    if wedge:
        # gradient along wedge
        t = (fx + fy) * 0.5
        blend = min(1.0, max(0.0, (t - 0.35) * 3))
        return tuple(int(cyan[i] * blend + cyan_dim[i] * (1 - blend)) for i in range(4))
    if inside:
        # vignette toward corners
        v = ((fx - 0.5) ** 2 + (fy - 0.5) ** 2) ** 0.5
        dark = min(1.0, v * 1.8)
        return tuple(int(bg[i] * (1 + dark * 0.35)) if i < 3 else 255 for i in range(4))
    return bg


def make_vista_ico(out_path: Path, sizes: tuple[int, ...] = (16, 32, 48)) -> None:
    pngs = []
    for sz in sizes:
        pngs.append(rgba_png(sz, sz, lambda x, y, s=sz: pixel_me(s, s, x, y)))

    # ICONDIR + entries + raw PNG payloads
    buf = bytearray()
    buf.extend(struct.pack("<HHH", 0, 1, len(pngs)))  # reserved, type 1 (icon), count
    offset = 6 + 16 * len(pngs)
    for sz, png in zip(sizes, pngs):
        iw = ih = sz
        buf.extend(
            struct.pack(
                "<BBBBHHII",
                iw if iw < 256 else 0,
                ih if ih < 256 else 0,
                0,
                0,
                1,
                0,
                len(png),
                offset,
            )
        )
        offset += len(png)
    for png in pngs:
        buf.extend(png)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(buf)


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    make_vista_ico(root / "Resources" / "MiniEngine.ico")
