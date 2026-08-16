#!/usr/bin/env python3
"""
Compatibility wrapper: build & run the monorepo Sandbox project.

Prefer: python Scripts/run_project.py --project Projects/Sandbox/Sandbox.lproject
"""

import os
import subprocess
import sys


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    run_project = os.path.join(script_dir, "run_project.py")
    default = "Projects/Sandbox/Sandbox.lproject"
    # Forward args; inject --project if the caller did not pass one
    extra = list(sys.argv[1:])
    if not any(a == "--project" or a.startswith("--project=") for a in extra):
        extra = ["--project", default] + extra
    return subprocess.run([sys.executable, run_project] + extra).returncode


if __name__ == "__main__":
    sys.exit(main())
