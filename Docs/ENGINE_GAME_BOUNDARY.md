# LeonEngine2 — Engine / Game Boundary

Dependency direction is **Game → Engine** only.

```text
Projects/LeonTournament  →  Engine
Projects/Sandbox         →  Engine
Engine                   ↛  any project
```

## What the Engine must not know

LeonTournament, Shooter, Rifle, TDM, teams, kills, deathmatch, game HUD, game weapons, game characters, game AI behavior, game maps, Mixamo Y Bot paths, `/Game/Animations/` product clips.

## Configuration, not compilation

Product class names appear in **project** `Config/*.ini` and `.lproject`, never as `#include` of `Projects/...` from `Engine/Source`.

Root CMake `LEON_PROJECT_DIR` selects the game to run. Extra monorepo games are discovered via `Projects/*/CMakeLists.txt` without naming a product in Engine sources.

## Enforcement

| Check | Location |
| :--- | :--- |
| No `Sandbox` / `LeonTournament` / `Projects/<Product>` in Engine sources | `Scripts/verify_ue_naming.py` |
| No game tokens in `Engine/Source` | `Tests/Gameplay/EngineGameSeparationTests.cpp` |
| Engine test binary does not link game `.cpp` | `Tests/CMakeLists.txt` (`RendererTests`) |

## Asset ownership

| Owner | Examples |
| :--- | :--- |
| Engine | Shaders, fonts, BRDF LUT, generic importers, `/Engine/...` |
| Project | Maps, materials `M_*`, blend spaces `BS_*`, skeletons, BT/Blackboard assets, HDR used by that game |

Sample-project HDR (`Projects/Sandbox/Content/HDR/DaySky1k.lhdr`) may be used as an **engine test fixture** because Sandbox is the blank sample game, analogous to a UE template — not because the engine ships that map. Engine **source** still must not embed that path; tests may.

## Plugin boundary

Plugins implement RHI / physics / net. They depend on Engine public interfaces. They must not include project headers.
