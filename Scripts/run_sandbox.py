#!/usr/bin/env python3
"""
LeonEngine2 - Build & Run Sandbox Script
Compiles pending changes and launches Sandbox.exe.
"""

import os
import subprocess
import sys

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_script = os.path.join(script_dir, "build_incremental.py")

    cmd = [sys.executable, build_script, "--run"] + sys.argv[1:]
    sys.exit(subprocess.run(cmd).returncode)

if __name__ == "__main__":
    main()
