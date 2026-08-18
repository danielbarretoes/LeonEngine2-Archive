#!/usr/bin/env python3
"""Force-bake all static-lit maps in LeonTournament + Sandbox at Production."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BAKE = ROOT / "Scripts" / "bake_lightmaps.py"

JOBS = [
    ("Projects/LeonTournament/LeonTournament.lproject", "/Game/Maps/TournamentArena"),
    ("Projects/LeonTournament/LeonTournament.lproject", "/Game/Maps/TournamentArenaNight"),
    ("Projects/LeonTournament/LeonTournament.lproject", "/Game/Maps/AnimLab"),
    ("Projects/Sandbox/Sandbox.lproject", "/Game/Maps/ShowcaseLevel"),
    ("Projects/Sandbox/Sandbox.lproject", "/Game/Maps/NightLevel"),
]


def main() -> int:
    failed: list[str] = []
    for project, map_path in JOBS:
        cmd = [
            sys.executable,
            str(BAKE),
            "--project",
            str(ROOT / project),
            "--map",
            map_path,
            "--quality=Production",
            "--force",
        ]
        print(f"\n========== BAKE {map_path} ==========", flush=True)
        print(f"[RUN] {' '.join(cmd)}", flush=True)
        code = subprocess.run(cmd, cwd=str(ROOT)).returncode
        if code != 0:
            print(f"[FAIL] {map_path} exit={code}", flush=True)
            failed.append(map_path)
        else:
            print(f"[OK] {map_path}", flush=True)

    print("\n========== SUMMARY ==========", flush=True)
    if failed:
        print("FAILED:", ", ".join(failed), flush=True)
        return 1
    print("All Production bakes succeeded.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
