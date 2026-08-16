#!/usr/bin/env python3
"""Validate Projects/Sandbox via LeonAssetTool validate_project."""

import os
import subprocess
import sys


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(script_dir)
    build_dir = os.path.join(root, "build")
    project = os.path.join(root, "Projects", "Sandbox", "Sandbox.lproject")

    if not os.path.isfile(project):
        print(f"[ERROR] Missing project descriptor: {project}")
        return 1

    tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool.exe")
    if not os.path.isfile(tool):
        tool = os.path.join(build_dir, "Tools", "LeonAssetTool", "LeonAssetTool")
    if not os.path.isfile(tool):
        print("[INFO] Building LeonAssetTool...")
        cfg = os.environ.get("LEON_BUILD_CONFIG", "Debug")
        cache = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.isfile(cache):
            cfg_cmd = [
                "cmake", "-B", "build", "-G", "Ninja",
                f"-DCMAKE_BUILD_TYPE={cfg}",
            ]
            if subprocess.run(cfg_cmd, cwd=root).returncode != 0:
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
    print("[PASSED] Sandbox project validated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
