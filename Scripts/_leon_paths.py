#!/usr/bin/env python3
"""
Shared path helpers for LeonEngine2 Engine Scripts.

Contract (Unreal-like):
  LEON_ENGINE_ROOT  — Engine repo root (Scripts/, Engine/, CMakeLists.txt)
  LEON_PROJECT      — Path to the game .lproject (may live outside the Engine repo)

Scripts never default to a product name (e.g. Sandbox).
"""

from __future__ import annotations

import json
import os
import shutil
import stat
import subprocess
import sys
def ensure_build_tools_in_path() -> None:
    """Ensure cmake and ninja are present in PATH on Windows environments."""
    if sys.platform != "win32":
        return

    path_dirs = os.environ.get("PATH", "").split(os.pathsep)
    has_cmake = shutil.which("cmake") is not None
    has_ninja = shutil.which("ninja") is not None
    if has_cmake and has_ninja:
        return

    candidates = [
        r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja",
        r"C:\Program Files\CMake\bin",
        r"C:\ProgramData\chocolatey\bin",
    ]

    new_entries: list[str] = []
    for c in candidates:
        if os.path.isdir(c) and c not in path_dirs and c not in new_entries:
            new_entries.append(c)

    if new_entries:
        os.environ["PATH"] = os.pathsep.join(new_entries) + os.pathsep + os.environ.get("PATH", "")


def ensure_msvc_environment() -> None:
    """Ensure MSVC compiler environment (INCLUDE, LIB, PATH) is initialized on Windows."""
    if sys.platform != "win32":
        return

    if "INCLUDE" in os.environ and "LIB" in os.environ:
        return

    vcvars_candidates = [
        r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    ]

    for vcvars in vcvars_candidates:
        if os.path.isfile(vcvars):
            try:
                cmd = f'call "{vcvars}" && set'
                output = subprocess.check_output(cmd, shell=True, text=True, stderr=subprocess.DEVNULL)
                for line in output.splitlines():
                    if "=" in line:
                        k, _, v = line.partition("=")
                        os.environ[k.strip()] = v.strip()
                break
            except Exception:
                pass


# Run automatically upon importing _leon_paths
ensure_build_tools_in_path()
ensure_msvc_environment()



def safe_rmtree(path: str) -> None:
    """Remove a directory tree safely handling read-only files on Windows."""
    def _onerror(func, p, _exc_info):
        try:
            os.chmod(p, stat.S_IWRITE)
            func(p)
        except OSError:
            pass

    if os.path.exists(path):
        shutil.rmtree(path, onerror=_onerror)


def engine_root() -> str:
    env = os.environ.get("LEON_ENGINE_ROOT", "").strip()
    if env:
        return os.path.abspath(env)
    # Scripts/_leon_paths.py → parent of Scripts/
    return os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))


def require_project(arg: Optional[str] = None) -> str:
    """
    Resolve absolute path to a .lproject file.
    Order: explicit arg → LEON_PROJECT env. No product defaults.
    """
    candidate = (arg or "").strip() or os.environ.get("LEON_PROJECT", "").strip()
    if not candidate:
        raise SystemExit(
            "[ERROR] --project <path.lproject> is required (or set LEON_PROJECT).\n"
            "  Example: python Scripts/build_project.py --project D:/Games/MyGame/MyGame.lproject"
        )

    root = engine_root()
    path = candidate
    if not os.path.isabs(path):
        # Prefer cwd-relative, then engine-root-relative
        cwd_path = os.path.abspath(path)
        eng_path = os.path.abspath(os.path.join(root, path))
        if os.path.isfile(cwd_path):
            path = cwd_path
        elif os.path.isfile(eng_path):
            path = eng_path
        else:
            path = cwd_path
    else:
        path = os.path.abspath(path)

    if not os.path.isfile(path):
        raise SystemExit(f"[ERROR] Project file not found: {path}")
    if not path.lower().endswith(".lproject"):
        raise SystemExit(f"[ERROR] Expected a .lproject file, got: {path}")
    return path


def project_dir(lproject: str) -> str:
    return os.path.dirname(os.path.abspath(lproject))


def project_content_dir(lproject: str) -> str:
    return os.path.join(project_dir(lproject), "Content")


def project_raw_dir(lproject: str) -> str:
    """Prefer Content/Raw, then Raw/ beside the project."""
    base = project_dir(lproject)
    content_raw = os.path.join(base, "Content", "Raw")
    if os.path.isdir(content_raw):
        return content_raw
    raw = os.path.join(base, "Raw")
    return raw


