#!/usr/bin/env python3
"""
LeonEngine2 — build Engine product (libraries, plugins, tools, tests).

Usage:
  python Scripts/build_engine.py --config Debug
  python Scripts/build_engine.py --clean --config Release
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root, safe_rmtree  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Build LeonEngine2 Engine product")
    parser.add_argument("--clean", action="store_true", help="Remove out/Engine before configure")
    parser.add_argument("--rebuild", action="store_true", help="Clean-first build")
    parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration",
    )
    parser.add_argument(
        "--target",
        default="",
        help="Optional CMake target (default: build all Engine targets)",
    )
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)
    build_dir = os.path.join(root, "out", "Engine")

    print("========================================")
    print("   LeonEngine2 - Build Engine")
    print("========================================")
    print(f"  Configuration: {args.config}")
    print(f"  Build Dir:     {build_dir}")
    print("----------------------------------------")

    if args.clean and os.path.exists(build_dir):
        print(f"[INFO] Cleaning {build_dir}...")
        safe_rmtree(build_dir)

    cache_file = os.path.join(build_dir, "CMakeCache.txt")

    # Detect stale cache missing the MSVC CRT setting and auto-clean.
    if os.path.exists(cache_file):
        with open(cache_file, encoding="utf-8", errors="replace") as f:
            cache_text = f.read()
        if sys.platform == "win32" and "CMAKE_MSVC_RUNTIME_LIBRARY" not in cache_text:
            print("[INFO] CMakeCache.txt is missing CMAKE_MSVC_RUNTIME_LIBRARY — deleting stale cache.")
            safe_rmtree(build_dir)

    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.exists(cache_file):
        print(f"[INFO] Configuring CMake ({args.config})...")
        config_cmd = [
            "cmake",
            "-S",
            ".",
            "-B",
            build_dir,
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            "-DLEON_PRODUCT=Engine",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
        if sys.platform == "win32":
            config_cmd.append("-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
        result = subprocess.run(config_cmd)
        if result.returncode != 0:
            print("[ERROR] CMake configuration failed!")
            return result.returncode

    import multiprocessing
    jobs = max(1, multiprocessing.cpu_count())
    build_cmd = ["cmake", "--build", build_dir, "--config", args.config, "--", f"-j{jobs}"]
    if args.target:
        build_cmd[3:3] = ["--target", args.target]  # insert before --
    if args.rebuild and not args.clean:
        build_cmd.insert(build_cmd.index("--"), "--clean-first")

    print(f"[INFO] Building Engine (--jobs {jobs})...")
    start_time = time.time()
    build_result = subprocess.run(build_cmd)
    elapsed_ms = (time.time() - start_time) * 1000

    if build_result.returncode != 0:
        print("----------------------------------------")
        print(f"[ERROR] Build failed with exit code {build_result.returncode}!")
        print("========================================")
        return build_result.returncode

    print("----------------------------------------")
    print(f"[SUCCESS] Engine build completed in {elapsed_ms:.1f} ms.")
    print(f"  Output: {build_dir}")
    print("========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
