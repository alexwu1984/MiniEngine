"""Ensure C++ sources under the repo root are UTF-8 with BOM (byte-safe decode tries).

Skips: ThirdParty/, build/, bin/, .git/, node_modules/, .vs/

Usage (from repo root):
  python Tools/ensure_cpp_utf8_bom.py
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKIP_DIRS = frozenset({"ThirdParty", "build", ".git", "bin", "node_modules", ".vs"})
EXT = frozenset({".cpp", ".cxx", ".cc", ".h", ".hpp", ".hh", ".inl", ".hxx"})
BOM = b"\xef\xbb\xbf"


def should_skip(path: pathlib.Path) -> bool:
    return bool(SKIP_DIRS.intersection(path.parts))


def main() -> int:
    stats = {
        "already_ok": 0,
        "added_bom_utf8": 0,
        "converted_gbk": 0,
        "converted_other": 0,
        "empty_skip": 0,
        "fail": [],
    }

    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in EXT:
            continue
        if should_skip(path):
            continue
        try:
            raw = path.read_bytes()
        except OSError as exc:
            stats["fail"].append((str(path), str(exc)))
            continue
        if not raw:
            stats["empty_skip"] += 1
            continue

        if raw.startswith(BOM):
            try:
                raw.decode("utf-8-sig")
                stats["already_ok"] += 1
                continue
            except UnicodeDecodeError:
                body = raw[3:]
        else:
            body = raw

        text = None
        how = None
        try:
            text = body.decode("utf-8")
            how = "utf8"
        except UnicodeDecodeError:
            pass
        if text is None:
            for enc in ("gbk", "gb2312", "cp936"):
                try:
                    text = body.decode(enc)
                    how = enc
                    break
                except UnicodeDecodeError:
                    continue
        if text is None:
            try:
                text = body.decode("latin-1")
                how = "latin1"
            except Exception:
                stats["fail"].append((str(path), "undecodable"))
                continue

        out = BOM + text.encode("utf-8")
        if out == raw:
            stats["already_ok"] += 1
            continue

        path.write_bytes(out)
        if how == "utf8":
            stats["added_bom_utf8"] += 1
        elif how in ("gbk", "gb2312", "cp936"):
            stats["converted_gbk"] += 1
        else:
            stats["converted_other"] += 1

    print("already_utf8_bom:", stats["already_ok"])
    print("added_bom_was_utf8_no_bom:", stats["added_bom_utf8"])
    print("converted_from_gbk_family:", stats["converted_gbk"])
    print("converted_other:", stats["converted_other"])
    print("empty_skipped:", stats["empty_skip"])
    print("failed:", len(stats["fail"]))
    for p, err in stats["fail"][:40]:
        print(" ", p, err)
    return 1 if stats["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
