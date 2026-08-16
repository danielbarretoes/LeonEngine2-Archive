#!/usr/bin/env python3
"""Build and launch a LeonEngine2 project (.lproject)."""

import os
import subprocess
import sys


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_script = os.path.join(script_dir, "build_project.py")
    cmd = [sys.executable, build_script, "--run"] + sys.argv[1:]
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
