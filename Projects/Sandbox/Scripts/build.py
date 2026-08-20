#!/usr/bin/env python3
"""Build Sandbox executable via Engine Scripts."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.abspath(os.path.join(here, "..", "Sandbox.lproject"))
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    build_project = os.path.join(engine_scripts, "build_project.py")
    return subprocess.run(
        [sys.executable, build_project, "--project", project] + sys.argv[1:]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
