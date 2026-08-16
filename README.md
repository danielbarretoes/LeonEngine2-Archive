# LeonEngine2

Unreal Engine–inspired C++20 game engine for solo / small-team development. Forward PBR (OpenGL 4.5), native asset pipeline, and a thin gameplay framework (`UWorld`, GameMode, HUD/UI).

## Quick start

```bash
# Configure + build (Ninja)
python Scripts/build_incremental.py --config Debug

# Run Sandbox
python Scripts/run_sandbox.py

# Unit / GPU tests
python Scripts/run_tests.py

# Validate Sandbox project assets
python Scripts/validate_sandbox.py
```

## Layout

| Path | Role |
|------|------|
| `Engine/` | Core library (`Leon::Core`) — core, renderer, world, gameplay, ui, asset |
| `Plugins/RHI/OpenGL/` | OpenGL 4.5 RHI plugin |
| `Projects/Sandbox/` | Reference project (`.lproject`, maps, materials, GameMode/HUD) |
| `Tools/LeonAssetTool/` | Import / validate CLI |
| `Docs/` | Architecture, renderer, assets |
| `Tests/` | doctest suites (math, IBL, PBR, GPU, gameplay, UI) |

## Docs

- [Architecture](Docs/ARCHITECTURE.md)
- [Naming](Docs/NAMING.md)
- [Asset pipeline](Docs/ASSET_PIPELINE.md)
- [Renderer feature audit](Docs/RENDERER_FEATURE_AUDIT.md)

## Naming note

UE-style prefixes (`U`/`A`/`F`) are intentional. Runtime objects use `std::shared_ptr` + EnTT — there is **no** Unreal GC, reflection, or `NewObject`.
