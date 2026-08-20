# LeonEngine2 — Editor Roadmap

## Phase 0 (current) — Build split + ImGui skeleton

- Product lives under [`Editor/`](../Editor/) with its own CMake entry: `cmake -S Editor -B out/Editor`.
- NAMING-compliant host: `Leon::Editor::FEditorApp`, `FViewportPanel` under `Editor/Source/Public/Editor/`.
- Reuses `FApplication` / `FWindow`, `UWorld`, and `FWorldRenderer` (empty world viewport).

Build:

```bat
python Scripts/build_editor.py --config Debug
python Scripts/build_editor.py --run
```

## Goals

Solo-dev editor that reuses Runtime systems (Unreal-like naming and flows) without inventing a parallel engine.

## Constraints

| Do | Do not |
| :--- | :--- |
| Out-of-process `Editor/` exe (`LeonEditor`) | Embed editor UI inside `LeonEngineCore` game loop by default |
| Reuse `FWorldRenderer`, `UWorld`, `.lmap` serializers | Fork a second renderer |
| Edit data that already has disk formats (`.lmap`, `.lmat`, `.lmi`) | Require Blueprints / UHT / UBT |
| Follow [NAMING.md](NAMING.md) for all editor code | Reintroduce pre-NAMING patterns |

## Suggested phases (after skeleton)

1. **Map viewport** — load/save `.lmap`, select actors, move transforms, place `APlayerStart` / lights / static meshes.
2. **Material instance tweak** — edit scalars/textures on `.lmi` / `.lmat` with live preview.
3. **Content browser / details / outliner** — active native panels under `Editor/Source/`.
4. **PIE** — play-in-editor against project `.lproject` (out-of-process first if needed).
5. **BT / Blackboard viewer** — read-only first; write `.lbt` after executor + asset format exist.
6. **UMG layout** — Canvas/Button/Text placement for menus; optional.

## Non-goals (v1 editor)

- Full Slate, Cascade, Control Rig, Recast bake UI, shader graph, marketplace plugins.
