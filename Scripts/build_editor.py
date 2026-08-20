#!/usr/bin/env python3
"""
LeonEngine2 — build Editor product (ImGui skeleton over LeonEngineCore).

Usage:
  python Scripts/build_editor.py --config Debug
  python Scripts/build_editor.py --run
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root  # noqa: E402


def find_editor_executable(build_dir: str) -> str:
    exe_name = "LeonEditor.exe" if sys.platform == "win32" else "LeonEditor"
    candidates = [
        os.path.join(build_dir, exe_name),
        os.path.join(build_dir, "Debug", exe_name),
        os.path.join(build_dir, "Release", exe_name),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(description="Build LeonEngine2 Editor product")
    parser.add_argument("--clean", action="store_true", help="Remove out/Editor before configure")
    parser.add_argument("--rebuild", action="store_true", help="Clean-first build")
    parser.add_argument("--run", action="store_true", help="Launch LeonEditor after successful build")
    parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration",
    )
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)
    build_dir = os.path.join(root, "out", "Editor")
    editor_source = os.path.join(root, "Editor")

    print("========================================")
    print("   LeonEngine2 - Build Editor")
    print("========================================")
    print(f"  Configuration: {args.config}")
    print(f"  Build Dir:     {build_dir}")
    print("----------------------------------------")

    if args.clean and os.path.exists(build_dir):
        print(f"[INFO] Cleaning {build_dir}...")
        safe_rmtree(build_dir)

    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.exists(cache_file):
        print(f"[INFO] Configuring CMake ({args.config})...")
        config_cmd = [
            "cmake",
            "-S",
            editor_source,
            "-B",
            build_dir,
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
        result = subprocess.run(config_cmd)
        if result.returncode != 0:
            print("[ERROR] CMake configuration failed!")
            return result.returncode

    build_cmd = ["cmake", "--build", build_dir, "--config", args.config, "--target", "LeonEditor"]
    if args.rebuild and not args.clean:
        build_cmd.append("--clean-first")

    print("[INFO] Building LeonEditor...")
    start_time = time.time()
    build_result = subprocess.run(build_cmd)
    elapsed_ms = (time.time() - start_time) * 1000

    if build_result.returncode != 0:
        print("----------------------------------------")
        print(f"[ERROR] Build failed with exit code {build_result.returncode}!")
        print("========================================")
        return build_result.returncode

    exe_path = find_editor_executable(build_dir)
    print("----------------------------------------")
    print(f"[SUCCESS] Editor build completed in {elapsed_ms:.1f} ms.")
    print(f"  Executable: {exe_path}")
    print("========================================")

    if args.run:
        if not os.path.isfile(exe_path):
            print(f"[ERROR] Executable not found at {exe_path}")
            return 1
        # Run from exe dir so POST_BUILD Engine/Resources resolve.
        return subprocess.run([exe_path], cwd=os.path.dirname(exe_path)).returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
