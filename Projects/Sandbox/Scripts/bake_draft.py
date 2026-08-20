#!/usr/bin/env python3
"""Bake Sandbox lightmaps with Draft quality via Engine Scripts."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.abspath(os.path.join(here, "..", "Sandbox.lproject"))
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    bake = os.path.join(engine_scripts, "bake_lightmaps.py")

    cmd = [
        sys.executable,
        bake,
        "--project",
        project,
        "--map",
        "/Game/Maps/ShowcaseLevel",
        "--quality=Draft",
    ] + sys.argv[1:]
    print(f"[RUN] {' '.join(cmd)}")
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
