#!/usr/bin/env python3
"""
LeonEngine2 — import raw assets into project Content via LeonAssetTool.
Requires --project (or LEON_PROJECT). Optional --raw / --content overrides.
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
    project_content_dir,
    project_raw_dir,
    require_project,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Import assets for a LeonEngine2 project")
    parser.add_argument("--project", default=os.environ.get("LEON_PROJECT", ""))
    parser.add_argument("--raw", default="", help="Override raw assets directory")
    parser.add_argument("--content", default="", help="Override content output directory")
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    raw = args.raw or project_raw_dir(project)
    content = args.content or project_content_dir(project)
    root = engine_root()

    try:
        tool = ensure_tool_built("AssetTool")
    except SystemExit as e:
        print(e)
        return 1

    cmd = [tool, "import", "--raw", raw, "--content", content]
    if args.force:
        cmd.append("--force")
    print(f"[RUN] {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
