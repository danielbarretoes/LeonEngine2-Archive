#!/usr/bin/env python3
"""
LeonEngine2 — build (and optionally run) a game project against the Engine.

Usage:
  python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject
  python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject --run
  python Scripts/build_project.py --project ... --target Sandbox --config Release
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time


def print_header(text: str) -> None:
    print("========================================")
    print(f"   LeonEngine2 - {text}")
    print("========================================")


def load_project_name(lproject_path: str) -> str:
    try:
        with open(lproject_path, encoding="utf-8") as f:
            data = json.load(f)
        name = data.get("ProjectName", "")
        if name:
            return name
    except (OSError, json.JSONDecodeError):
        pass
    return os.path.splitext(os.path.basename(lproject_path))[0]


def find_executable(build_dir: str, target: str) -> str:
    exe_name = f"{target}.exe" if sys.platform == "win32" else target
    candidates = [
        os.path.join(build_dir, "Projects", target, exe_name),
        os.path.join(build_dir, "Projects", "Sandbox", exe_name),
        os.path.join(build_dir, exe_name),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(description="Build a LeonEngine2 project (.lproject)")
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", ""),
        help="Path to .lproject (or set LEON_PROJECT)",
    )
    parser.add_argument("--target", default="", help="CMake executable target (default: ProjectName from .lproject)")
    parser.add_argument("--clean", action="store_true", help="Remove build/ before configure")
    parser.add_argument("--rebuild", action="store_true", help="Clean-first build")
    parser.add_argument("--run", action="store_true", help="Launch the project executable after build")
    parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration",
    )
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    engine_root = os.path.dirname(script_dir)
    os.chdir(engine_root)

    project = args.project
    if not project:
        print("[ERROR] --project <path-to-.lproject> is required (or set LEON_PROJECT)")
        return 1
    if not os.path.isfile(project):
        print(f"[ERROR] Project file not found: {project}")
        return 1

    target = args.target or load_project_name(project)
    project_dir = os.path.dirname(os.path.abspath(project))
    # Monorepo: pass relative Projects/... dir to CMake when under engine root
    try:
        rel_project_dir = os.path.relpath(project_dir, engine_root)
    except ValueError:
        rel_project_dir = project_dir

    build_dir = os.path.join(engine_root, "build")
    exe_path = find_executable(build_dir, target)

    print_header("Build Project")
    print(f"  Configuration: {args.config}")
    print(f"  Engine Root:    {engine_root}")
    print(f"  Project:        {project}")
    print(f"  Target:         {target}")
    print("----------------------------------------")

    if args.clean and os.path.exists(build_dir):
        print(f"[INFO] Cleaning build directory ({build_dir})...")
        shutil.rmtree(build_dir)

    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.exists(cache_file):
        print(f"[INFO] Configuring CMake ({args.config})...")
        config_cmd = [
            "cmake",
            "-B",
            "build",
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            f"-DLEON_PROJECT_DIR={rel_project_dir.replace(chr(92), '/')}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
        ]
        result = subprocess.run(config_cmd)
        if result.returncode != 0:
            print("[ERROR] CMake configuration failed!")
            return result.returncode

    print(f"[INFO] Building target '{target}'...")
    build_cmd = ["cmake", "--build", "build", "--config", args.config, "--target", target]
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

    exe_path = find_executable(build_dir, target)
    print("----------------------------------------")
    print(f"[SUCCESS] Build completed in {elapsed_ms:.1f} ms.")
    print(f"  Executable: {exe_path}")
    print("========================================")

    if args.run:
        if not os.path.isfile(exe_path):
            print(f"[ERROR] Executable not found at {exe_path}")
            return 1
        print(f"\n[LAUNCH] {exe_path} --project={project}")
        return subprocess.run([exe_path, f"--project={project}"], cwd=engine_root).returncode

    return 0


if __name__ == "__main__":
    sys.exit(main())
