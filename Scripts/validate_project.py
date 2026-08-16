#!/usr/bin/env python3
"""Validate a LeonEngine2 project via LeonAssetTool validate_project."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate a LeonEngine2 .lproject")
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", "Projects/Sandbox/Sandbox.lproject"),
        help="Path to .lproject (default: LEON_PROJECT or Sandbox monorepo path)",
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(script_dir)
    project = args.project
    if not os.path.isabs(project):
        project = os.path.join(root, project)

    if not os.path.isfile(project):
        print(f"[ERROR] Missing project descriptor: {project}")
        return 1

    build_dir = os.path.join(root, "build")
    tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool.exe")
    if not os.path.isfile(tool):
        tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool")
    if not os.path.isfile(tool):
        print("[INFO] Building LeonAssetTool...")
        cfg = os.environ.get("LEON_BUILD_CONFIG", "Debug")
        cache = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.isfile(cache):
            if subprocess.run(
                ["cmake", "-B", "build", "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={cfg}"],
                cwd=root,
            ).returncode != 0:
                return 1
        if subprocess.run(["ninja", "-C", build_dir, "LeonAssetTool"], cwd=root).returncode != 0:
            print("[ERROR] Failed to build LeonAssetTool")
            return 1
        if not os.path.isfile(tool):
            tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool.exe")
        if not os.path.isfile(tool):
            tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool")
        if not os.path.isfile(tool):
            print("[ERROR] LeonAssetTool executable not found after build")
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
