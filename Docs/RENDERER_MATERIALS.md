# Materials & Surface Detail

PBR materials for LeonEngine2. Contracts: [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md). Types: `FMaterial`, `FMaterialInstance` (Renderer module — not `UObject`).

## Features

- Gram-Schmidt TBN in the fragment shader; `u_NormalScale`; fallback to geometric normal
- Color maps (albedo, emissive): hardware sRGB decode via `.ltex` / `GL_SRGB8_ALPHA8`
- Data maps (normal, metallic, roughness, AO): linear
- Alpha: `Opaque`, `Mask` (`discard`), `Blend` (depth write off)
- UV tiling / offset; double-sided cull bypass
- Debug modes via `u_DebugMode` (see contract)

Prefer on-disk `.ltex` over raw PNG so semantic / color-space metadata is authoritative.

## Color path

```
sRGB color maps  → GPU decode once → linear shading
Linear data maps → linear sample
HDR lighting     → bloom → exposure → tone map → IEC sRGB encode → LDR
```

Shaders must not apply an extra `pow(rgb, 2.2)` on already-decoded color maps.

## Texture slots

| Slot | Sampler | Space | Role |
| :--- | :--- | :--- | :--- |
| 0 | `u_AlbedoMap` | sRGB → linear | Base color / alpha |
| 1 | `u_NormalMap` | Linear | Tangent-space normal |
| 2 | `u_MetallicMap` | Linear | Metallic (R) |
| 3 | `u_AOMap` | Linear | AO (R) |
| 4 | `u_RoughnessMap` | Linear | Roughness (R) |
| 5 | `u_PlanarReflectionMap` | Linear HDR | Floor planar |
| 6 | `u_BRDFLUT` | Linear | Split-sum LUT |
| 7 | `u_IrradianceMap` | Linear HDR | Diffuse IBL |
| 8 | `u_PrefilterMap` | Linear HDR | Specular IBL |
| 9 | `u_EmissiveMap` | sRGB → linear | Emissive |
| 10 | `u_CascadeShadowMap` | Depth compare | CSM |
| 11 | `u_SpotShadowMap` | Depth compare | Spot shadow |
| 12 | Lightmap | Linear HDR | Baked irradiance |
| 13 | `u_PlanarReflectionMap1` | Linear HDR | Optional wall planar |

## Planar flag

`UsePlanarReflection` is for surfaces **on** a registered plane (floor / panel). Curved chrome uses IBL. The shader fades planar weight by distance to the plane.
