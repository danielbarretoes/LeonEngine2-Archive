#!/usr/bin/env python3
"""Monorepo shortcut: build & run Sandbox via Engine Scripts (not Engine tooling)."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.join(here, "..", "Sandbox.lproject")
    project = os.path.abspath(project)
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    run_project = os.path.join(engine_scripts, "run_project.py")
    return subprocess.run(
        [sys.executable, run_project, "--project", project] + sys.argv[1:]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
