#!/usr/bin/env python3
"""
Regenerate TournamentArena / TournamentArenaNight .lmap geometry + Static bake lights.

Matches FLeonTournamentArenaBuilder (Box meshes, MetersPerUv, lab materials).
Preserves PlayerStart / Team*Start / bots / NavBounds from the existing map when present.

Usage:
  python Scripts/rebuild_tournament_arena.py
  python Scripts/rebuild_tournament_arena.py --bake
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DAY_MAP = ROOT / "Projects/LeonTournament/Content/Maps/TournamentArena.lmap"
NIGHT_MAP = ROOT / "Projects/LeonTournament/Content/Maps/TournamentArenaNight.lmap"

H = 32.0
WALL_H = 10.0
CEILING_Y = 10.25
THICK = 0.9

MAT = {
    "Floor": "/Game/Materials/M_LabFloor.lmat",
    "Wall": "/Game/Materials/M_LabWall.lmat",
    "Ceiling": "/Game/Materials/M_LabCeiling.lmat",
    "Prop": "/Game/Materials/M_LabProp.lmat",
    "Metal": "/Game/Materials/M_ArenaMetal.lmat",
    "Accent": "/Game/Materials/M_ArenaAccent.lmat",
}
MPU = {
    "Floor": 2.0,
    "Wall": 1.25,
    "Ceiling": 3.0,
    "Prop": 1.5,
    "Metal": 1.5,
    "Accent": 1.0,
}


def guid(name: str) -> str:
    return hashlib.md5(f"LeonTournamentArena|{name}".encode()).hexdigest()


def fmt_vec(v) -> str:
    def f(x: float) -> str:
        if abs(x - round(x)) < 1e-6:
            return str(int(round(x)))
        s = f"{x:.6g}"
        return s

    return f"[{f(v[0])}, {f(v[1])}, {f(v[2])}]"


def emit_box(
    name: str,
    loc,
    size,
    surface: str,
    *,
    tint=None,
    visible_in_reflection: bool | None = None,
    lightmap_asset: str,
) -> str:
    mat = MAT[surface]
    mpu = MPU[surface]
    if visible_in_reflection is None:
        visible_in_reflection = surface != "Floor"
    lines = [
        f'  - Name: "{name}"',
        f'    Class: "AActor"',
        f'    GUID: "{guid(name)}"',
        f"    Transform:",
        f"      Translation: {fmt_vec(loc)}",
        f"      Rotation: [0, 0, 0]",
        f"      Scale: [1, 1, 1]",
        f"    StaticMesh:",
        f'      Type: "Box"',
        f"      Size: 1",
        f"      Width: {size[0]}",
        f"      Height: {size[1]}",
        f"      Depth: {size[2]}",
        f"      Radius: 0.5",
        f"      MetersPerUv: {mpu}",
        f"      SubdivisionsX: 24",
        f"      SubdivisionsZ: 24",
        f'      Shader: "Engine/Assets/Shaders/PBR_Lit.glsl"',
        f"      CastShadows: true",
        f"      ReceiveShadows: true",
        f"      VisibleInReflection: {'true' if visible_in_reflection else 'false'}",
        f"      Mobility: Static",
        f"      LightmapResolution: 64",
        f'      LightmapAsset: "{lightmap_asset}"',
        f"    Material:",
        f'      Asset: "{mat}"',
    ]
    return "\n".join(lines)


def emit_point(name: str, pos, color, intensity: float, radius: float) -> str:
    return "\n".join(
        [
            f'  - Name: "{name}"',
            f'    Class: "AActor"',
            f'    GUID: "{guid(name)}"',
            f"    Transform:",
            f"      Translation: {fmt_vec(pos)}",
            f"      Rotation: [0, 0, 0]",
            f"      Scale: [1, 1, 1]",
            f"    PointLight:",
            f"      Enabled: true",
            f"      Color: {fmt_vec(color)}",
            f"      Intensity: {intensity}",
            f"      Radius: {radius}",
            f"      Mobility: Static",
        ]
    )


def emit_spot(name: str, pos, direction, color, intensity: float, radius: float, inner: float, outer: float) -> str:
    return "\n".join(
        [
            f'  - Name: "{name}"',
            f'    Class: "AActor"',
            f'    GUID: "{guid(name)}"',
            f"    Transform:",
            f"      Translation: {fmt_vec(pos)}",
            f"      Rotation: [0, 0, 0]",
            f"      Scale: [1, 1, 1]",
            f"    SpotLight:",
            f"      Enabled: true",
            f"      Direction: {fmt_vec(direction)}",
            f"      Color: {fmt_vec(color)}",
            f"      Intensity: {intensity}",
            f"      Radius: {radius}",
            f"      CutOff: {inner}",
            f"      OuterCutOff: {outer}",
            f"      Mobility: Static",
        ]
    )


def geometry(night: bool, lm: str) -> list[str]:
    out: list[str] = []
    box = lambda n, loc, sc, surf, **kw: out.append(emit_box(n, loc, sc, surf, lightmap_asset=lm, **kw))

    box("Floor", (0, -0.25, 0), (H * 2, 0.5, H * 2), "Floor")
    box("Ceiling", (0, CEILING_Y, 0), (H * 2, 0.5, H * 2), "Ceiling")
    box("WallN", (0, WALL_H * 0.5, -H), (H * 2, WALL_H, THICK), "Wall")
    box("WallS", (0, WALL_H * 0.5, H), (H * 2, WALL_H, THICK), "Wall")
    box("WallW", (-H, WALL_H * 0.5, 0), (THICK, WALL_H, H * 2), "Wall")
    box("WallE", (H, WALL_H * 0.5, 0), (THICK, WALL_H, H * 2), "Wall")

    for n, loc, sc in [
        ("LaneW_N", (-14, 2.4, -20), (0.7, 4.8, 14)),
        ("LaneW_S", (-14, 2.4, 20), (0.7, 4.8, 14)),
        ("LaneE_N", (14, 2.4, -20), (0.7, 4.8, 14)),
        ("LaneE_S", (14, 2.4, 20), (0.7, 4.8, 14)),
        ("LaneN_W", (-20, 2.4, -14), (14, 4.8, 0.7)),
        ("LaneN_E", (20, 2.4, -14), (14, 4.8, 0.7)),
        ("LaneS_W", (-20, 2.4, 14), (14, 4.8, 0.7)),
        ("LaneS_E", (20, 2.4, 14), (14, 4.8, 0.7)),
    ]:
        box(n, loc, sc, "Wall")

    box("CoverA", (-22, 1.15, -22), (3.8, 2.3, 1.5), "Prop")
    box("CoverB", (22, 1.15, 22), (3.8, 2.3, 1.5), "Metal")
    box("CoverC", (-22, 1.15, 22), (1.6, 2.3, 3.8), "Accent")
    box("CoverD", (22, 1.15, -22), (1.6, 2.3, 3.8), "Accent")
    box("CoverMidW", (-5, 1.1, 0), (2.6, 2.2, 1.2), "Metal")
    box("CoverMidE", (5, 1.1, 0), (2.6, 2.2, 1.2), "Metal")
    box("CoverN", (0, 1.1, -18), (4.2, 2.2, 1.3), "Prop")
    box("CoverS", (0, 1.1, 18), (4.2, 2.2, 1.3), "Prop")
    for n, loc in [
        ("PillarNW", (-18, 3, -18)),
        ("PillarNE", (18, 3, -18)),
        ("PillarSW", (-18, 3, 18)),
        ("PillarSE", (18, 3, 18)),
    ]:
        box(n, loc, (1.3, 6, 1.3), "Metal")

    if not night:
        box("Floor2_N", (0, 4.6, -22), (H * 2 - 4, 0.4, 16), "Floor")
        box("Floor2_S", (0, 4.6, 22), (H * 2 - 4, 0.4, 16), "Floor")
        box("RailN", (0, 5.4, -14.2), (H * 2 - 8, 1.0, 0.35), "Metal")
        box("RailS", (0, 5.4, 14.2), (H * 2 - 8, 1.0, 0.35), "Metal")
        box("StairsW1", (-26, 1.0, 0), (4, 2, 5), "Prop")
        box("StairsW2", (-26, 2.5, 0), (4, 1.5, 4), "Prop")
        box("StairsW3", (-26, 3.8, 0), (4, 1.2, 3), "Prop")
        box("StairsE1", (26, 1.0, 0), (4, 2, 5), "Prop")
        box("StairsE2", (26, 2.5, 0), (4, 1.5, 4), "Prop")
        box("StairsE3", (26, 3.8, 0), (4, 1.2, 3), "Prop")
        box("UpperCoverA", (-24, 5.7, -24), (3, 2, 1.4), "Accent")
        box("UpperCoverB", (24, 5.7, 24), (3, 2, 1.4), "Accent")
    return out


def lighting(night: bool) -> list[str]:
    out: list[str] = []
    warm = (1.0, 0.82, 0.55)
    cool = (0.55, 0.72, 1.0)
    soft = (1.0, 0.96, 0.90)
    sodium = (1.0, 0.65, 0.28)
    color = cool if night else soft
    intensity = 6.5 if night else 11.0
    y = 8.2 if night else 9.0
    idx = 0
    z = -24.0
    while z <= 24.0 + 0.1:
        x = -24.0
        while x <= 24.0 + 0.1:
            out.append(emit_point(f"PL_Ceil_{idx:02d}", (x, y, z), color, intensity, 16.0))
            idx += 1
            x += 12.0
        z += 12.0

    corner_i = 9.0 if night else 12.0
    corner_c = sodium if night else warm
    for n, p in [
        ("PL_NW", (-22, 5.5, -22)),
        ("PL_NE", (22, 5.5, -22)),
        ("PL_SW", (-22, 5.5, 22)),
        ("PL_SE", (22, 5.5, 22)),
    ]:
        out.append(emit_point(n, p, corner_c, corner_i, 14.0))
    out.append(emit_point("PL_Atrium", (0, 7.5, 0), cool if night else soft, 8.0 if night else 10.0, 18.0))
    out.append(
        emit_spot(
            "Spot_Mid",
            (0, 9.5, 0),
            (0, -1, 0),
            cool if night else soft,
            16.0 if night else 22.0,
            26.0,
            18.0,
            32.0,
        )
    )
    out.append(
        emit_spot(
            "Spot_T1",
            (-20, 9, -20),
            (0.35, -1, 0.35),
            warm,
            14.0 if night else 18.0,
            20.0,
            14.0,
            28.0,
        )
    )
    out.append(
        emit_spot(
            "Spot_T2",
            (20, 9, 20),
            (-0.35, -1, -0.35),
            warm,
            14.0 if night else 18.0,
            20.0,
            14.0,
            28.0,
        )
    )
    if not night:
        out.append(emit_point("PL_UpperN", (0, 7.5, -22), warm, 9.0, 14.0))
        out.append(emit_point("PL_UpperS", (0, 7.5, 22), warm, 9.0, 14.0))
        out.append(emit_spot("Spot_StairsW", (-26, 8.5, 0), (0.2, -1, 0), soft, 16.0, 18.0, 12.0, 24.0))
        out.append(emit_spot("Spot_StairsE", (26, 8.5, 0), (-0.2, -1, 0), soft, 16.0, 18.0, 12.0, 24.0))
    return out


def extract_preserved(text: str) -> list[str]:
    """Keep PlayerStart / Team*Start / bots / NavBounds actor blocks."""
    blocks: list[str] = []
    parts = re.split(r"(?=^  - Name: )", text, flags=re.M)
    for part in parts:
        if not part.startswith("  - Name:"):
            continue
        m = re.match(r'  - Name: "([^"]+)"', part)
        if not m:
            continue
        name = m.group(1)
        keep = (
            name == "PlayerStart"
            or name.startswith("Team1Start_")
            or name.startswith("Team2Start_")
            or name.startswith("Bot_")
            or name in ("NavBounds", "NavMeshBounds")
            or "NavMeshBoundsVolume" in part[:200]
        )
        if keep:
            blocks.append(part.rstrip() + "\n")
    return blocks


def default_spawns() -> list[str]:
    blocks: list[str] = []
    blocks.append(
        "\n".join(
            [
                '  - Name: "PlayerStart"',
                '    Class: "APlayerStart"',
                f'    GUID: "{guid("PlayerStart")}"',
                "    Transform:",
                "      Translation: [0, 3.5, 10.5]",
                "      Rotation: [0, -90, 0]",
                "      Scale: [1, 1, 1]",
            ]
        )
        + "\n"
    )
    t1 = [(-26, 2, -20), (-26, 2, -8), (-26, 2, 8), (-26, 2, 20), (-22, 2, -24), (-22, 2, 24), (-28, 2, 0), (-20, 2, 0)]
    t2 = [(26, 2, -20), (26, 2, -8), (26, 2, 8), (26, 2, 20), (22, 2, -24), (22, 2, 24), (28, 2, 0), (20, 2, 0)]
    for i, p in enumerate(t1):
        n = f"Team1Start_{i}"
        blocks.append(
            "\n".join(
                [
                    f'  - Name: "{n}"',
                    '    Class: "APlayerStart"',
                    f'    GUID: "{guid(n)}"',
                    '    PlayerStartTag: "Team1"',
                    "    TeamIndex: 1",
                    "    Transform:",
                    f"      Translation: {fmt_vec(p)}",
                    "      Rotation: [0, 0, 0]",
                    "      Scale: [1, 1, 1]",
                ]
            )
            + "\n"
        )
    for i, p in enumerate(t2):
        n = f"Team2Start_{i}"
        blocks.append(
            "\n".join(
                [
                    f'  - Name: "{n}"',
                    '    Class: "APlayerStart"',
                    f'    GUID: "{guid(n)}"',
                    '    PlayerStartTag: "Team2"',
                    "    TeamIndex: 2",
                    "    Transform:",
                    f"      Translation: {fmt_vec(p)}",
                    "      Rotation: [0, 180, 0]",
                    "      Scale: [1, 1, 1]",
                ]
            )
            + "\n"
        )
    blocks.append(
        "\n".join(
            [
                '  - Name: "NavBounds"',
                '    Class: "ANavMeshBoundsVolume"',
                f'    GUID: "{guid("NavBounds")}"',
                "    Transform:",
                "      Translation: [0, 5, 0]",
                "      Rotation: [0, 0, 0]",
                f"      Scale: [{H * 2}, 12, {H * 2}]",
            ]
        )
        + "\n"
    )
    return blocks


def emit_sun(night: bool) -> str:
    if night:
        return "\n".join(
            [
                '  - Name: "Moonlight"',
                '    Class: "AActor"',
                f'    GUID: "{guid("Moonlight")}"',
                "    Transform:",
                "      Translation: [0, 14, 0]",
                "      Rotation: [0, 0, 0]",
                "      Scale: [1, 1, 1]",
                "    DirectionalLight:",
                "      Enabled: true",
                "      Direction: [0.28, -1, -0.32]",
                "      Color: [0.45, 0.58, 0.95]",
                "      Intensity: 1.6",
                "      Mobility: Stationary",
            ]
        )
    return "\n".join(
        [
            '  - Name: "Directional Sunlight"',
            '    Class: "AActor"',
            f'    GUID: "{guid("Directional Sunlight")}"',
            "    Transform:",
            "      Translation: [0, 14, 0]",
            "      Rotation: [0, 0, 0]",
            "      Scale: [1, 1, 1]",
            "    DirectionalLight:",
            "      Enabled: true",
            "      Direction: [-0.35, -1, -0.45]",
            "      Color: [1, 0.97, 0.9]",
            "      Intensity: 3",
            "      Mobility: Stationary",
        ]
    )


def header(night: bool) -> str:
    if night:
        return """# LeonEngine2 Map Asset File (.lmap)