def load_project_json(lproject: str) -> dict:
    with open(lproject, encoding="utf-8") as f:
        return json.load(f)


def project_name(lproject: str) -> str:
    try:
        data = load_project_json(lproject)
        name = data.get("ProjectName", "")
        if name:
            return name
    except (OSError, json.JSONDecodeError, TypeError):
        pass
    return os.path.splitext(os.path.basename(lproject))[0]


def resolve_default_map_path(lproject: str, map_arg: Optional[str] = None) -> str:
    """
    Resolve a .lmap absolute path from --map or DefaultMap in .lproject.
    Virtual forms: /Game/Maps/Foo → <Project>/Content/Maps/Foo.lmap
    """
    content = project_content_dir(lproject)
    if map_arg:
        m = map_arg.strip()
        if os.path.isfile(m):
            return os.path.abspath(m)
        if m.startswith("/Game/"):
            rel = m[len("/Game/") :]
            if not rel.lower().endswith(".lmap"):
                rel = rel + ".lmap"
            candidate = os.path.join(content, rel.replace("/", os.sep))
            if os.path.isfile(candidate):
                return os.path.abspath(candidate)
        # Relative to project or content
        for base in (project_dir(lproject), content, engine_root()):
            cand = os.path.join(base, m)
            if os.path.isfile(cand):
                return os.path.abspath(cand)
        raise SystemExit(f"[ERROR] Map not found: {map_arg}")

    data = load_project_json(lproject)
    default_map = data.get("DefaultMap", "")
    if not default_map:
        raise SystemExit("[ERROR] No --map and .lproject has no DefaultMap")
    return resolve_default_map_path(lproject, default_map)


def find_tool(tool_name: str = "AssetTool") -> str:
    """Locate a built tool under out/Engine or out/Projects."""
    root = engine_root()
    exe = f"{tool_name}.exe" if sys.platform == "win32" else tool_name
    search_roots = [
        os.path.join(root, "out", "Engine"),
        os.path.join(root, "out", "Projects"),
        os.path.join(root, "build"),
    ]
    for sroot in search_roots:
        if not os.path.exists(sroot):
            continue
        for dp, _, files in os.walk(sroot):
            if exe in files:
                return os.path.join(dp, exe)
    return os.path.join(search_roots[0], exe)


def ensure_tool_built(tool_name: str = "AssetTool", config: Optional[str] = None) -> str:
    path = find_tool(tool_name)
    if os.path.isfile(path):
        return path

    root = engine_root()
    build_dir = os.path.join(root, "out", "Engine")
    cfg = config or os.environ.get("LEON_BUILD_CONFIG", "Debug")
    print(f"[INFO] Building {tool_name}...")
    cache = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache):
        r = subprocess.run(
            [
                "cmake",
                "-S",
                ".",
                "-B",
                build_dir,
                "-G",
                "Ninja",
                f"-DCMAKE_BUILD_TYPE={cfg}",
                "-DLEON_PRODUCT=Engine",
            ],
            cwd=root,
        )
        if r.returncode != 0:
            raise SystemExit("[ERROR] CMake configure failed")
    r = subprocess.run(["cmake", "--build", build_dir, "--target", tool_name], cwd=root)
    if r.returncode != 0:
        raise SystemExit(f"[ERROR] Failed to build {tool_name}")
    path = find_tool(tool_name)
    if not os.path.isfile(path):
        raise SystemExit(f"[ERROR] {tool_name} not found after build")
    return path


def find_project_executable(build_dir: str, target: str, project_folder_name: str = "") -> str:
    exe_name = f"{target}.exe" if sys.platform == "win32" else target
    candidates = [
        os.path.join(build_dir, "_project", exe_name),
        os.path.join(build_dir, exe_name),
        os.path.join(build_dir, "Projects", target, exe_name),
        os.path.join(build_dir, "Projects", project_folder_name, exe_name) if project_folder_name else "",
    ]
    candidates = [c for c in candidates if c]
    projects_root = os.path.join(build_dir, "Projects")
    if os.path.isdir(projects_root):
        for name in os.listdir(projects_root):
            cand = os.path.join(projects_root, name, exe_name)
            if cand not in candidates:
                candidates.append(cand)
    for c in candidates:
        if os.path.isfile(c):
            return c
    return candidates[0]
