# Milestone Implementation Plan — Advanced Shadow System (v0.9.0)

## 1. Executive Summary & Audit Findings

### 1.1 Current Architecture Audit (Phase 0)
- **Current CSM:** 3 static cascades (split0 = 4.5, split1 = 20.0, split2 = 75.0) stored in `DEPTH32F_ARRAY_SHADOW` ($2048 \times 2048$).
- **Current Spot Shadow:** 1 perspective shadow map ($1024 \times 1024$).
- **Current Point Light Shadows:** Not yet implemented (point light evaluated without shadow occlusion).
- **Current Filtering & Bias:** Hardcoded 9-tap 3x3 grid loop in `PBR_Lit.glsl` with fixed dot-product slope bias (`0.0005 * (1 - NdotL)`). No Normal Offset Bias, no Poisson disk sampling, no contact shadows, no cascade blending.
- **Current Stabilization:** Basic texel snapping along X and Y axes, but lacking bounding sphere stabilization during camera rotation.

### 1.2 Target Architecture for v0.9.0
We will implement an industry-standard, physically robust **Advanced Shadow System** supporting:
1. **4 Cascades for Directional Lights:** Practical Split Scheme ($\lambda \in [0, 1]$ blending logarithmic and uniform splits).
2. **Robust Frustum Fitting & Stabilization:** Bounding sphere / texel grid snapping preventing shimmering during camera movement and rotation.
3. **Shadow Resource Architecture & Shadow Atlas:** Abstractions (`FShadowSettings`, `FShadowCascade`, `FShadowMap`, `FShadowAtlas`) managing shadow render targets, resolutions, and viewport partitions with UV scale/offset and safety gutters to eliminate edge bleeding.
4. **Comprehensive Filtering (PCF 1-tap, 3x3, 5x5, Poisson Disk):** Runtime-selectable filter modes (`Hard`, `PCF3x3`, `PCF5x5`, `Poisson16`, `Poisson32`).
5. **Multi-Term Bias Model:** Constant Depth Bias, Slope Scale Bias, and **Normal Offset Bias** ($p' = p + \mathbf{n} \cdot \text{normalBias} \cdot \text{texelSize}$) eliminating surface acne and peter-panning.
6. **Smooth Cascade Blending:** Optional transition width (`CascadeBlendWidth`) removing harsh boundary lines between cascade levels.
7. **Screen-Space Contact Shadows:** Lightweight screen-space depth ray marching towards light sources capturing fine contact details (creases, feet, contact points) that sub-texel shadow maps miss.
8. **Point Light & Spot Light Shadow Quality:** Unified bias and filtering architecture preserving existing spot lights while enabling point light omnidirectional cubemap / atlas shadows.
9. **Forensic Shadow Debug Views:** Atlas overview, individual cascades 0–3, cascade index false-color visualization, shadow occlusion factor, PCF sample contribution, contact shadow factor, and linearized light depth.
10. **Headless GPU Testing & Mutation Shield:** 6 new GPU test suites executing real shaders on offscreen OpenGL 4.5 Core, expanded shader mutation suite (`MUT_SHADOW_A` through `MUT_SHADOW_P`) with **100% detection rate**, and preserved C++ mutation audit.

---

## 2. Proposed Architectural Changes

### Component 1: Shadow Data Model & Core Abstractions (`Engine/include/renderer/ShadowTypes.hpp`)
Create `ShadowTypes.hpp` defining:
- `EShadowFilterMode`: `Hard` (1 tap), `PCF3x3` (9 taps), `PCF5x5` (25 taps), `Poisson` (Poisson disk).
- `ECascadeSplitScheme`: `Practical` ($\lambda$), `Logarithmic`, `Uniform`.
- `FShadowSettings`:
  - `bEnableShadows`: bool
  - `bEnableContactShadows`: bool
  - `FilterMode`: `EShadowFilterMode`
  - `CascadeCount`: 4
  - `SplitLambda`: float (0.0 = uniform, 1.0 = logarithmic, default 0.85)
  - `ConstantBias`: float (default 0.001f)
  - `SlopeBias`: float (default 0.002f)
  - `NormalBias`: float (default 0.02f)
  - `ShadowDistance`: float (e.g. 100.0f)
  - `CascadeBlendWidth`: float (e.g. 0.1f)
  - `ContactShadowDistance`: float (default 0.25f)
  - `ContactShadowThickness`: float (default 0.05f)
  - `ContactShadowSteps`: int (default 16)
  - `bStabilizeCascades`: bool (default true)
- `FShadowCascade`: Stores LightSpaceMatrix, SplitNear, SplitFar, OrthoSize, AtlasUVScale, AtlasUVOffset.

---

### Component 2: Math & Cascade Calculations (`Engine/include/renderer/ShadowMath.hpp` / `Engine/src/renderer/ShadowMath.cpp`)
- **Practical Split Scheme:**
  $$C_i = \lambda \cdot z_{\text{near}} \left(\frac{z_{\text{far}}}{z_{\text{near}}}\right)^{i / N} + (1 - \lambda)\left(z_{\text{near}} + \frac{i}{N}(z_{\text{far}} - z_{\text{near}})\right)$$
- **Frustum Corner Extraction:** Compute 8 world-space corners of each frustum slice from camera inverse view-projection.
- **Bounding Sphere / Texel Snapping Stabilization:**
  - Compute minimum bounding sphere enclosing the 8 frustum corners.
  - Fix the orthographic projection extent to the sphere radius (invariant to camera rotation).
  - Snap light view position to texel increments:
    $$\Delta x = \frac{2 \times \text{radius}}{\text{AtlasRes}}, \quad \text{pos}_x = \lfloor \text{pos}_x / \Delta x \rfloor \times \Delta x$$

---

### Component 3: GPU Uniform Buffer Objects (`Engine/include/renderer/SceneRenderer.hpp`)
Extend `FCameraBufferData` (Binding 0) and shadow uniforms:
- `LightSpaceMatrices[4]`: 4 cascade matrices.
- `SpotLightSpaceMatrix`: 1 spot light matrix.
- `CascadeSplits`: `vec4(split0, split1, split2, split3)`.
- `CascadeOffsets[4]`: `vec4` containing atlas UV scale & offset (`scaleX, scaleY, offsetX, offsetY`).
- `ShadowParams`: `vec4(constantBias, slopeBias, normalBias, cascadeBlendWidth)`.
- `ShadowSettings`: `ivec4(filterMode, contactShadowSteps, bEnableContactShadows, shadowDebugMode)`.
- `ContactShadowParams`: `vec4(contactShadowDistance, contactShadowThickness, 0, 0)`.

---

### Component 4: GLSL Shaders (`Engine/Assets/Shaders/PBR_Lit.glsl` & `ShadowDepth.glsl`)
1. **Poisson Disk Sampling:** Include 16-tap Poisson distribution table with spiral Vogel disk distribution.
2. **Normal Offset Bias Calculation:**
   $$\mathbf{p}_{\text{offset}} = \mathbf{p} + \mathbf{N} \cdot (\text{normalBias} \cdot \text{worldUnitsPerTexel})$$
3. **Multi-Mode Filter Switch:**
   - `Hard`: Single depth comparison.
   - `PCF3x3`: 9 bilinear filtered taps.
   - `PCF5x5`: 25 filtered taps.
   - `Poisson`: 16/32 Poisson disk samples with rotary interleaved gradient noise jitter.
4. **Cascade Selection & Smooth Blending:**
   - Find active cascade index $i \in \{0, 1, 2, 3\}$.
   - If within `CascadeBlendWidth` of boundary, sample cascade $i$ and cascade $i+1$ and blend linearly.
5. **Screen-Space Contact Shadows:**
   - Ray march in screen space from surface towards light direction in view space.
   - Compare ray depth against scene depth buffer.
   - Return soft occlusion factor $\in [0, 1]$.
6. **Shadow Debug Views (`u_DebugMode` 24–30):**
   - Mode 24: Shadow Occlusion Factor (Grayscale)
   - Mode 25: Cascade Index False-Color (0 = Red, 1 = Green, 2 = Blue, 3 = Yellow)
   - Mode 26: Contact Shadow Factor Only
   - Mode 27: Cascade 0 Depth Map
   - Mode 28: Cascade 1 Depth Map
   - Mode 29: Cascade 2 Depth Map
   - Mode 30: Cascade 3 Depth Map

---

### Component 5: SceneRenderer Pipeline Integration (`Engine/src/renderer/SceneRenderer.cpp`)
- Instantiate 4-cascade `FFramebuffer` ($2048 \times 2048$ `DEPTH32F_ARRAY_SHADOW` or $4096 \times 4096$ Atlas).
- Update `RenderCascadedShadowPass` with 4 cascades, practical split scheme, and texel stabilization.
- Update `RenderSpotShadowPass` with normal offset bias support.
- Pass depth texture to main pass for screen-space contact shadows.

---

### Component 6: Headless GPU Tests (`Tests/Shader/`)
Implement 6 new headless GPU test suites:
1. `Tests/Shader/ShadowCascadeTests.cpp`: Validate 4-cascade matrices, split boundaries ($\lambda = 0.0$ vs $1.0$).
2. `Tests/Shader/ShadowPCFTests.cpp`: Validate Hard, PCF 3x3, PCF 5x5, and Poisson filter monotonicity and softening.
3. `Tests/Shader/ShadowBiasTests.cpp`: Validate Normal Offset Bias and Slope Scale Bias against surface angle variations.
4. `Tests/Shader/ShadowAtlasTests.cpp`: Validate cascade layer/atlas coordinate mapping and edge clamp safety.
5. `Tests/Shader/ShadowSelectionTests.cpp`: Validate cascade selection across boundary $\pm \epsilon$ and smooth blend.
6. `Tests/Shader/ShadowContactTests.cpp`: Validate screen-space contact shadow ray marcher with obstructors.

---

### Component 7: Mutation Testing (`Scripts/run_shader_mutations.py`)
Add 16 shadow mutations:
- `MUT_SHADOW_A`: Invert cascade selection boundary check.
- `MUT_SHADOW_B`: Swap cascade splits vector order.
- `MUT_SHADOW_C`: Invert atlas UV scale / layer mapping.
- `MUT_SHADOW_D`: Invert atlas UV offset.
- `MUT_SHADOW_E`: Drop constant depth bias.
- `MUT_SHADOW_F`: Invert normal offset bias direction ($+ \to -$).
- `MUT_SHADOW_G`: Disable slope scale bias.
- `MUT_SHADOW_H`: Reduce PCF samples (corrupt tap loop).
- `MUT_SHADOW_I`: Corrupt Poisson disk weights / sample offsets.
- `MUT_SHADOW_J`: Invert depth comparison operator ($< \to >$).
- `MUT_SHADOW_K`: Invert shadow occlusion factor ($1 - s \to s$).
- `MUT_SHADOW_L`: Disable cascade stabilization snapping.
- `MUT_SHADOW_M`: Invert contact shadow occlusion factor.
- `MUT_SHADOW_N`: Zero out directional shadow factor.
- `MUT_SHADOW_O`: Use wrong cascade light matrix index.
- `MUT_SHADOW_P`: Drop cascade smooth blend interpolation.

Target: **100% caught rate (56/56 total shader mutations)**.

---

### Component 8: Showcase Scene & Interactive Runtime Controls
- Create `Projects/Sandbox/Content/Maps/ShadowShowcase.llevel` or showcase configuration featuring large ground planes, elevation steps, slopes, overlapping blockers, pillars, and multiple light types.
- Bind `F11` (Shadow Debug View cycling) and `F12` (Shadow Filter Mode cycling: Hard $\to$ PCF 3x3 $\to$ PCF 5x5 $\to$ Poisson).

---

### Component 9: Documentation
- Create `Docs/RENDERER_SHADOWS.md`.
- Update `Docs/RENDERER_TEST_COVERAGE.md`, `Docs/ARCHITECTURE.md`, `CHANGELOG.md` (v0.9.0).

---

## 3. Verification Plan

### Automated Verification
```powershell
# 1. Build and run all mathematical and GPU tests
python Scripts/run_tests.py

# 2. Run all GLSL shader mutation tests (aiming for 56/56 100% caught)
python Scripts/run_shader_mutations.py

# 3. Run C++ mutation audit
python Scripts/run_mutation_audit.py

# 4. Full compilation
cmake --build build --config Debug
```

### Manual & Visual Verification
- Run `Sandbox.exe` and verify:
  - Cascade transitions are seamless without popping or shimmering.
  - Normal offset bias eliminates acne on curved spheres and slope planes without detachment.
  - PCF and Poisson modes produce soft, smooth penumbras.
  - Contact shadows ground small geometry and crevices.
  - Shadow debug views clearly render cascade indices (red, green, blue, yellow) and occlusion factors.
