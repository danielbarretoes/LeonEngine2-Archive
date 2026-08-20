#!/usr/bin/env python3
"""
LeonEngine2 - Clean Rebuild (From Scratch)
Deletes out/Projects/<Name> (via --clean) and rebuilds the given project via build_project.py.
Requires --project or LEON_PROJECT.
"""

from __future__ import annotations

import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import require_project  # noqa: E402


def main() -> int:
    # Forward all args; ensure --rebuild and --clean semantics via build_project
    script_dir = os.path.dirname(os.path.abspath(__file__))
    build_script = os.path.join(script_dir, "build_project.py")

    # Parse lightly for --project presence before forward
    argv = list(sys.argv[1:])
    has_project = any(a == "--project" or a.startswith("--project=") for a in argv)
    if not has_project and not os.environ.get("LEON_PROJECT"):
        try:
            require_project(None)
        except SystemExit as e:
            print(e)
            return 1

    cmd = [sys.executable, build_script, "--rebuild", "--clean"] + argv
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    sys.exit(main())
