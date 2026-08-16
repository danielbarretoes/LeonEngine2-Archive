# Renderer Contract

Canonical rendering contracts for LeonEngine2. This document describes the **implemented** pipeline, not a wishlist.

Source of truth: `Engine/Source/Runtime` and `Engine/Assets/Shaders`.

## Coordinate system

| Property | Value |
| :--- | :--- |
| Handedness | Right-handed |
| Up | +Y |
| Camera forward | −Z |
| Clip / NDC | OpenGL (`z` in `[-1, 1]`, `y` up) |
| Front face | CCW |
| Culling | Back (unless the material is double-sided) |
| Depth | Classic OpenGL, near `0.1`, far `1000`. Not reversed-Z. |

World scale: scenes on the order of meters. Near/far is sufficient for that range.

Planar reflections are **Y = 0 only** (mirror across the XZ plane).

## Vertex layout

One static-mesh layout: `FCanonicalMeshVertex` (68 bytes, 17 tightly packed floats). Shader locations are explicit:

| Location | Attribute | Type |
| :--- | :--- | :--- |
| 0 | Position | vec3 |
| 1 | Normal | vec3 |
| 2 | UV0 | vec2 |
| 3 | Tangent | vec4 (xyz + handedness `w`) |
| 4 | Color | vec3 **linear** |
| 5 | Lightmap UV (UV1) | vec2 |

Bitangent is not stored. Reconstruct `B = cross(N, T) * w`. Invariant: `T × B ≈ N` when `w = +1`. Mirrored UVs use `w = −1`. Negative scale also flips `w` in the vertex shader via `determinant(u_NormalMatrix)`.

On-disk `.lmesh` version **3** matches this layout. Versions 1 and 2 are converted at load (no dual runtime path).

## Color management

```
MaterialColor  = linear
TextureColor   = hardware-decoded linear (GL_SRGB8_ALPHA8 for sRGB color maps)
Data maps      = linear (normal, roughness, metallic, AO)
Lighting       = linear HDR
Display        = bloom → exposure → tone map → gamma 2.2 → LDR
```

sRGB textures are decoded **once** by the GPU. Shaders must not `pow(rgb, 2.2)`.

Albedo / emissive CPU constants are linear. Baker albedo averages decode sRGB with the piecewise IEC 61966-2-1 transfer (`SRGBToLinear`).

## PBR (direct)

Cook-Torrance with GGX NDF, Smith geometry (direct `k = (r+1)²/8`), Schlick Fresnel.

```
Lo = (kD * albedo / PI + specular) * radiance * NdotL * shadow
```

`kD = (1 - F) * (1 - metallic)`.

## IBL

Irradiance cubemap stores

```
E(N) = ∫ Li(ω) max(N·ω, 0) dω
```

Diffuse:

```
Lo_diffuse = albedo / PI * E
```

Prefilter: GGX/Karis split-sum. BRDF LUT uses IBL Smith (`k = a²/2`), texel centers `(x+0.5)/size`. Cache `.libl` **v5** hashes HDR content, algorithm version, resolutions, and sample counts. **Exposure and environment intensity are not baked into IBL.**

Runtime: IBL and skybox multiply `EnvironmentIntensity`. Scene `Exposure` is applied only in `ToneMapping.glsl`.

Energy check: uniform `Li = 1` ⇒ `E = π`. Albedo `1` ⇒ `Lo_diffuse = 1` (`u_DebugMode == 35`).

## Shadows

- Directional: 4-cascade CSM, configurable resolution.
- Spotlight: **one** shadowed spot (`FShadowSettings.ShadowedSpotIndex`, default `0`). Viewport uses `SpotResolution`.
- Contact shadows: **removed**.
- Depth: classical `GL_LEQUAL` compare. Bias: constant + slope + normal offset.

## Lightmaps

Stored value is **diffuse irradiance**:

```
E(x) = ∫Ω Li(x, ωi) max(N·ωi, 0) dω
```

Runtime:

```
Lo_diffuse = kD * albedo / PI * E
```

When a lightmap is bound, diffuse IBL is replaced by the lightmap; specular IBL remains.

- `NumIndirectBounces == 0` disables GI (direct only).
- GI estimator: cosine hemisphere sampling, `E += π * Lo_hit`, throughput `albedo` per extra bounce.
- Receptor **emissive is runtime-only**. Other surfaces contribute emissive through GI `Li`.
- Bake AO is **not** multiplied into `E`. Artistic AO is the material AO map at runtime.
- Spotlight angular factor is the same Hermite smoothstep as `PBR_Lit.glsl` (`FLightAttenuation.hpp`).

`.llightmap` version **2**. Cache key includes baker algorithm version `2`, geometry, transforms, lights (including disabled), materials, UVs, resolution, bounces, samples, AO settings.

### Mobility

| Light | Bake | Runtime direct / shadows |
| :--- | :--- | :--- |
| Static | Direct + indirect | No |
| Stationary | Indirect only | Yes |
| Movable | No | Yes |

Only **Static** mesh components sample lightmaps.

## Exposure

One scene exposure (`FSkyboxComponent::Exposure` → post-process `u_Exposure`). Lighting buffers stay linear HDR. Changing exposure does not invalidate IBL or lightmap caches.

`EnvironmentIntensity` (default `1.2`) is an artistic environment scale, not a `/π` compensation.

## Transparency

1. Opaque and masked: depth test and write on.
2. Transparent (`EAlphaMode::Blend`): sorted back-to-front, depth test on, depth write off.

## Negative scale

`det(M) < 0` flips face culling and tangent handedness. Combined with Y=0 planar reflection, both winding flips apply.

## Resize

Window resize → `UEngine` → `FWorldRenderer::OnViewportResize` → HDR FBO, planar FBO, bloom/tone-map/FXAA targets, UI viewport.

## Cache

| Asset | Version / key |
| :--- | :--- |
| `.libl` | v5, HDR hash, sizes, sample counts |
| BRDF LUT | `LEONBRDF` v1 |
| `.llightmap` | v2 + bake input hash |
| `.lmesh` | v3 (v1/v2 migrated on load) |

## Debug views (`u_DebugMode`)

| Mode | View |
| ---: | :--- |
| 0 | Lit composite |
| 9 | IBL specular |
| 10 / 36 | Direct Lo |
| 11 / 17 | World normal |
| 14 | Base color |
| 15 | Metallic |
| 16 | Roughness |
| 18 | AO |
| 19 | Emissive |
| 20 | Tangent |
| 21 | Bitangent |
| 22 | UV0 |
| 24 | Shadow factor |
| 32 | Lightmap irradiance |
| 33 | UV1 |
| 35 | IBL diffuse (`albedo/π * E`) |
| 37 | HDR before tone map |
| 38 | Direct diffuse |
| 39 | Direct specular |

## Limitations

- Forward renderer, one draw per mesh. No clustered lights, VSM, or GPU-driven path.
- At most 16 point lights and 8 spot lights in the UBO; one shadowed spotlight.
- Planar reflection is the Y=0 plane only.
- No OIT.
- No reversed-Z.
- Bake AO settings may still appear on maps; they do not modulate stored irradiance.
- Golden PNG references are not shipped; GPU tests check mathematical constraints (energy, sRGB, resize).
