"""
Normalize text file line endings without decoding file contents (safe for GBK / UTF-8).

MSVC C4335: classic Mac uses lone CR (0x0D) as line end -> normalize to LF (UNIX) or CRLF (DOS).

Usage:
  python Scripts/normalize_line_endings_bytes.py path1.cpp path2.h ...
  python Scripts/normalize_line_endings_bytes.py --lf Engine/Src/**/*.cpp   (shell expands)

Default output: LF only (MSVC accepts UNIX format per warning text).
Use --crlf for Windows CRLF after internal LF normalization.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def bytes_to_lf(data: bytes) -> bytes:
    """CRLF -> LF; lone CR -> LF; leave lone LF."""
    out = bytearray()
    n = len(data)
    i = 0
    while i < n:
        c = data[i]
        if c == 0x0D:
            if i + 1 < n and data[i + 1] == 0x0A:
                out.append(0x0A)
                i += 2
            else:
                out.append(0x0A)
                i += 1
        else:
            out.append(c)
            i += 1
    return bytes(out)


def lf_to_crlf(data: bytes) -> bytes:
    return data.replace(b"\n", b"\r\n")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("files", nargs="+", type=Path, help="Files to normalize in place")
    p.add_argument("--crlf", action="store_true", help="Emit CRLF instead of LF")
    args = p.parse_args()

    changed = 0
    for path in args.files:
        if not path.is_file():
            print(f"skip (not a file): {path}", file=sys.stderr)
            continue
        raw = path.read_bytes()
        lf = bytes_to_lf(raw)
        out = lf_to_crlf(lf) if args.crlf else lf
        if out != raw:
            path.write_bytes(out)
            changed += 1
            print(f"normalized: {path}")
    print(f"done; updated {changed} file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
