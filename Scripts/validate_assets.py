#!/usr/bin/env python3
"""
LeonEngine2 - Python Asset Validation Tool Wrapper
Validates imported assets for LeonEngine2.
"""

import os
import sys
import subprocess
import argparse

def main():
    parser = argparse.ArgumentParser(description="Validate assets for LeonEngine2")
    parser.add_argument("--content", default="Projects/Sandbox/Content", help="Output content path (default: Projects/Sandbox/Content)")
    parser.add_argument("--tool", default="build/Tools/LeonAssetTool/LeonAssetTool.exe", help="Path to LeonAssetTool binary")
    args = parser.parse_args()

    project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    tool_path = os.path.join(project_root, args.tool)

    if not os.path.exists(tool_path):
        print(f"[ERROR] Asset tool binary not found at: {tool_path}")
        print("[INFO] Building project first...")
        res = subprocess.run(["cmake", "--build", "build", "--target", "LeonAssetTool"], cwd=project_root)
        if res.returncode != 0:
            print("[ERROR] Failed to build LeonAssetTool.")
            sys.exit(1)

    cmd = [tool_path, "validate", "--content", args.content]

    print(f"[RUN] {' '.join(cmd)}")
    res = subprocess.run(cmd, cwd=project_root)
    sys.exit(res.returncode)

if __name__ == "__main__":
    main()
