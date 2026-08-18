# LeonEngine2 — Advanced Materials & Surface Detail Pipeline (v0.8.0)

## 1. Executive Summary

LeonEngine2 v0.8.0 introduces a production-grade, physically robust **PBR Material Subsystem** that builds directly on top of the Cook-Torrance direct lighting model, the Dual-Kawase HDR Bloom pipeline, ACES Tone Mapping, and FXAA 3.11 anti-aliasing.

The material pipeline provides full support for:
1. **Normal Mapping & Tangent Space Orthogonalization:** Robust Gram-Schmidt basis reconstruction in fragment shading ($T, B, N$), with scaling (`u_NormalScale`) and strict fallback to geometric normals when disabled.
2. **PBR Texture Workflow:** Standardized texture channel layout with strict sRGB-to-Linear conversion for color maps (Albedo, Emissive) and linear data retention for physics channels (Metallic, Roughness, Ambient Occlusion).
3. **Deterministic Texture Fallbacks:** Exact scalar parameter evaluation when texture maps are absent, eliminating shader branching dependencies on default texture contents.
4. **Physically Decoupled Emissive Radiance:** Emissive contribution enters directly into HDR linear radiance ($Lo += \text{emissive}$), unaffected by directional cosines ($N \cdot L$), shadow maps, or light counts, providing HDR values ($> 1.0$) that directly feed the Bloom pyramid.
5. **Alpha Modes & Culling:** Support for `Opaque`, `Mask` (alpha testing with `u_AlphaCutoff` and fragment discard), and `Blend` (translucency), along with material-driven double-sided back-face culling bypass.
6. **Texture Coordinate Transformations:** Uniform UV coordinate scaling and translation (`u_UVTiling`, `u_UVOffset`).
7. **Forensic Material Debug Views:** 11 dedicated debug visualization modes (`F10` key) spanning Base Color, Metallic, Roughness, World Normal, AO, Emissive, Tangent $T$, Bitangent $B$, UV coordinates, and Direct $N \cdot L$.

---

## 2. Mathematical Architecture

### 2.1 Tangent Space & Gram-Schmidt Orthogonalization

To eliminate normal interpolation distortion across curved triangle surfaces, the fragment shader performs per-fragment Gram-Schmidt orthogonalization on interpolated basis vectors:

