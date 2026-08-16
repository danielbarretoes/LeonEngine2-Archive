#!/usr/bin/env python3
"""
LeonEngine2 - Code Formatter
Formats Engine/, Plugins/, monorepo Projects/, and optionally an external --project tree.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root, project_dir  # noqa: E402

EXTENSIONS = (".hpp", ".h", ".cpp", ".c", ".inl")
EXCLUDE_DIRS = {"ThirdParty", "build", ".cache", ".git", ".vscode"}


def main() -> int:
    parser = argparse.ArgumentParser(description="Format LeonEngine2 C/C++ sources")
    parser.add_argument(
        "--project",
        default="",
        help="Optional .lproject — also format that project's Source/ tree",
    )
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)

    print("========================================")
    print("   LeonEngine2 - Code Formatter (Python)")
    print("========================================")

    try:
        subprocess.run(
            ["clang-format", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("[ERROR] clang-format not found. Installing via pip...")
        subprocess.run([sys.executable, "-m", "pip", "install", "clang-format"], check=True)

    target_dirs = [
        os.path.join(root, "Engine"),
        os.path.join(root, "Plugins"),
        os.path.join(root, "Projects"),
    ]
    if args.project:
        from _leon_paths import require_project

        try:
            lp = require_project(args.project)
        except SystemExit as e:
            print(e)
            return 1
        target_dirs.append(project_dir(lp))

    files_to_format = []
    for target_path in target_dirs:
        if not os.path.exists(target_path):
            continue
        for dirpath, dirs, files in os.walk(target_path):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for file in files:
                if file.endswith(EXTENSIONS):
                    files_to_format.append(os.path.join(dirpath, file))

    start_time = time.time()
    for file_path in files_to_format:
        rel_path = os.path.relpath(file_path, root)
        print(f"  -> Formatting: {rel_path}")
        subprocess.run(["clang-format", "-i", "-style=file", file_path], check=True)

    elapsed = (time.time() - start_time) * 1000
    print("----------------------------------------")
    print(f"[SUCCESS] Formatted {len(files_to_format)} files in {elapsed:.1f} ms.")
    print("========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
