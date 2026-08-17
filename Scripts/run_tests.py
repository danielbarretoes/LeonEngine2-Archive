#!/usr/bin/env python3
"""
LeonEngine2 - Test Runner
Builds and executes Engine (RendererTests) and LeonTournament suites.
Reports passed / failed / skipped. Exit code is non-zero if any test failed or skipped.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import time

SUMMARY_RE = re.compile(
    r"test cases:\s+(\d+)\s+\|\s+(\d+) passed\s+\|\s+(\d+) failed(?:\s+\|\s+(\d+) skipped)?"
)


def find_exe(build_dir: str, name: str) -> str:
    candidates = [
        os.path.join(build_dir, "Tests", f"{name}.exe"),
        os.path.join(build_dir, "Tests", name),
    ]
    for path in candidates:
        if os.path.exists(path):
            return path
    return candidates[0]


def parse_summary(output: str) -> tuple[int, int, int, int]:
    total = passed = failed = skipped = 0
    for match in SUMMARY_RE.finditer(output):
        total += int(match.group(1))
        passed += int(match.group(2))
        failed += int(match.group(3))
        skipped += int(match.group(4) or 0)
    return total, passed, failed, skipped


def run_suite(name: str, exe_path: str, extra_args: list[str], cwd: str) -> tuple[int, int, int, int, int]:
    print(f"[RUN] {name}: {exe_path}")
    print("-" * 80)
    cmd = [exe_path] + extra_args
    proc = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    text = (proc.stdout or "") + (proc.stderr or "")
    sys.stdout.write(proc.stdout or "")
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    total, passed, failed, skipped = parse_summary(text)
    if total == 0 and proc.returncode != 0:
        failed = 1
    return proc.returncode, total, passed, failed, skipped


def main() -> int:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, "build")

    print("=" * 80)
    print("   LeonEngine2 - Engine and LeonTournament Test Suites")
    print("=" * 80)
    print("[BUILD] Compiling RendererTests and LeonTournamentTests...")

    t0 = time.perf_counter()
    build_cmd = ["ninja", "-C", build_dir, "RendererTests", "LeonTournamentTests"]
    res = subprocess.run(build_cmd, capture_output=True, text=True)
    t_build = (time.perf_counter() - t0) * 1000.0
    if res.returncode != 0:
        print("[ERROR] Build failed:")
        print(res.stderr or res.stdout)
        return 1
    print(f"[SUCCESS] Built test binaries in {t_build:.1f} ms\n")

    extra_args = sys.argv[1:]
    suites = [
        ("Engine (RendererTests)", find_exe(build_dir, "RendererTests")),
        ("LeonTournamentTests", find_exe(build_dir, "LeonTournamentTests")),
    ]

    grand_passed = grand_failed = grand_skipped = 0
    t_run0 = time.perf_counter()
    any_fail = False
    for name, exe in suites:
        if not os.path.exists(exe):
            print(f"[ERROR] Test executable not found at: {exe}")
            return 1
        code, total, passed, failed, skipped = run_suite(name, exe, extra_args, project_root)
        print(f"[SUMMARY] {name}: {passed} passed, {failed} failed, {skipped} skipped (of {total})")
        print("=" * 80)
        grand_passed += passed
        grand_failed += failed
        grand_skipped += skipped
        if code != 0 or failed > 0:
            any_fail = True

    t_run = (time.perf_counter() - t_run0) * 1000.0
    print(f"Results: {grand_passed} passed, {grand_failed} failed, {grand_skipped} skipped in {t_run:.1f} ms")
    if grand_skipped > 0:
        print("[FAILED] Skipped tests are not allowed (except documented infrastructure skips).")
        return 1
    if any_fail or grand_failed > 0:
        print("[FAILED] One or more test cases failed.")
        return 1
    if grand_passed == 0:
        print("[FAILED] No tests ran.")
        return 1
    print(f"[PASSED] All {grand_passed} test cases passed successfully!")
    return 0


if __name__ == "__main__":
    sys.exit(main())
