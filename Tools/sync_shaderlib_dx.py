"""Copy ShaderLibDX to runtime dir (CMake POST_BUILD). Skips source ``Built/`` (offline .cso goes to dest via precompile)."""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


def _sync_shader_tree(src: Path, dst: Path) -> None:
    dst.mkdir(parents=True, exist_ok=True)
    for path in src.rglob("*"):
        rel = path.relative_to(src)
        if rel.parts and rel.parts[0] == "Built":
            continue
        out = dst / rel
        if path.is_dir():
            out.mkdir(parents=True, exist_ok=True)
        else:
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, out)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source", required=True, help="ShaderLibDX source directory")
    p.add_argument("--dest", required=True, help="Destination directory (created if missing)")
    args = p.parse_args()
    src = Path(args.source).resolve()
    dst = Path(args.dest).resolve()
    if not src.is_dir():
        print(f"SourceDir not found: {src}", file=sys.stderr)
        return 1
    try:
        _sync_shader_tree(src, dst)
    except OSError as e:
        print(f"sync_shaderlib_dx: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