# Human-Readable & Deterministic Serialization
Map:
  Name: "TournamentArenaNight"
  Version: "2.1"

Environment:
  WorldSettings:
    StaticLighting: true
    LightingBuildQuality: Draft
    LightmapResolution: 64
    NumIndirectBounces: 2
    SamplesPerTexel: 8
    IndirectIntensity: 1.15
    AmbientOcclusion: true
    AOIntensity: 1
    AORadius: 1
    TexelPadding: 2
    WorldScale: 1
    LightmapAsset: "/Game/Lightmaps/TournamentArenaNight.llightmap"
  Skybox:
    Enabled: true
    Exposure: 1.05
    SunIntensity: 1.4
    EnvironmentIntensity: 0.85
    UseHDREnvironmentMap: true
    HDREnvironmentMap: "/Game/HDR/NightSky1k.lhdr"
    SkyZenithColor: [0.02, 0.04, 0.1]
    HorizonColor: [0.08, 0.1, 0.18]
    GroundColor: [0.04, 0.04, 0.05]
    SunColor: [0.45, 0.58, 0.95]

Actors:
"""
    return """# LeonEngine2 Map Asset File (.lmap)
# Human-Readable & Deterministic Serialization
Map:
  Name: "TournamentArena"
  Version: "2.1"

