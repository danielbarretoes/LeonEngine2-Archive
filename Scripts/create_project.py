#!/usr/bin/env python3
"""
Create a blank LeonEngine2 game project (external or under Projects/).

Usage:
  python Scripts/create_project.py --name MyGame --output D:/Games/MyGame
  python Scripts/create_project.py --name MyGame --output Projects/MyGame
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _leon_paths import engine_root  # noqa: E402


def sanitize_name(name: str) -> str:
    name = name.strip()
    if not name or not re.match(r"^[A-Za-z][A-Za-z0-9_]*$", name):
        raise SystemExit(
            "[ERROR] --name must be a C identifier (letter then letters/digits/_), e.g. MyGame"
        )
    return name


def substitute(text: str, project_name: str) -> str:
    return text.replace("{{PROJECT_NAME}}", project_name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a blank LeonEngine2 project")
    parser.add_argument("--name", required=True, help="Project / CMake target name")
    parser.add_argument(
        "--output",
        required=True,
        help="Output directory (created if missing). Project files go inside this folder.",
    )
    args = parser.parse_args()

    name = sanitize_name(args.name)
    root = engine_root()
    out = args.output
    if not os.path.isabs(out):
        out = os.path.abspath(os.path.join(root, out))
    else:
        out = os.path.abspath(out)

    if os.path.exists(out) and os.listdir(out):
        raise SystemExit(f"[ERROR] Output directory is not empty: {out}")

    template_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Templates", "BlankProject")
    if not os.path.isdir(template_root):
        raise SystemExit(f"[ERROR] Missing templates at {template_root}")

    os.makedirs(out, exist_ok=True)

    # .lproject
    with open(os.path.join(template_root, "Project.lproject.template"), encoding="utf-8") as f:
        lproj = substitute(f.read(), name)
    lproj_path = os.path.join(out, f"{name}.lproject")
    with open(lproj_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(lproj)

    # CMakeLists
    with open(os.path.join(template_root, "CMakeLists.txt.template"), encoding="utf-8") as f:
        cmake = substitute(f.read(), name)
    with open(os.path.join(out, "CMakeLists.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(cmake)

    # Config
    config_dir = os.path.join(out, "Config")
    os.makedirs(config_dir, exist_ok=True)
    for ini in ("DefaultEngine.ini", "DefaultGame.ini", "DefaultInput.ini"):
        src = os.path.join(template_root, "Config", f"{ini}.template")
        with open(src, encoding="utf-8") as f:
            body = substitute(f.read(), name)
        with open(os.path.join(config_dir, ini), "w", encoding="utf-8", newline="\n") as f:
            f.write(body)

    # Map
    maps_dir = os.path.join(out, "Content", "Maps")
    os.makedirs(maps_dir, exist_ok=True)
    with open(
        os.path.join(template_root, "Content", "Maps", "Empty.lmap.template"), encoding="utf-8"
    ) as f:
        body = substitute(f.read(), name)
    with open(os.path.join(maps_dir, "Empty.lmap"), "w", encoding="utf-8", newline="\n") as f:
        f.write(body)

    # Source
    src_root = os.path.join(out, "Source", name)
    os.makedirs(os.path.join(src_root, "Public"), exist_ok=True)
    os.makedirs(os.path.join(src_root, "Private"), exist_ok=True)
    with open(os.path.join(template_root, "Source", "Main.cpp.template"), encoding="utf-8") as f:
        body = substitute(f.read(), name)
    with open(os.path.join(src_root, "Main.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write(body)

    # Empty Content placeholders
    for d in ("Textures", "Materials", "Meshes", "Lightmaps", "HDR"):
        os.makedirs(os.path.join(out, "Content", d), exist_ok=True)

    print("========================================")
    print("   LeonEngine2 - Create Project")
    print("========================================")
    print(f"  Name:     {name}")
    print(f"  Output:   {out}")
    print(f"  Project:  {lproj_path}")
    print("----------------------------------------")
    print("Next steps:")
    print(f'  python Scripts/build_project.py --project "{lproj_path}"')
    print(f'  python Scripts/run_project.py --project "{lproj_path}"')
    print(f'  python Scripts/validate_project.py --project "{lproj_path}"')
    print("========================================")
    return 0


if __name__ == "__main__":
    sys.exit(main())
