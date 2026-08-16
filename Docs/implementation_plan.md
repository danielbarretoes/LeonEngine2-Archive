# Implementation Plan — Static Lighting & Lightmap Baking (v0.15)

## Audit summary

| Area | Current state | Extension |
| :--- | :--- | :--- |
| Lights | `FDirectional/Point/SpotLight` + ECS components; all dynamic | Add `ELightMobility` |
| Meshes | `FStaticMeshVertex` has UV0 only; `.lmesh` v1 | UV1 + `.lmesh` v2 |
| Map | `.lmap` YAML via `FMapSerializer` | Mobility, lightmap refs |
| Renderer | `FWorldRenderer` + `PBR_Lit.glsl` slots 0–11 | Slot 12 lightmap |
| Assets | `UAssetManager` typed loaders | `EAssetType::Lightmap` + `.llightmap` |
| Hash | FNV-1a 64 (`ComputeFileHash64` / manifest) | Bake cache key |
| Sampling | `FIBLMath::CosineSampleHemisphere` | Reuse in baker |

## Naming decisions

| Concept | Name | Why |
| :--- | :--- | :--- |
| Baker | `FLightmass` | Unreal-like offline static lighting; not in the frame renderer |
| Builder/atlas | `FLightmapBuilder` | Packs UV islands into atlas |
| Asset | `FLightmapAsset` / `.llightmap` | Matches `.lmesh` / `.lhdr` native pattern |
| Mobility | `ELightMobility`, `EComponentMobility` | Unreal-style |

## Phases

1. Mobility enums + `.lmap` fields  
2. UV1 + mesh v2 + UV validation helpers  
3. `.llightmap` binary + AssetManager  
4. `FLightmass` CPU bake (direct + indirect + AO) + LeonAssetTool  
5. Runtime load + PBR sampling  
6. Tests, Sandbox demo, docs, walkthrough  

## Constraints

- Engine never references Sandbox.  
- No parallel lighting APIs.  
- Stationary: architecture only (documented), bake uses Static.  
- Deterministic seeds for sampling.
