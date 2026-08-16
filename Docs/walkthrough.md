# Walkthrough — Static Lighting & Lightmap Baking (v0.15)

## What changed

- Mobility: `ELightMobility`, `EComponentMobility` on light/mesh components; serialized in `.lmap`.
- Mesh UV1 + `.lmesh` v2; UV validation helpers (`FLightmapUV`).
- Native `.llightmap` atlas asset + `UAssetManager::GetLightmap`.
- Offline CPU bake: `FLightmass` / `FLightBaker` / `FLightmapBuilder`.
- Runtime: `PBR_Lit` samples lightmap (binding 12); Static lights skipped in dynamic UBO.
- LeonAssetTool: `bake_lightmaps`, `validate_lightmaps`, `inspect .llightmap`.
- Sandbox `ShowcaseLevel`: procedural primitives + `DaySky1k` HDRI. `NightLevel`: imported `.lmesh` (`Cottage_FREE`, `Car`, `PalmTree`, `StreetLamp`, `Ground`) + `NightSky1k`. HUD chip travels between the two maps.

## Commands

```bat
python Scripts/bake_lightmaps.py --project Projects/Sandbox/Sandbox.lproject --force
python Scripts/bake_lightmaps.py --project Projects/Sandbox/Sandbox.lproject --map /Game/Maps/NightLevel --force
LeonAssetTool bake_lightmaps --map Projects/Sandbox/Content/Maps/ShowcaseLevel.lmap --force
LeonAssetTool validate_lightmaps --map Projects/Sandbox/Content/Maps/ShowcaseLevel.lmap
LeonAssetTool inspect Projects/Sandbox/Content/Lightmaps/ShowcaseLevel.llightmap
```

## Architecture (short)

```
.lmap → FLightmass → atlas + FLightBaker → .llightmap
UWorld → FWorldRenderer → PBR_Lit (dynamic + IBL + baked)
```

## Test results

- Bake both Sandbox maps after content changes (`--force`).
- Engine / Sandbox / LeonAssetTool build clean.

- Procedural meshes use UV0 as lightmap UV at runtime.
- Bake is CPU; keep SamplesPerTexel / resolution modest for large scenes.
- No GPU Lightmass / Lumen / volumetric LMs.

## How to use

1. Mark meshes `Mobility: Static`, lights `Mobility: Static`.
2. Set Environment `StaticLighting: true` (+ bake settings).
3. Run `bake_lightmaps`.
4. Open the map — lightmaps load via `LightmapAsset` paths written into the `.lmap`.
