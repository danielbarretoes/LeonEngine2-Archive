#!/usr/bin/env python3
"""
LeonEngine2 — stage a Shipping (distributable) build of a game project.

Industry layout (double-clickable folder / zip):
  <Out>/<ProjectName>/
    <ProjectName>.exe
    <ProjectName>.lproject
    Content/
    Config/
    Engine/Resources/          # shaders, fonts, engine textures
    <runtime DLLs>          # MinGW/MSVC redistributables when dynamically linked

Usage:
  python Scripts/package_project.py --project Projects/LeonTournament/LeonTournament.lproject
  python Scripts/package_project.py --project ... --out D:/Releases --no-zip
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from typing import Iterable, Set

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import (  # noqa: E402
    engine_root,
    find_project_executable,
    project_dir,
    project_name,
    require_project,
)

# Windows system / API-set DLLs that must not be redistributed from the toolchain.
_SYSTEM_DLL_PREFIXES = (
    "api-ms-win-",
    "ext-ms-",
)
_SYSTEM_DLLS = {
    "advapi32.dll",
    "bcrypt.dll",
    "combase.dll",
    "comdlg32.dll",
    "crypt32.dll",
    "d3d11.dll",
    "d3d12.dll",
    "dbghelp.dll",
    "dwmapi.dll",
    "dxgi.dll",
    "gdi32.dll",
    "imm32.dll",
    "iphlpapi.dll",
    "kernel32.dll",
    "msvcrt.dll",
    "ntdll.dll",
    "ole32.dll",
    "oleaut32.dll",
    "opengl32.dll",
    "powrprof.dll",
    "rpcrt4.dll",
    "sechost.dll",
    "setupapi.dll",
    "shell32.dll",
    "shlwapi.dll",
    "ucrtbase.dll",
    "user32.dll",
    "userenv.dll",
    "version.dll",
    "winhttp.dll",
    "winmm.dll",
    "ws2_32.dll",
}


def print_header(text: str) -> None:
    print("========================================")
    print(f"   LeonEngine2 - {text}")
    print("========================================")


def _cmake_cache_value(cache_path: str, key: str) -> str:
    if not os.path.isfile(cache_path):
        return ""
    prefix = f"{key}:"
    with open(cache_path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            if line.startswith(prefix):
                return line.split("=", 1)[-1].strip()
    return ""


def _compiler_bin_dir(build_dir: str) -> str:
    compiler = _cmake_cache_value(os.path.join(build_dir, "CMakeCache.txt"), "CMAKE_CXX_COMPILER")
    if compiler and os.path.isfile(compiler):
        return os.path.dirname(os.path.abspath(compiler))
    for name in ("c++", "g++", "clang++", "cl"):
        found = shutil.which(name)
        if found:
            return os.path.dirname(os.path.abspath(found))
    return ""


def _find_objdump(compiler_bin: str) -> str:
    for candidate in (
        os.path.join(compiler_bin, "objdump.exe"),
        os.path.join(compiler_bin, "objdump"),
        shutil.which("objdump") or "",
        shutil.which("llvm-objdump") or "",
    ):
        if candidate and os.path.isfile(candidate):
            return candidate
    return ""


def _find_strip(compiler_bin: str) -> str:
    for candidate in (
        os.path.join(compiler_bin, "strip.exe"),
        os.path.join(compiler_bin, "strip"),
        shutil.which("strip") or "",
        shutil.which("llvm-strip") or "",
    ):
        if candidate and os.path.isfile(candidate):
            return candidate
    return ""


def _is_system_dll(name: str) -> bool:
    lower = name.lower()
    if lower in _SYSTEM_DLLS:
        return True
    return any(lower.startswith(p) for p in _SYSTEM_DLL_PREFIXES)


def _pe_imported_dlls(exe_path: str, objdump: str) -> Set[str]:
    if not objdump:
        return set()
    result = subprocess.run(
        [objdump, "-p", exe_path],
        capture_output=True,
        text=True,
        errors="ignore",
    )
    if result.returncode != 0:
        return set()
    dlls: Set[str] = set()
    for match in re.finditer(r"DLL Name:\s*(\S+)", result.stdout, flags=re.IGNORECASE):
        dlls.add(match.group(1))
    return dlls


def _copy_runtime_dlls(exe_path: str, stage_dir: str, compiler_bin: str) -> list[str]:
    """Copy non-system DLLs required by the exe (typical MinGW libstdc++/libgcc/winpthread)."""
    objdump = _find_objdump(compiler_bin)
    imported = _pe_imported_dlls(exe_path, objdump)
    search_dirs = []
    if compiler_bin:
        search_dirs.append(compiler_bin)
    search_dirs.append(os.path.dirname(exe_path))

    copied: list[str] = []
    for dll_name in sorted(imported, key=str.lower):
        if _is_system_dll(dll_name):
            continue
        src = ""
        for directory in search_dirs:
            candidate = os.path.join(directory, dll_name)
            if os.path.isfile(candidate):
                src = candidate
                break
        if not src:
            print(f"[WARN] Runtime DLL not found beside toolchain: {dll_name}")
            continue
        dst = os.path.join(stage_dir, os.path.basename(src))
        shutil.copy2(src, dst)
        copied.append(os.path.basename(dst))
        print(f"[INFO] Bundled runtime: {os.path.basename(dst)}")
    return copied


def _ignore_content(_dir: str, names: Iterable[str]) -> set[str]:
    skip = {".git", ".DS_Store", "Thumbs.db"}
    return {n for n in names if n in skip or n.endswith(".tmp")}


def _copytree(src: str, dst: str) -> None:
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst, ignore=_ignore_content)


def _write_readme(stage_dir: str, name: str) -> None:
    path = os.path.join(stage_dir, "README.txt")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            f"{name} — Shipping package\n"
            f"========================\n\n"
            f"Run {name}.exe from this folder (double-click is fine).\n"
            f"Do not move the exe without Content/, Config/, and Engine/.\n"
            f"Requires a GPU with OpenGL drivers.\n"
        )


def main() -> int:
    parser = argparse.ArgumentParser(description="Package a LeonEngine2 Shipping distributable")
    parser.add_argument(
        "--project",
        default=os.environ.get("LEON_PROJECT", ""),
        help="Path to .lproject (or set LEON_PROJECT)",
    )
    parser.add_argument("--target", default="", help="CMake executable target (default: ProjectName)")
    parser.add_argument(
        "--config",
        default="Release",
        choices=["Release", "RelWithDebInfo"],
        help="Build configuration (Shipping uses Release)",
    )
    parser.add_argument(
        "--out",
        default="",
        help="Output root (default: <Project>/Dist/Shipping)",
    )
    parser.add_argument("--no-build", action="store_true", help="Skip build; stage existing exe")
    parser.add_argument("--no-strip", action="store_true", help="Keep symbols on the staged exe")
    parser.add_argument("--no-zip", action="store_true", help="Do not create a .zip archive")
    parser.add_argument("--clean", action="store_true", help="Delete previous stage folder first")
    args = parser.parse_args()

    root = engine_root()
    os.chdir(root)

    try:
        project = require_project(args.project)
    except SystemExit as e:
        print(e)
        return 1

    name = args.target or project_name(project)
    abs_project_dir = project_dir(project)
    out_root = args.out.strip() or os.path.join(abs_project_dir, "Dist", "Shipping")
    out_root = os.path.abspath(out_root)
    stage_dir = os.path.join(out_root, name)

    print_header("Package Shipping")
    print(f"  Configuration: {args.config}")
    print(f"  Engine Root:    {root}")
    print(f"  Project:        {project}")
    print(f"  Stage:          {stage_dir}")
    print("----------------------------------------")

    if not args.no_build:
        build_script = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build_project.py")
        build_cmd = [
            sys.executable,
            build_script,
            "--project",
            project,
            "--target",
            name,
            "--config",
            args.config,
        ]
        print(f"[INFO] Building {name} ({args.config})...")
        build_result = subprocess.run(build_cmd)
        if build_result.returncode != 0:
            print("[ERROR] Shipping build failed")
            return build_result.returncode

    build_dir = os.path.join(root, "out", "Projects", os.path.basename(abs_project_dir))
    exe_src = find_project_executable(build_dir, name, os.path.basename(abs_project_dir))
    if not os.path.isfile(exe_src):
        print(f"[ERROR] Executable not found: {exe_src}")
        return 1

    content_src = os.path.join(abs_project_dir, "Content")
    config_src = os.path.join(abs_project_dir, "Config")
    engine_assets_src = os.path.join(root, "Engine", "Assets")
    if not os.path.isdir(content_src):
        print(f"[ERROR] Missing Content/: {content_src}")
        return 1
    if not os.path.isdir(config_src):
        print(f"[ERROR] Missing Config/: {config_src}")
        return 1
    if not os.path.isdir(engine_assets_src):
        print(f"[ERROR] Missing Engine/Resources: {engine_assets_src}")
        return 1

    if args.clean and os.path.isdir(stage_dir):
        print(f"[INFO] Cleaning stage: {stage_dir}")
        shutil.rmtree(stage_dir)

    start = time.time()
    os.makedirs(out_root, exist_ok=True)
    if os.path.isdir(stage_dir):
        shutil.rmtree(stage_dir)
    os.makedirs(stage_dir, exist_ok=True)

    exe_name = os.path.basename(exe_src)
    exe_dst = os.path.join(stage_dir, exe_name)
    print(f"[INFO] Staging executable -> {exe_name}")
    shutil.copy2(exe_src, exe_dst)

    print("[INFO] Staging Content/")
    _copytree(content_src, os.path.join(stage_dir, "Content"))
    print("[INFO] Staging Config/")
    _copytree(config_src, os.path.join(stage_dir, "Config"))
    print("[INFO] Staging Engine/Resources/")
    _copytree(engine_assets_src, os.path.join(stage_dir, "Engine", "Assets"))

    lproject_dst = os.path.join(stage_dir, os.path.basename(project))
    shutil.copy2(project, lproject_dst)
    print(f"[INFO] Staging {os.path.basename(project)}")

    compiler_bin = _compiler_bin_dir(build_dir)
    runtime_dlls = _copy_runtime_dlls(exe_dst, stage_dir, compiler_bin)

    if not args.no_strip and sys.platform == "win32":
        strip = _find_strip(compiler_bin)
        if strip:
            print("[INFO] Stripping symbols (Shipping)")
            strip_result = subprocess.run([strip, "--strip-unneeded", exe_dst])
            if strip_result.returncode != 0:
                print("[WARN] strip failed; shipping unstripped exe")
        else:
            print("[WARN] strip not found; shipping unstripped exe")

    _write_readme(stage_dir, name)

    zip_path = ""
    if not args.no_zip:
        zip_base = os.path.join(out_root, f"{name}-Win64-Shipping")
        print(f"[INFO] Creating archive: {zip_base}.zip")
        zip_path = shutil.make_archive(zip_base, "zip", root_dir=out_root, base_dir=name)

    elapsed = time.time() - start
    print("----------------------------------------")
    print(f"[SUCCESS] Shipping package ready in {elapsed:.1f}s")
    print(f"  Folder: {stage_dir}")
    if zip_path:
        print(f"  Zip:    {zip_path}")
    if runtime_dlls:
        print(f"  Runtime DLLs: {', '.join(runtime_dlls)}")
    print("========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
