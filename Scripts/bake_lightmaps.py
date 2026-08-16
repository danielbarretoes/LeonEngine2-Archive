#!/usr/bin/env python3
"""
Bake (or validate) lightmaps for a LeonEngine2 project map.

Usage:
  python Scripts/bake_lightmaps.py --project Projects/Sandbox/Sandbox.lproject
  python Scripts/bake_lightmaps.py --project ... --map /Game/Maps/ShowcaseLevel --force
  python Scripts/bake_lightmaps.py --project ... --validate-only
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import (  # noqa: E402
    engine_root,
    ensure_tool_built,
    require_project,
    resolve_default_map_path,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Bake lightmaps for a LeonEngine2 project")
    parser.add_argument("--project", default=os.environ.get("LEON_PROJECT", ""))
    parser.add_argument("--map", default="", help="Map path, /Game/Maps/Name, or omit for DefaultMap")
    parser.add_argument("--force", action="store_true")
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="Run validate_lightmaps instead of bake",
    )
    args = parser.parse_args()

    try:
        project = require_project(args.project)
        map_path = resolve_default_map_path(project, args.map or None)
        tool = ensure_tool_built("LeonAssetTool")
    except SystemExit as e:
        print(e)
        return 1

    root = engine_root()
    if args.validate_only:
        cmd = [tool, "validate_lightmaps", "--map", map_path]
    else:
        cmd = [tool, "bake_lightmaps", "--map", map_path]
        if args.force:
            cmd.append("--force")

    print(f"[RUN] {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
