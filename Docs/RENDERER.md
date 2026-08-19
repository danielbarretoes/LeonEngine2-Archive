# Renderer Documentation Index

Forward PBR renderer for LeonEngine2 (OpenGL 4.5, solo / indie scope). Entry points: `FWorldRenderer`, `IRenderAPI` / `IRenderDriver`, plugin `Plugins/RHI/OpenGL`.

Canonical frame order and math live in [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md). High-level engine wiring: [ARCHITECTURE.md](ARCHITECTURE.md) §6–7.

| Document | Role |
| :--- | :--- |
| [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md) | Implemented contracts: coordinates, PBR, IBL, shadows, lightmaps, planar, exposure, limitations |
| [RENDERER_REMEDIATION.md](RENDERER_REMEDIATION.md) | Audit follow-up: P0–P3 fix order (no Unreal feature wishlist) |
| [RENDERER_SHADOWS.md](RENDERER_SHADOWS.md) | CSM / spot shadow math, bias, filtering |
| [RENDERER_POSTPROCESSING.md](RENDERER_POSTPROCESSING.md) | SSAO → bloom → tone map → FXAA |
| [RENDERER_MATERIALS.md](RENDERER_MATERIALS.md) | Material slots, alpha modes, TBN, color space |
| [IBL_CACHE_DESIGN.md](IBL_CACHE_DESIGN.md) | `.libl` v6 and BRDF LUT disk cache |
| [STATIC_LIGHTING.md](STATIC_LIGHTING.md) | Lightmass bake / `.llightmap` / mobility |
| [PERFORMANCE_AUDIT.md](PERFORMANCE_AUDIT.md) | Measured LeonTournament frame costs |
| [TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md) | Engine/GPU test layout and known gaps |
| [NAMING.md](NAMING.md) | UE-style prefixes (`FWorldRenderer`, `IRenderAPI`, …) |

## Frame pipeline (summary)

```text
Gather lights → Lighting UBO → UpdateIBL
→ CSM → Spot shadow → Planar reflection
→ HDR opaque (frustum cull + instancing ≤64) → Skybox → Transparent
→ 3D text → Particles → Gameplay debug
→ SSAO → Bloom → Tone map → FXAA → UI / overlay → Present
```

## Scope

Designed for one developer: maintainable forward path, not Unreal parity. Missing clustered lights, reverse-Z, OIT, SSR, or a render thread are **intentional** unless listed as REQUIRED in [RENDERER_REMEDIATION.md](RENDERER_REMEDIATION.md).
