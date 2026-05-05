"""Normalize shader sources to UTF-8 without BOM (strip signature if present).

Tries UTF-8 first, then GBK family (common in Chinese toolchains), then latin-1.
Scans Render/ShaderLibDX and ThirdParty/DirectXTex/DirectXTex/Shaders.

Usage (from repo root):
  python Tools/ensure_shader_utf8.py
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

SHADER_ROOTS = [
    ROOT / "Render" / "ShaderLibDX",
    ROOT / "ThirdParty" / "DirectXTex" / "DirectXTex" / "Shaders",
]

EXT = frozenset({".hlsl", ".hlsli", ".xsh", ".xsf"})
BOM = b"\xef\xbb\xbf"


def iter_shader_files() -> list[pathlib.Path]:
    out: list[pathlib.Path] = []
    for base in SHADER_ROOTS:
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in EXT:
                out.append(path)
    return out


def decode_body(body: bytes) -> tuple[str, str]:
    try:
        return body.decode("utf-8"), "utf8"
    except UnicodeDecodeError:
        pass
    for enc in ("gbk", "gb2312", "cp936"):
        try:
            return body.decode(enc), enc
        except UnicodeDecodeError:
            continue
    return body.decode("latin-1"), "latin1"


def main() -> int:
    stats = {
        "unchanged": 0,
        "rewrote_utf8_strip_bom": 0,
        "converted_gbk_family": 0,
        "converted_other": 0,
        "empty_skip": 0,
        "fail": [],
    }

    paths = iter_shader_files()
    if not paths:
        print("no shader files found (check SHADER_ROOTS)")
        return 1

    for path in paths:
        try:
            raw = path.read_bytes()
        except OSError as exc:
            stats["fail"].append((str(path), str(exc)))
            continue
        if not raw:
            stats["empty_skip"] += 1
            continue

        if raw.startswith(BOM):
            body = raw[3:]
            had_bom = True
        else:
            body = raw
            had_bom = False

        try:
            text, how = decode_body(body)
        except UnicodeError:
            stats["fail"].append((str(path), "undecodable"))
            continue

        out_bytes = text.encode("utf-8")
        if out_bytes == raw:
            stats["unchanged"] += 1
            continue

        path.write_bytes(out_bytes)
        if had_bom and how == "utf8":
            stats["rewrote_utf8_strip_bom"] += 1
        elif how in ("gbk", "gb2312", "cp936"):
            stats["converted_gbk_family"] += 1
        else:
            stats["converted_other"] += 1

    print("total_files:", len(paths))
    print("unchanged:", stats["unchanged"])
    print("rewrote_utf8_strip_bom:", stats["rewrote_utf8_strip_bom"])
    print("converted_from_gbk_family:", stats["converted_gbk_family"])
    print("converted_other_encoding:", stats["converted_other"])
    print("empty_skipped:", stats["empty_skip"])
    print("failed:", len(stats["fail"]))
    for p, err in stats["fail"][:40]:
        print(" ", p, err)
    return 1 if stats["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
