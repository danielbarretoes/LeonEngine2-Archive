#!/usr/bin/env python3
"""Upgrade map WorldSettings to Production and bump large-mesh lightmap resolution."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] if Path(__file__).name == "upgrade_lightmap_production.py" else Path.cwd()

MAPS = [
    ROOT / "Projects/LeonTournament/Content/Maps/TournamentArena.lmap",
    ROOT / "Projects/LeonTournament/Content/Maps/TournamentArenaNight.lmap",
    ROOT / "Projects/LeonTournament/Content/Maps/AnimLab.lmap",
    ROOT / "Projects/Sandbox/Content/Maps/ShowcaseLevel.lmap",
    ROOT / "Projects/Sandbox/Content/Maps/NightLevel.lmap",
]

LARGE_TOKENS = (
    "floor",
    "ground",
    "wall",
    "ceiling",
    "maze",
    "street",
    "road",
    "slab",
    "platform",
    "cover",
    "pillar",
    "house",
    "building",
)

WS_BLOCK = re.compile(r"(WorldSettings:\n)(.*?)(\n  Skybox:)", re.S)


def upgrade_world_settings(block: str) -> str:
    reps = [
        (r"LightingBuildQuality:\s*\w+", "LightingBuildQuality: Production"),
        (r"LightmapResolution:\s*\d+", "LightmapResolution: 128"),
        (r"NumIndirectBounces:\s*\d+", "NumIndirectBounces: 3"),
        (r"SamplesPerTexel:\s*\d+", "SamplesPerTexel: 32"),
        (r"AmbientOcclusion:\s*\w+", "AmbientOcclusion: true"),
        (r"AOIntensity:\s*[0-9.]+", "AOIntensity: 1"),
        (r"AORadius:\s*[0-9.]+", "AORadius: 1"),
        (r"TexelPadding:\s*[0-9.]+", "TexelPadding: 2"),
    ]
    out = block
    for pat, repl in reps:
        out = re.sub(pat, repl, out, count=1)
    return out


def bump_actor_lm(text: str) -> tuple[str, int]:
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    bumped = 0
    current_name = ""
    for line in lines:
        m = re.match(r'\s+- Name:\s*"([^"]+)"', line)
        if m:
            current_name = m.group(1)
        if "LightmapResolution:" in line and current_name:
            low = current_name.lower()
            if any(t in low for t in LARGE_TOKENS):
                mm = re.search(r"LightmapResolution:\s*(\d+)", line)
                if mm and int(mm.group(1)) < 128:
                    line = re.sub(r"LightmapResolution:\s*\d+", "LightmapResolution: 128", line)
                    bumped += 1
        out.append(line)
    return "".join(out), bumped


def main() -> int:
    for path in MAPS:
        if not path.exists():
            print(f"MISSING {path}")
            continue
        text = path.read_text(encoding="utf-8")
        m = WS_BLOCK.search(text)
        if not m:
            print(f"SKIP (no WorldSettings): {path}")
            continue
        new_ws = upgrade_world_settings(m.group(2))
        text2 = text[: m.start(2)] + new_ws + text[m.end(2) :]
        text3, bumped = bump_actor_lm(text2)
        path.write_text(text3, encoding="utf-8", newline="\n")
        print(f"OK {path.name}: WorldSettings=Production, bumped_meshes={bumped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
