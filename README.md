# LeonEngine2

Unreal Engine–inspired C++20 game engine for solo / small-team development. Forward PBR (OpenGL 4.5), native asset pipeline, and a thin gameplay framework (`UWorld`, GameMode, HUD/UI).

## Quick start

```bash
# Configure + build a project (Engine script + --project)
python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject --config Debug

# Build and run
python Scripts/run_project.py --project Projects/Sandbox/Sandbox.lproject

# Unit / GPU tests (Engine)
python Scripts/run_tests.py

# Validate project assets
python Scripts/validate_project.py --project Projects/Sandbox/Sandbox.lproject
```

Compatibility wrappers `run_sandbox.py` / `validate_sandbox.py` forward to the `--project` scripts.

## Layout

| Path | Role |
|------|------|
| `Scripts/` | Engine tooling (`build_project`, `run_project`, `verify_ue_naming`, …) |
| `Engine/` | Product-agnostic runtime (`LeonEngineCore`) |
| `Plugins/RHI/OpenGL/` | OpenGL 4.5 RHI plugin |
| `Projects/Sandbox/` | Reference game (`.lproject`, Content, Main, GameMode/HUD) |
| `Tools/LeonAssetTool/` | Import / validate CLI |
| `Docs/` | Architecture, renderer, assets |
| `Tests/` | Engine suite (`RendererTests`) and `LeonTournamentTests` |

## Sandbox

Reference game with two maps. Default boot is `/Game/Maps/ShowcaseLevel` (procedural PBR primitives, `DaySky1k`). The HUD chip in the top-right travels to `/Game/Maps/NightLevel` (imported static meshes, `NightSky1k`) and back. IBL/HDR engine tests use `DaySky1k.lhdr`.

## Docs

- [Architecture](Docs/ARCHITECTURE.md)
- [Engine architecture](Docs/ENGINE_ARCHITECTURE.md)
- [Gameplay framework](Docs/GAMEPLAY_FRAMEWORK.md)
- [Engine / game boundary](Docs/ENGINE_GAME_BOUNDARY.md)
- [Testing architecture](Docs/TESTING_ARCHITECTURE.md)
- [Naming](Docs/NAMING.md)
- [Asset pipeline](Docs/ASSET_PIPELINE.md)
- [Renderer](Docs/RENDERER.md)

## Naming note

UE-style prefixes (`U`/`A`/`F`) are intentional. Runtime objects use `std::shared_ptr` + EnTT — there is **no** Unreal GC, reflection, or `NewObject`.
