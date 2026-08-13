#!/usr/bin/env python3
"""
LeonEngine2 - Code Formatter (Python)
Scans and formats Engine/, Plugins/, and Projects/ using clang-format.
"""

import os
import subprocess
import sys
import time

TARGET_DIRS = ["Engine", "Plugins", "Projects"]
EXTENSIONS = (".hpp", ".h", ".cpp", ".c", ".inl")
EXCLUDE_DIRS = {"ThirdParty", "build", ".cache", ".git", ".vscode"}

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    os.chdir(project_root)

    print("========================================")
    print("   LeonEngine2 - Code Formatter (Python)")
    print("========================================")

    # Check clang-format
    try:
        subprocess.run(["clang-format", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except (FileNotFoundError, subprocess.CalledProcessError):
        print("[ERROR] clang-format not found. Installing via pip...")
        subprocess.run([sys.executable, "-m", "pip", "install", "clang-format"], check=True)

    files_to_format = []
    for target in TARGET_DIRS:
        target_path = os.path.join(project_root, target)
        if not os.path.exists(target_path):
            continue
        for root, dirs, files in os.walk(target_path):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for file in files:
                if file.endswith(EXTENSIONS):
                    files_to_format.append(os.path.join(root, file))

    start_time = time.time()
    for file_path in files_to_format:
        rel_path = os.path.relpath(file_path, project_root)
        print(f"  -> Formatting: {rel_path}")
        subprocess.run(["clang-format", "-i", "-style=file", file_path], check=True)

    elapsed = (time.time() - start_time) * 1000
    print("----------------------------------------")
    print(f"[SUCCESS] Formatted {len(files_to_format)} files in {elapsed:.1f} ms.")
    print("========================================")

if __name__ == "__main__":
    main()
