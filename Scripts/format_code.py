#!/usr/bin/env python3
"""
LeonEngine2 - Code Formatter
Formats Engine/, Editor/, Plugins/, Projects/, Tests/, Tools/, and optionally an external --project tree.
"""

from __future__ import annotations

import argparse
import glob
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root, project_dir  # noqa: E402

EXTENSIONS = (".hpp", ".h", ".cpp", ".c", ".inl")
EXCLUDE_DIRS = {"ThirdParty", "build", "out", ".cache", ".git", ".vscode", "Legacy", "_deps"}


def find_clang_format() -> str:
    """Find a usable clang-format binary across PATH, Python envs, and Visual Studio."""
    # 1. System PATH
    found = shutil.which("clang-format")
    if found:
        return found

    # 2. Python Scripts directory
    py_dir = os.path.dirname(sys.executable)
    candidates = [
        os.path.join(py_dir, "Scripts", "clang-format.exe"),
        os.path.join(py_dir, "Scripts", "clang-format"),
        os.path.join(py_dir, "bin", "clang-format"),
    ]

    # 3. clang_format python module vendored binary
    try:
        import clang_format

        cf_pkg_dir = os.path.dirname(clang_format.__file__)
        candidates.extend([
            os.path.join(cf_pkg_dir, "data", "bin", "clang-format.exe"),
            os.path.join(cf_pkg_dir, "data", "bin", "clang-format"),
        ])
    except ImportError:
        pass

    # 4. Visual Studio / LLVM installations
    vs_globs = [
        r"C:\Program Files\Microsoft Visual Studio\*\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
        r"C:\Program Files\Microsoft Visual Studio\*\Enterprise\VC\Tools\Llvm\x64\bin\clang-format.exe",
        r"C:\Program Files\Microsoft Visual Studio\*\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe",
        r"C:\Program Files\LLVM\bin\clang-format.exe",
    ]
    for pattern in vs_globs:
        matches = glob.glob(pattern)
        if matches:
            candidates.extend(matches)

    for c in candidates:
        if os.path.isfile(c):
            return os.path.abspath(c)

    return ""


def ensure_clang_format() -> str:
    """Ensure clang-format is available, installing via pip if necessary."""
    exe = find_clang_format()
    if exe:
        return exe

    print("[INFO] clang-format not found. Installing via pip...")
    try:
        subprocess.run([sys.executable, "-m", "pip", "install", "clang-format"], check=True)
    except subprocess.CalledProcessError as e:
        print(f"[ERROR] Failed to install clang-format via pip: {e}")
        return ""

    return find_clang_format()


def main() -> int:
    parser = argparse.ArgumentParser(description="Format LeonEngine2 C/C++ sources")
    parser.add_argument(
        "--project",
        default="",
        help="Optional .lproject — also format that project's Source/ tree",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Dry run: verify formatting without modifying files (returns non-zero if changes needed)",
    )
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)

    print("========================================")
    print("   LeonEngine2 - Code Formatter (Python)")
    print("========================================")

    clang_format_exe = ensure_clang_format()
    if not clang_format_exe:
        print("[ERROR] Could not find or install clang-format.")
        return 1

    try:
        ver_res = subprocess.run([clang_format_exe, "--version"], capture_output=True, text=True, check=True)
        print(f"[TOOL] Using {ver_res.stdout.strip()} ({clang_format_exe})")
    except Exception as e:
        print(f"[WARNING] Could not get clang-format version: {e}")

    target_dirs = [
        os.path.join(root, "Engine"),
        os.path.join(root, "Editor"),
        os.path.join(root, "Plugins"),
        os.path.join(root, "Projects"),
        os.path.join(root, "Tests"),
        os.path.join(root, "Tools"),
    ]
    if args.project:
        from _leon_paths import require_project

        try:
            lp = require_project(args.project)
        except SystemExit as e:
            print(e)
            return 1
        pdir = project_dir(lp)
        if pdir not in target_dirs:
            target_dirs.append(pdir)

    files_to_format = []
    for target_path in target_dirs:
        if not os.path.exists(target_path):
            continue
        for dirpath, dirs, files in os.walk(target_path):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for file in files:
                if file.endswith(EXTENSIONS):
                    files_to_format.append(os.path.join(dirpath, file))

    if not files_to_format:
        print("[INFO] No C/C++ files found to format.")
        return 0

    print(f"[INFO] Found {len(files_to_format)} files across {len(target_dirs)} root trees.")

    start_time = time.perf_counter()
    batch_size = 50
    cmd_base = [clang_format_exe, "-style=file"]
    if not args.check:
        cmd_base.append("-i")
    else:
        cmd_base.append("--dry-run")
        cmd_base.append("-Werror")

    had_error = False
    for i in range(0, len(files_to_format), batch_size):
        batch = files_to_format[i : i + batch_size]
        res = subprocess.run(cmd_base + batch, capture_output=True, text=True)
        if res.returncode != 0:
            had_error = True
            print(f"[ERROR] Batch formatting error (files {i+1}..{i+len(batch)}):")
            if res.stderr:
                print(res.stderr.strip())

    elapsed_ms = (time.perf_counter() - start_time) * 1000.0
    print("----------------------------------------")
    if had_error:
        print(f"[FAILED] Formatting completed with errors in {elapsed_ms:.1f} ms.")
        return 1

    mode_str = "Checked" if args.check else "Formatted"
    print(f"[SUCCESS] {mode_str} {len(files_to_format)} files in {elapsed_ms:.1f} ms.")
    print("========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
