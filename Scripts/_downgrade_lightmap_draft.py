#!/usr/bin/env python3
"""Downgrade map WorldSettings to Draft (Medium) lightmap quality."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

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


def downgrade_world_settings(block: str) -> str:
    reps = [
        (r"LightingBuildQuality:\s*\w+", "LightingBuildQuality: Draft"),
        (r"LightmapResolution:\s*\d+", "LightmapResolution: 64"),
        (r"NumIndirectBounces:\s*\d+", "NumIndirectBounces: 2"),
        (r"SamplesPerTexel:\s*\d+", "SamplesPerTexel: 8"),
        (r"AmbientOcclusion:\s*\w+", "AmbientOcclusion: true"),
        (r"AOIntensity:\s*[0-9.]+", "AOIntensity: 1"),
        (r"AORadius:\s*[0-9.]+", "AORadius: 1"),
        (r"TexelPadding:\s*[0-9.]+", "TexelPadding: 2"),
    ]
    out = block
    for pat, repl in reps:
        out = re.sub(pat, repl, out, count=1)
    return out


def clamp_actor_lm(text: str, target: int = 64) -> tuple[str, int]:
    """Clamp large structural meshes that were bumped above medium res."""
    lines = text.splitlines(keepends=True)
    out: list[str] = []
    clamped = 0
    current_name = ""
    for line in lines:
        m = re.match(r'\s+- Name:\s*"([^"]+)"', line)
        if m:
            current_name = m.group(1)
        if "LightmapResolution:" in line and current_name:
            low = current_name.lower()
            if any(t in low for t in LARGE_TOKENS):
                mm = re.search(r"LightmapResolution:\s*(\d+)", line)
                if mm and int(mm.group(1)) > target:
                    line = re.sub(
                        r"LightmapResolution:\s*\d+",
                        f"LightmapResolution: {target}",
                        line,
                    )
                    clamped += 1
        out.append(line)
    return "".join(out), clamped


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
        new_ws = downgrade_world_settings(m.group(2))
        text2 = text[: m.start(2)] + new_ws + text[m.end(2) :]
        text3, clamped = clamp_actor_lm(text2)
        path.write_text(text3, encoding="utf-8", newline="\n")
        print(f"OK {path.name}: WorldSettings=Draft, clamped_meshes={clamped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
