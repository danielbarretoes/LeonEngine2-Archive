#!/usr/bin/env python3
"""Build and execute LeonTournament gameplay test suite."""

from __future__ import annotations

import os
import subprocess
import sys


def main() -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    project_dir = os.path.abspath(os.path.join(here, ".."))
    root = os.path.abspath(os.path.join(here, "..", "..", ".."))
    build_dir = os.path.join(root, "out", "Projects", "LeonTournament")

    print("[TEST] Compiling and running LeonTournament integration tests...")
    build_cmd = ["cmake", "--build", build_dir, "--target", "LeonTournamentTests"]
    res = subprocess.run(build_cmd, cwd=root)
    if res.returncode != 0:
        return res.returncode

    exe = os.path.join(build_dir, "_project", "LeonTournamentTests.exe")
    if not os.path.isfile(exe):
        exe = os.path.join(build_dir, "LeonTournamentTests.exe")

    if not os.path.isfile(exe):
        print(f"[ERROR] Test executable not found at {exe}")
        return 1

    return subprocess.run([exe] + sys.argv[1:], cwd=root).returncode


if __name__ == "__main__":
    sys.exit(main())
