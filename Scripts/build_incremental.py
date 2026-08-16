#!/usr/bin/env python3
"""
LeonEngine2 - Incremental Build Script (thin alias of build_project.py).
Requires --project or LEON_PROJECT.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import require_project  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="LeonEngine2 Incremental Build Tool")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--rebuild", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release", "RelWithDebInfo"])
    parser.add_argument("--project", default=os.environ.get("LEON_PROJECT", ""))
    parser.add_argument("--target", default="", help="CMake target override")
    args, unknown = parser.parse_known_args()

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_project = os.path.join(script_dir, "build_project.py")
    cmd = [sys.executable, build_project, "--project", project, "--config", args.config]
    if args.clean:
        cmd.append("--clean")
    if args.rebuild:
        cmd.append("--rebuild")
    if args.run:
        cmd.append("--run")
    if args.target:
        cmd.extend(["--target", args.target])
    cmd.extend(unknown)
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
