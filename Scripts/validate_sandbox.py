#!/usr/bin/env python3
"""Compatibility wrapper → validate_project.py for the monorepo Sandbox."""

import os
import subprocess
import sys


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    validate = os.path.join(script_dir, "validate_project.py")
    extra = list(sys.argv[1:])
    if not any(a == "--project" or a.startswith("--project=") for a in extra):
        extra = ["--project", "Projects/Sandbox/Sandbox.lproject"] + extra
    return subprocess.run([sys.executable, validate] + extra).returncode


if __name__ == "__main__":
    sys.exit(main())
