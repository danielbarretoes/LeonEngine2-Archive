#!/usr/bin/env python3
"""
LeonEngine2 - Incremental Build Script
Compiles only modified files since last build across platforms.
"""

import argparse
import os
import shutil
import subprocess
import sys
import time

def print_header(text: str):
    print("========================================")
    print(f"   LeonEngine2 - {text}")
    print("========================================")

def main():
    parser = argparse.ArgumentParser(description="LeonEngine2 Incremental Build Tool")
    parser.add_argument("--clean", action="store_true", help="Remove build/ directory before configuring and building.")
    parser.add_argument("--rebuild", action="store_true", help="Clean existing build targets and rebuild from scratch.")
    parser.add_argument("--run", action="store_true", help="Launch Sandbox.exe after successful build.")
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release", "RelWithDebInfo"], help="Build configuration.")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    os.chdir(project_root)

    build_dir = os.path.join(project_root, "build")
    exe_path = os.path.join(build_dir, "Projects", "Sandbox", "Sandbox.exe" if sys.platform == "win32" else "Sandbox")

    print_header("Incremental Build (Python)")
    print(f"  Configuration: {args.config}")
    print(f"  Project Root:  {project_root}")
    print("----------------------------------------")

    # 1. Clean if requested
    if args.clean and os.path.exists(build_dir):
        print(f"[INFO] Cleaning build directory ({build_dir})...")
        shutil.rmtree(build_dir)

    # 2. Configure CMake if build directory does not exist or CMakeCache.txt is missing
    cache_file = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.exists(cache_file):
        print(f"[INFO] Configuring CMake ({args.config})...")
        config_cmd = [
            "cmake", "-B", "build",
            "-G", "Ninja",
            f"-DCMAKE_BUILD_TYPE={args.config}",
            "-DCMAKE_EXPORT_COMPILE_COMMANDON"
        ]
        result = subprocess.run(config_cmd)
        if result.returncode != 0:
            print("[ERROR] CMake configuration failed!")
            sys.exit(result.returncode)

    # 3. Build
    print("[INFO] Building LeonEngine2...")
    build_cmd = ["cmake", "--build", "build", "--config", args.config]
    if args.rebuild and not args.clean:
        build_cmd.append("--clean-first")

    start_time = time.time()
    build_result = subprocess.run(build_cmd)
    elapsed_ms = (time.time() - start_time) * 1000

    if build_result.returncode != 0:
        print("----------------------------------------")
        print(f"[ERROR] Build failed with exit code {build_result.returncode}!")
        print("========================================")
        sys.exit(build_result.returncode)

    print("----------------------------------------")
    print(f"[SUCCESS] Build completed in {elapsed_ms:.1f} ms.")
    print(f"  Executable: {exe_path}")
    print("========================================")

    # 4. Run if requested
    if args.run:
        if os.path.exists(exe_path):
            print("\n[LAUNCH] Starting Sandbox executable...")
            subprocess.run([exe_path])
        else:
            print(f"[ERROR] Executable not found at {exe_path}")

if __name__ == "__main__":
    main()
