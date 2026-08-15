#!/usr/bin/env python3
"""
LeonEngine2 - Test Runner
Compiles and executes the test suite with detailed metrics.
"""

import os
import subprocess
import sys
import time

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, "build")
    
    print("=" * 80)
    print("   LeonEngine2 - Comprehensive Regression & Gameplay Test Suite")
    print("=" * 80)
    print("[BUILD] Compiling RendererTests suite...")
    
    t0 = time.perf_counter()
    build_cmd = ["ninja", "-C", build_dir, "RendererTests"]
    res = subprocess.run(build_cmd, capture_output=True, text=True)
    t_build = (time.perf_counter() - t0) * 1000.0
    
    if res.returncode != 0:
        print("[ERROR] Build failed:")
        print(res.stderr or res.stdout)
        sys.exit(1)
        
    print(f"[SUCCESS] Built RendererTests in {t_build:.1f} ms\n")
    
    exe_path = os.path.join(build_dir, "Tests", "RendererTests.exe")
    if not os.path.exists(exe_path):
        exe_path = os.path.join(build_dir, "Tests", "RendererTests")
        
    if not os.path.exists(exe_path):
        print(f"[ERROR] Test executable not found at: {exe_path}")
        sys.exit(1)
        
    print(f"[RUN] Executing: {exe_path}")
    print("-" * 80)
    
    t_run0 = time.perf_counter()
    
    # If specific arguments passed, forward them directly
    if len(sys.argv) > 1:
        run_cmd = [exe_path] + sys.argv[1:]
        test_proc = subprocess.run(run_cmd, cwd=project_root)
        t_run = (time.perf_counter() - t_run0) * 1000.0
        print("-" * 80)
        if test_proc.returncode == 0:
            print(f"[PASSED] Tests passed in {t_run:.1f} ms!")
        else:
            print(f"[FAILED] Test run failed with exit code {test_proc.returncode} in {t_run:.1f} ms.")
        print("=" * 80)
        sys.exit(test_proc.returncode)
    else:
        # Run test cases
        list_proc = subprocess.run([exe_path, "--list-test-cases"], capture_output=True, text=True, cwd=project_root)
        lines = list_proc.stdout.splitlines()
        test_cases = []
        start = False
        for line in lines:
            if "listing all test case names" in line:
                start = True
                continue
            if "========" in line:
                continue
            if "unskipped test cases" in line:
                break
            if start and line.strip():
                test_cases.append(line.strip())
                
        passed = 0
        failed = 0
        for idx, tc in enumerate(test_cases):
            p = subprocess.run([exe_path, f"-tc={tc}"], capture_output=True, text=True, cwd=project_root)
            if p.returncode == 0:
                print(f"  [{idx+1:02d}/{len(test_cases):02d}] PASS: {tc}")
                passed += 1
            else:
                print(f"  [{idx+1:02d}/{len(test_cases):02d}] FAIL: {tc}")
                if p.stdout:
                    print(p.stdout)
                if p.stderr:
                    print(p.stderr)
                failed += 1
                
        t_run = (time.perf_counter() - t_run0) * 1000.0
        print("-" * 80)
        print(f"Results: {passed} passed, {failed} failed out of {len(test_cases)} tests in {t_run:.1f} ms")
        if failed == 0:
            print(f"[PASSED] All {passed} test cases passed successfully!")
            print("=" * 80)
            sys.exit(0)
        else:
            print(f"[FAILED] {failed} test cases failed.")
            print("=" * 80)
            sys.exit(1)

if __name__ == "__main__":
    main()
