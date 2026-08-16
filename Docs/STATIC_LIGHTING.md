# Static Lighting & Lightmap Baking (v0.15+)

Offline static lighting for indie-scale scenes. Inspired by Unreal Lightmass / early UE4, not Lumen.

## Concepts

| Enum | Values | Runtime / Bake |
| :--- | :--- | :--- |
| `ELightMobility` | Static, Stationary, Movable | **Static**: full bake (direct+indirect), excluded from dynamic UBO. **Stationary**: indirect bake only; direct + shadows remain dynamic (`!= Static` in UBO). **Movable**: fully dynamic, ignored by Lightmass. |
| `EComponentMobility` | Static, Stationary, Movable | Only **Static** geometry receives / samples lightmaps. |

Helpers: `IsLightmassBakeLight`, `DoesLightmassBakeDirect`.

## Assets

- **`.llightmap`**: native HDR atlas (`FLightmapAsset`), magic `LLLM`, version 1, RGBA32F payload + bake-input `ContentHash`.
- **`.lmesh` v2**: `LightmapUV` (UV1). Missing UV1 is generated at bake and **persisted** via `UStaticMesh::SaveToFile`. v1 loads with UV1 = UV0.
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
3. Resolve material albedo (base color + CPU texture average)
4. UV1 (generate + persist if missing)
5. Atlas (`FLightmapBuilder`, `TexelPadding`)
6. CPU bake (`FLightBaker`: Static direct; Static+Stationary in indirect gather; AO with `AORadius * WorldScale`)
7. Write `.llightmap` (`ContentHash` = bake input hash)
8. Stamp `.lmap` chart metadata + `LightmapBakeHash`

### Cache

`FLightmass::ComputeBakeInputHash(UWorld, settings)` fingerprints actors/transforms/mobility/resolutions, mesh/material file fingerprints, and bake knobs — **not** the stamped Scale/Bias/Index/hash fields. Second bake without `--force` skips when hash matches.

### Validate

`ValidateMap` requires Static meshes; when `StaticLighting` is on, also checks atlas exists, header valid, hash not stale, and chart metadata finiteness.

## Runtime

`FWorldRenderer` binds atlas slot **12**. `PBR_Lit.glsl`: `albedo * irradiance` (baked) + dynamic Movable/Stationary lights + specular IBL (diffuse IBL off when LM active).

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

No GPU Lightmass, no volumetric lightmaps, no photon mapping, no true Stationary shadow-map baking. Procedural meshes use UV0 as lightmap UV at runtime (`u_LightmapUseTexCoord`).