$$T' = \text{normalize}(T - N \cdot (N \cdot T))$$
$$B' = \text{normalize}(B - N \cdot (N \cdot B) - T' \cdot (T' \cdot B))$$
$$\text{TBN} = \begin{bmatrix} T'_x & B'_x & N_x \\ T'_y & B'_y & N_y \\ T'_z & B'_z & N_z \end{bmatrix}$$

When a normal map is active:
$$\mathbf{n}_{\text{TS}} = \text{texture}(u\_\text{NormalMap}, \mathbf{uv}) \times 2.0 - 1.0$$
$$\mathbf{n}_{\text{TS}, xy} \times= u\_\text{NormalScale}$$
$$\mathbf{n}_{\text{world}} = \text{normalize}(\text{TBN} \times \text{normalize}(\mathbf{n}_{\text{TS}}))$$

When `u_UseNormalMap == 0`:
$$\mathbf{n}_{\text{world}} = \text{normalize}(v\_\text{Normal})$$

---

### 2.2 Color Spaces & Radiance Accumulation

```
┌─────────────────────────────────────────────────────────────┐
│ 2D Textures on Disk (PNG / JPG)                             │
└──────────────┬───────────────────────────────┬──────────────┘
               │                               │
       (sRGB Color Space)              (Linear Data Space)
       Albedo & Emissive               Metallic, Roughness, AO
               │                               │
               ▼                               ▼
       texture(sampler, uv).rgb         texture(sampler, uv).r
               │                               │
               ▼                               ▼
       pow(sample, 2.2) (Linear RGB)   Direct Linear Use
               │                               │
               └───────────────┬───────────────┘
                               │
                               ▼
               Cook-Torrance PBR Shading Equation
               HDR Color = Ambient(IBL * AO) + Lo(Direct) + Emissive
                               │
                               ▼
               Dual-Kawase Bloom Extraction & Blur
                               │
                               ▼
               ACES Filmic Tone Mapping + Gamma 2.2
                               │
                               ▼
               FXAA 3.11 Anti-Aliasing -> 8-bit Backbuffer
```

---

## 3. Shader Texture Slots & Uniform Protocol

### 3.1 Texture Slot Allocation Table

| Slot | Sampler Name | Format | Color Space | Role |
| :--- | :--- | :--- | :--- | :--- |
| **0** | `u_AlbedoMap` | RGBA8 | sRGB $\to$ Linear | Base Color & Alpha Mask |
| **1** | `u_NormalMap` | RGBA8 | Linear | Tangent-Space Normal Map |
| **2** | `u_MetallicMap` | R8 / RGBA8 | Linear | Metallic Channel (Red) |
| **3** | `u_AOMap` | R8 / RGBA8 | Linear | Ambient Occlusion (Red) |
| **4** | `u_RoughnessMap` | R8 / RGBA8 | Linear | Roughness Channel (Red) |
| **5** | `u_PlanarReflectionMap` | RGBA16F | Linear HDR | Floor planar capture (projective UV, not screen UV) |
| **6** | `u_BRDFLUT` | RG16F | Linear | Split-Sum 2D LUT |
| **7** | `u_IrradianceMap` | Cubemap RGBA32F | Linear HDR | Diffuse Irradiance Environment |
| **8** | `u_PrefilterMap` | Cubemap RGBA32F | Linear HDR | Specular Prefiltered Environment |
| **9** | `u_EmissiveMap` | RGBA8 | sRGB $\to$ Linear | Emissive Radiance Map |
| **10** | `u_CascadeShadowMap` | 2DArray Depth | Depth Compare | Directional CSM Shadow Map |
| **11** | `u_SpotShadowMap` | 2D Depth | Depth Compare | Spotlight Shadow Map |
| **13** | `u_PlanarReflectionMap1` | RGBA16F | Linear HDR | Optional wall planar capture |

---

### 3.2 Material Debug Views (`F10` Key / `u_DebugMode`)

| Mode ID | Name | Output Formula |
| :--- | :--- | :--- |
| **0** | Full Shading Composite | Full HDR PBR Lit + Post-Processing |
| **14** | Base Color / Albedo | $\mathbf{c}_{\text{albedo}}$ |
| **15** | Metallic | $\text{vec3}(\text{metallic})$ |
| **16** | Roughness | $\text{vec3}(\text{roughness})$ |
| **17** | Normal | $\mathbf{N} \times 0.5 + 0.5$ |
| **18** | Ambient Occlusion | $\text{vec3}(\text{ao})$ |
| **19** | Emissive Radiance | $\mathbf{L}_{\text{emissive}}$ |
| **20** | Tangent Vector | $\mathbf{T} \times 0.5 + 0.5$ |
| **21** | Bitangent Vector | $\mathbf{B} \times 0.5 + 0.5$ |
| **22** | UV Coordinates | $\text{vec3}(\text{fract}(\mathbf{uv}), 0.0)$ |
| **23** | Direct Cosine $N \cdot L$ | $\text{vec3}(\max(\mathbf{N} \cdot \mathbf{L}, 0.0))$ |

### 3.3 `UsePlanarReflection`

Set this on materials that **are** the registered reflector (studio floor, wet street, wall mirror panel). Do not set it on chrome spheres, car bodies, or other curved metals: those sample the IBL prefilter cubemap. The shader also fades planar weight when the fragment is more than ~40 cm from the capture plane, so a leftover flag cannot stretch the floor grab over a ball.

---

## 4. Test Verification & Mutation Coverage

* **Regression Test Suite:** 57 test cases, 8,102,320 assertions, **0 failures**.
* **GLSL Shader Mutation Testing:** 40/40 mutations caught (**100.0% detection rate**).
* **C++ Mutation Testing Audit:** 13/15 mutations caught (**86.7% baseline preserved**).
