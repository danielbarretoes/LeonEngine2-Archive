#!/usr/bin/env python3
"""Validate a LeonEngine2 project via LeonAssetTool validate_project."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root, ensure_tool_built, require_project  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a LeonEngine2 .lproject")
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", ""),
        help="Path to .lproject (or set LEON_PROJECT)",
    )
    args = parser.parse_args()

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    root = engine_root()
    try:
        tool = ensure_tool_built("ProjectTool")
    except SystemExit as e:
        print(e)
        return 1

    cmd = [tool, "validate_project", "--project", project]
    print(f"[RUN] {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=root)
    if result.returncode != 0:
        print("[FAILED] validate_project")
        return result.returncode
    print("[PASSED] Project validated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
