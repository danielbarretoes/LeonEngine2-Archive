# LeonEngine2

Unreal Engine–inspired C++20 game engine for solo / small-team development. Forward PBR (OpenGL 4.5), native asset pipeline, and a thin gameplay framework (`UWorld`, GameMode, HUD/UI).

## Quick start

```bash
# Engine libraries / tools / tests → out/Engine
python Scripts/build_engine.py --config Debug

# Editor (ImGui Docking) → out/Editor
python Scripts/build_editor.py --config Debug
python Scripts/run_editor.py

# One game project → out/Projects/<Name>
python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject --config Debug

# Build and run game project
python Scripts/run_project.py --project Projects/Sandbox/Sandbox.lproject

# Unit / GPU tests (Engine product)
python Scripts/run_tests.py

# Validate project assets
python Scripts/validate_project.py --project Projects/Sandbox/Sandbox.lproject
```

## Layout

| Path | Role |
|------|------|
| `Scripts/` | Engine tooling (`build_engine`, `build_editor`, `build_project`, …) |
| `CMake/` | Shared CMake (stack, ImGui deps, sync) |
| `Engine/` | Product-agnostic runtime (`LeonEngineCore`) |
| `Editor/` | Out-of-process editor (`LeonEditor`) |
| `Plugins/RHI/OpenGL/` | OpenGL 4.5 RHI plugin |
| `Projects/Sandbox/` | Reference game (`.lproject`, Content, Main, GameMode/HUD) |
| `Tools/` | Specialized engine tools (`AssetTool`, `Lightmass`, `ProjectTool`) |
| `Docs/` | Architecture, renderer, assets, decisions |
| `Tests/` | Engine test suite (`RendererTests`) |
| `out/` | Build artifacts (`Engine`, `Editor`, `Projects/<Name>`) |

## Docs

- [Architecture](Docs/ARCHITECTURE.md)
- [Architecture Decisions & Best Practices](Docs/ARCHITECTURE_DECISIONS.md)
- [Product builds](Docs/BUILD.md)
- [Editor architecture](Docs/EDITOR_ARCHITECTURE.md)
- [Editor roadmap](Docs/EDITOR_ROADMAP.md)
- [Engine architecture](Docs/ENGINE_ARCHITECTURE.md)
- [Gameplay framework](Docs/GAMEPLAY_FRAMEWORK.md)
- [Engine / game boundary](Docs/ENGINE_GAME_BOUNDARY.md)
- [Testing architecture](Docs/TESTING_ARCHITECTURE.md)
- [Naming](Docs/NAMING.md)
- [Asset pipeline](Docs/ASSET_PIPELINE.md)
- [Renderer](Docs/RENDERER.md)

## Naming note

UE-style prefixes (`U`/`A`/`F`) are intentional. Runtime objects use `std::shared_ptr` + EnTT — there is **no** Unreal GC, reflection, or `NewObject`.
