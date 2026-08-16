#!/usr/bin/env python3
"""Monorepo shortcut: validate Sandbox via Engine Scripts."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.abspath(os.path.join(here, "..", "Sandbox.lproject"))
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    validate = os.path.join(engine_scripts, "validate_project.py")
    return subprocess.run(
        [sys.executable, validate, "--project", project] + sys.argv[1:]
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
