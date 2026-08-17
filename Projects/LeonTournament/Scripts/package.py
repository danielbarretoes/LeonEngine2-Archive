#!/usr/bin/env python3
"""Monorepo shortcut: Shipping package for LeonTournament via Engine Scripts.

Produces a distributable folder + Win64 zip under Dist/Shipping/.
"""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.join(here, "..", "LeonTournament.lproject")
    project = os.path.abspath(project)
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    package_project = os.path.join(engine_scripts, "package_project.py")
    return subprocess.run(
        [sys.executable, package_project, "--project", project] + sys.argv[1:]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
