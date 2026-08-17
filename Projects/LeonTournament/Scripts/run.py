#!/usr/bin/env python3
"""Monorepo shortcut: build & run LeonTournament via Engine Scripts.

Extra args are forwarded (for example --anim-lab for the third-person anim lab).
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
    run_project = os.path.join(engine_scripts, "run_project.py")
    return subprocess.run(
        [sys.executable, run_project, "--project", project] + sys.argv[1:]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
