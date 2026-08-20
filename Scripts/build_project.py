#!/usr/bin/env python3
"""
LeonEngine2 — build (and optionally run) a game project against the Engine.

Usage:
  python Scripts/build_project.py --project D:/Games/MyGame/MyGame.lproject
  python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject --run
  set LEON_PROJECT=... && python Scripts/build_project.py --run
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import (  # noqa: E402
    engine_root,
    find_project_executable,
    project_dir,
    project_name,
    require_project,
    safe_rmtree,
)


def print_header(text: str) -> None:
    print("========================================")
    print(f"   LeonEngine2 - {text}")
    print("========================================")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a LeonEngine2 project (.lproject)")
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", ""),
        help="Path to .lproject (or set LEON_PROJECT)",
    )
    parser.add_argument("--target", default="", help="CMake executable target (default: ProjectName)")
    parser.add_argument("--clean", action="store_true", help="Remove out/Projects/<Name> before configure")
    parser.add_argument("--rebuild", action="store_true", help="Clean-first build")
    parser.add_argument("--run", action="store_true", help="Launch the project executable after build")
    parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration",
    )
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    target = args.target or project_name(project)
    abs_project_dir = project_dir(project)
    folder_name = os.path.basename(abs_project_dir)
    cmake_project_dir = abs_project_dir.replace("\\", "/")
    build_dir = os.path.join(root, "out", "Projects", folder_name)

    print_header("Build Project")
    print(f"  Configuration: {args.config}")
    print(f"  Engine Root:    {root}")
    print(f"  Project:        {project}")
    print(f"  Target:         {target}")
    print(f"  Build Dir:      {build_dir}")
    print("----------------------------------------")

    if args.clean and os.path.exists(build_dir):
        print(f"[INFO] Cleaning build directory ({build_dir})...")
        safe_rmtree(build_dir)

    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    need_configure = not os.path.exists(cache_file)
    if not need_configure and os.path.isfile(cache_file):
        cached_dir = ""
        with open(cache_file, encoding="utf-8", errors="ignore") as f:
            for line in f:
                if line.startswith("LEON_PROJECT_DIR:"):
                    cached_dir = line.split("=", 1)[-1].strip().replace("\\", "/")
                    break
        want = cmake_project_dir.rstrip("/")
        have = cached_dir.rstrip("/")
        if have and os.path.normcase(os.path.abspath(have)) != os.path.normcase(os.path.abspath(want)):
            print(f"[INFO] LEON_PROJECT_DIR changed ({have} -> {want}); reconfiguring...")
            need_configure = True

    if need_configure:
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
            "-DLEON_PRODUCT=Project",
            f"-DLEON_PROJECT_DIR={cmake_project_dir}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
        result = subprocess.run(config_cmd)
        if result.returncode != 0:
            print("[ERROR] CMake configuration failed!")
            return result.returncode

    print(f"[INFO] Building target '{target}'...")
    build_cmd = ["cmake", "--build", build_dir, "--config", args.config, "--target", target]
    if args.rebuild and not args.clean:
        build_cmd.append("--clean-first")

    start_time = time.time()
    build_result = subprocess.run(build_cmd)
    elapsed_ms = (time.time() - start_time) * 1000

    if build_result.returncode != 0:
        print("----------------------------------------")
        print(f"[ERROR] Build failed with exit code {build_result.returncode}!")
        print("========================================")
        return build_result.returncode

    exe_path = find_project_executable(build_dir, target, folder_name)
    print("----------------------------------------")
    print(f"[SUCCESS] Build completed in {elapsed_ms:.1f} ms.")
    print(f"  Executable: {exe_path}")
    print("========================================")

    if args.run:
        if not os.path.isfile(exe_path):
            print(f"[ERROR] Executable not found at {exe_path}")
            return 1
        print(f"\n[LAUNCH] {exe_path} --project={project}")
        return subprocess.run([exe_path, f"--project={project}"], cwd=root).returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