Environment:
  WorldSettings:
    StaticLighting: true
    LightingBuildQuality: Draft
    LightmapResolution: 64
    NumIndirectBounces: 2
    SamplesPerTexel: 8
    IndirectIntensity: 1.1
    AmbientOcclusion: true
    AOIntensity: 1
    AORadius: 1
    TexelPadding: 2
    WorldScale: 1
    LightmapAsset: "/Game/Lightmaps/TournamentArena.llightmap"
  Skybox:
    Enabled: true
    Exposure: 0.95
    SunIntensity: 2.4
    EnvironmentIntensity: 1.35
    UseHDREnvironmentMap: true
    HDREnvironmentMap: "/Game/HDR/DaySky1k.lhdr"
    SkyZenithColor: [0.12, 0.28, 0.55]
    HorizonColor: [0.55, 0.62, 0.75]
    GroundColor: [0.18, 0.19, 0.22]
    SunColor: [1, 0.96, 0.88]

Actors:
"""


def rebuild(path: Path, night: bool) -> None:
    lm = (
        "/Game/Lightmaps/TournamentArenaNight.llightmap"
        if night
        else "/Game/Lightmaps/TournamentArena.llightmap"
    )
    preserved: list[str] = []
    if path.exists():
        preserved = extract_preserved(path.read_text(encoding="utf-8"))
    if not preserved:
        preserved = default_spawns()

    chunks = [header(night), emit_sun(night) + "\n"]
    chunks.extend(x if x.endswith("\n") else x + "\n" for x in preserved)
    for block in geometry(night, lm):
        chunks.append(block + "\n")
    for block in lighting(night):
        chunks.append(block + "\n")
    path.write_text("".join(chunks), encoding="utf-8", newline="\n")
    print(f"[OK] wrote {path} ({'night' if night else 'day'})")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bake", action="store_true", help="Draft bake both maps after rewrite")
    args = parser.parse_args()

    rebuild(DAY_MAP, night=False)
    rebuild(NIGHT_MAP, night=True)

    if args.bake:
        bake = ROOT / "Scripts/bake_lightmaps.py"
        project = "Projects/LeonTournament/LeonTournament.lproject"
        for m in ("/Game/Maps/TournamentArena", "/Game/Maps/TournamentArenaNight"):
            cmd = [sys.executable, str(bake), "--project", project, "--map", m, "--force", "--quality", "Draft"]
            print(f"[RUN] {' '.join(cmd)}")
            rc = subprocess.run(cmd, cwd=ROOT).returncode
            if rc != 0:
                return rc
    return 0


if __name__ == "__main__":
    sys.exit(main())
