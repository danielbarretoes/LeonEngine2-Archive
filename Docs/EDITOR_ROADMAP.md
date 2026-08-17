# LeonEngine2 — Editor Roadmap (Future)

Contract only. **No `LeonEditor` executable in this milestone.**

## Goals

Provide a solo-dev editor later that reuses Runtime systems (same Unreal-like naming and flows) without inventing a parallel engine.

## Constraints

| Do | Do not |
| :--- | :--- |
| Out-of-process `Tools/LeonEditor` (or `Programs/`) exe | Embed editor UI inside `LeonEngineCore` game loop by default |
| Reuse `FWorldRenderer`, `UWorld`, `.lmap` serializers | Fork a second renderer |
| Edit data that already has disk formats (`.lmap`, `.lmat`, `.lmi`) | Require Blueprints / UHT / UBT |
| Ship after naming + physics + net RPC paths are stable | Block gameplay on editor existence |

## Suggested phases (after P0 systems)

1. **Map viewport** — load/save `.lmap`, select actors, move transforms, place `APlayerStart` / lights / static meshes.
2. **Material instance tweak** — edit scalars/textures on `.lmi` / `.lmat` with live preview.
3. **BT / Blackboard viewer** — read-only first (trees are still code-built); write `.lbt` only after executor + asset format exist.
4. **UMG layout** — Canvas/Button/Text placement for menus; optional.

## Dependencies before starting

- Naming contract stable ([NAMING.md](NAMING.md) Unreal prefix rules).
- Virtual paths `/Game` `/Engine` stable.
- Physics not dual-ticking (Jolt dynamics authority).
- Net ServerRPC framing stable for multiplayer PIE-like later.

## Non-goals (v1 editor)

- Full Slate, Cascade, Control Rig, Recast bake UI, shader graph, marketplace plugins.
