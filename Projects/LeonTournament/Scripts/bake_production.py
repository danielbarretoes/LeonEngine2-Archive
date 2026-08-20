#!/usr/bin/env python3
"""Bake LeonTournament lightmaps with Production quality via Engine Scripts."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project = os.path.abspath(os.path.join(here, "..", "LeonTournament.lproject"))
    engine_scripts = os.path.abspath(os.path.join(here, "..", "..", "..", "Scripts"))
    bake = os.path.join(engine_scripts, "bake_lightmaps.py")

    maps = [
        "/Game/Maps/TournamentArena",
        "/Game/Maps/TournamentArenaNight",
        "/Game/Maps/MainMenu",
        "/Game/Maps/AnimLab",
        "/Game/Maps/RenderLab",
    ]
    extra = sys.argv[1:]
    if "--map" not in extra:
        for m in maps:
            cmd = [
                sys.executable,
                bake,
                "--project",
                project,
                "--map",
                m,
                "--quality=Production",
            ] + extra
            print(f"[RUN] {' '.join(cmd)}")
            code = subprocess.run(cmd).returncode
            if code != 0:
                return code
        return 0

    return subprocess.run(
        [sys.executable, bake, "--project", project, "--quality=Production"] + extra
    ).returncode


if __name__ == "__main__":
    sys.exit(main())
