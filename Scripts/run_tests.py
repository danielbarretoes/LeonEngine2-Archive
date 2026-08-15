#!/usr/bin/env python3
"""
LeonEngine2 - Renderer Mathematical Regression Test Runner
Compiles and executes the RendererTests suite with detailed metrics.
"""

import os
import subprocess
import sys
import time

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    build_dir = os.path.join(project_root, "build")
    
    # 1. Compile RendererTests target
    print("=" * 80)
    print("   LeonEngine2 - Renderer Mathematical Regression Tests")
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
    
    # 2. Locate and execute test binary
    exe_path = os.path.join(build_dir, "Tests", "RendererTests.exe")
    if not os.path.exists(exe_path):
        exe_path = os.path.join(build_dir, "Tests", "RendererTests")
        
    if not os.path.exists(exe_path):
        print(f"[ERROR] Test executable not found at: {exe_path}")
        sys.exit(1)
        
    print(f"[RUN] Executing: {exe_path}")
    print("-" * 80)
    
    t_run0 = time.perf_counter()
    # Pass any forwarded command line arguments (e.g. -tc="*IBL*")
    run_cmd = [exe_path] + sys.argv[1:]
    test_proc = subprocess.run(run_cmd, cwd=project_root)
    t_run = (time.perf_counter() - t_run0) * 1000.0
    
    print("-" * 80)
    if test_proc.returncode == 0:
        print(f"[PASSED] All mathematical regression tests passed in {t_run:.1f} ms!")
    else:
        print(f"[FAILED] Test run failed with exit code {test_proc.returncode} in {t_run:.1f} ms.")
    print("=" * 80)
    
    sys.exit(test_proc.returncode)

if __name__ == "__main__":
    main()
