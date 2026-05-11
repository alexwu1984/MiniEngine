"""Sync Render/ShaderLibDX -> runtime output (exe-side ShaderLibDX).

Used by CMake copy_runtime_assets + POST_BUILD. Windows: robocopy (exit code >= 8 = failure).
The top-level ``Built`` directory under source is never copied (/XD Built): offline .cso is produced
per build configuration under dest by build_precompiled_shaders.py.

Elsewhere: mirror with pathlib/shutil (skips source .../Built subtree).

Usage:
  python Tools/sync_shaderlib_dx.py --source Render/ShaderLibDX --dest build/bin/debug/ShaderLibDX
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


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
    dst.mkdir(parents=True, exist_ok=True)

    if sys.platform == "win32":
        r = subprocess.run(
            [
                "robocopy",
                str(src),
                str(dst),
                "/E",
                "/XD",
                "Built",
                "/R:1",
                "/W:1",
                "/NFL",
                "/NDL",
                "/NJH",
                "/NJS",
                "/NP",
            ],
            shell=False,
        )
        rc = r.returncode
        if rc >= 8:
            return rc
        return 0

    # Non-Windows: full tree sync (replace existing files); skip top-level Built/ (offline bytecode lives under dest only).
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
