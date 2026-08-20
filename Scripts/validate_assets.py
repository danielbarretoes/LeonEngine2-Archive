#!/usr/bin/env python3
"""
LeonEngine2 — validate native content for a project via LeonAssetTool.
Requires --project (or LEON_PROJECT). Optional --content override.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root, ensure_tool_built, project_content_dir, require_project  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate assets for a LeonEngine2 project")
    parser.add_argument("--project", default=os.environ.get("LEON_PROJECT", ""))
    parser.add_argument("--content", default="", help="Override content directory")
    args = parser.parse_args()

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    content = args.content or project_content_dir(project)
    root = engine_root()

    try:
        tool = ensure_tool_built("AssetTool")
    except SystemExit as e:
        print(e)
        return 1

    cmd = [tool, "validate", "--content", content]
    print(f"[RUN] {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
