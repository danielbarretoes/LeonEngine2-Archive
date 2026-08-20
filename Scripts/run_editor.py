#!/usr/bin/env python3
"""Build and launch the LeonEngine2 Editor (LeonEditor)."""

import os
import subprocess
import sys


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_script = os.path.join(script_dir, "build_editor.py")
    cmd = [sys.executable, build_script, "--run"] + sys.argv[1:]
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
