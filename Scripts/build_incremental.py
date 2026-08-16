#!/usr/bin/env python3
"""
LeonEngine2 - Incremental Build Script (Engine tooling).

Builds the configured monorepo project. Prefer build_project.py --project for clarity.
"""

import argparse
import os
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="LeonEngine2 Incremental Build Tool")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--rebuild", action="store_true")
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release", "RelWithDebInfo"])
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", "Projects/Sandbox/Sandbox.lproject"),
        help="Path to .lproject (default: LEON_PROJECT or monorepo Sandbox)",
    )
    parser.add_argument("--target", default="", help="CMake target override")
    args, unknown = parser.parse_known_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_project = os.path.join(script_dir, "build_project.py")
    cmd = [sys.executable, build_project, "--project", args.project, "--config", args.config]
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
