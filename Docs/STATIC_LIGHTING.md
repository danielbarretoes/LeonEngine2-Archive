# Static Lighting & Lightmap Baking (v0.15+)

Offline static lighting for indie-scale scenes. Inspired by Unreal Lightmass / early UE4, not Lumen.

## Concepts

| Enum | Values | Runtime / Bake |
| :--- | :--- | :--- |
| `ELightMobility` | Static, Stationary, Movable | **Static**: full bake (direct+indirect), excluded from dynamic UBO. **Stationary**: indirect bake only; direct + shadows remain dynamic (`!= Static` in UBO). **Movable**: fully dynamic, ignored by Lightmass. |
| `EComponentMobility` | Static, Stationary, Movable | Only **Static** geometry receives / samples lightmaps. |

Helpers: `IsLightmassBakeLight`, `DoesLightmassBakeDirect`.

## Assets

- **`.llightmap`**: native HDR atlas (`FLightmapAsset`), magic `LLLM`, version **2**, RGBA32F payload (`rgb` = irradiance `E`, `a` = coverage) + bake-input `ContentHash`.
- **`.lmesh` v3**: packed tangent `vec4` + `LightmapUV`. v1/v2 migrate on load. Missing UV1 is generated at bake (split per triangle) and **persisted** via `UStaticMesh::SaveToFile`.
- **`.lmap` Environment**: `StaticLighting`, `LightmapResolution`, `NumIndirectBounces`, `SamplesPerTexel`, `IndirectIntensity`, `AmbientOcclusion`, `AOIntensity`, `AORadius`, `TexelPadding`, `WorldScale`, `LightmapAsset`, `LightmapBakeHash`.

## Offline bake (LeonAssetTool)

```
python Scripts/bake_lightmaps.py --project <path.lproject> [--map /Game/Maps/Name] [--force]
python Scripts/bake_lightmaps.py --project <path.lproject> --validate-only

LeonAssetTool bake_lightmaps --map <path.lmap> [--force]
LeonAssetTool validate_lightmaps --map <path.lmap>
LeonAssetTool inspect <path.llightmap>
```

Pipeline:

1. Load map
2. Collect Static meshes + Static/Stationary lights
3. Resolve material albedo (base color × albedo map sample at UV0)
4. UV1 (generate + persist if missing)
5. Atlas (`FLightmapBuilder`, `TexelPadding`)
6. CPU bake (`FLightBaker`): stores **diffuse irradiance** `E = ∫ Li max(N·ω,0) dω`. Static lights contribute direct `E`; Static+Stationary contribute to GI. Cosine misses add sky `E += π L_env` (HDR or atmosphere, scaled by `EnvironmentIntensity`, not `IndirectIntensity`). `NumIndirectBounces == 0` skips GI but still includes environment miss. Receptor emissive is not stored. Bake AO can scale stored `E`. Bake-input hash algorithm version **4** (includes skybox/HDR).
7. Write `.llightmap` (`ContentHash` = bake input hash)
8. Stamp `.lmap` chart metadata + `LightmapBakeHash`

### Cache

`FLightmass::ComputeBakeInputHash(UWorld, settings)` fingerprints actors/transforms/mobility/resolutions, mesh/material file fingerprints, and bake knobs — **not** the stamped Scale/Bias/Index/hash fields. Second bake without `--force` skips when hash matches.

### Validate

`ValidateMap` requires Static meshes; when `StaticLighting` is on, also checks atlas exists, header valid, hash not stale, and chart metadata finiteness.

## Runtime

`FWorldRenderer` binds atlas slot **12**. `PBR_Lit.glsl`: `Lo_diffuse = kD * albedo / PI * E` (baked irradiance) + dynamic Movable/Stationary lights + specular IBL (diffuse IBL off when a lightmap is bound).

See `Docs/RENDERER_CONTRACT.md` for equations, mobility, and cache versions.

### Debug (F7 cycle)

| Mode | `u_DebugMode` | Shows |
| :--- | :--- | :--- |
| Dynamic only | 10 | Runtime lights (`Lo`) |
| Baked only | 31 | `albedo * lightmap` |
| Lightmap irradiance | 32 | Raw atlas HDR |
| Lightmap UV | 33 | Atlas UV (R/G); blue=1 if no lightmap |
| Dyn + Baked | 34 | `Lo + baked` without IBL |
| Lit | 0 (F12) | Full composite |

## Modules

- `FLightmass` — orchestration (tool entry)
- `FLightmapBuilder` — atlas pack
- `FLightBaker` — shading (same attenuation model as `FLight` / PBR)
- `FLightmapAsset` / `FLightmapUV` — asset + UV helpers

## Limits (vs Unreal)

No GPU Lightmass, no volumetric lightmaps, no photon mapping, no true Stationary shadow-map baking. Procedural meshes use UV0 as lightmap UV at runtime (`u_LightmapUseTexCoord`). Imported NightLevel meshes store generated UV1 in `.lmesh`.

## Sandbox maps

| Map | Geometry | HDRI | Lightmap |
| :--- | :--- | :--- | :--- |
| `/Game/Maps/ShowcaseLevel` | Procedural primitives (studio floor 36×32) | `DaySky1k` | `ShowcaseLevel.llightmap` |
| `/Game/Maps/NightLevel` | Imported static meshes | `NightSky1k` | `NightLevel.llightmap` |

`AutumnField1k.lhdr` is an engine IBL/HDR test fixture under Sandbox `Content/HDR/`; neither map references it. Rebake after content changes: `python Projects/Sandbox/Scripts/bake.py --force`.
