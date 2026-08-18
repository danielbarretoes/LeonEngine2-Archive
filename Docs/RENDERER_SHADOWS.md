# Advanced Shadow System Architecture (v0.9.0)

LeonEngine2 v0.9.0 features a state-of-the-art shadow rendering pipeline engineered for mathematical correctness, sub-texel geometric stability, and high performance.

---

## 1. Architectural Overview

The shadow pipeline consists of:
- **Cascaded Shadow Maps (CSM)**: 4-layer 2D Texture Array (`GL_TEXTURE_2D_ARRAY`, `GL_DEPTH_COMPONENT32F`, $2048 \times 2048 \times 4$).
- **Stabilized Projection & Texel Snapping**: Bounding sphere enclosure of view frustum slices snapped to world-space shadow texel increments to eliminate camera rotation shimmering.
- **Analytical Practical Split Scheme**: Hybrid logarithmic/linear partition ($\lambda \in [0, 1]$).
- **Multi-Term Bias Architecture**: Combined constant depth bias, slope-scale depth bias, and normal offset bias to eliminate shadow acne without light leaks (Peter Panning).
- **Multi-Mode Filtering**:
  - `Hard`: Single tap hardware comparison.
  - `PCF 3x3`: 9-tap uniform filter grid.
  - `PCF 5x5`: 25-tap uniform filter grid.
  - `Poisson Disk`: 16 fixed LearnOpenGL offsets rotated by interleaved gradient noise (not a generated Vogel spiral).
- **Smooth Cascade Blending**: Mix over `max(cascadeLength × CascadeBlendWidth, 3 m)` (capped at 90% of the slice). Each cascade frustum overlaps that zone. Out-of-bounds samples of the next map are skipped (they used to mix toward fully lit and draw a floor line). NDC depth bias scales with the ortho Z range.
- **Far Shadow Distance Soft Fadeout**: Linear fade over 15 m past the last cascade split (`mix(shadow, 0, clamp((d − split.w) / 15, 0, 1))`).
- **Alpha Masked Caster Support**: Cutoff discard for masked materials (`u_AlphaMode == 1`).
- **Spotlight shadows**: one shadowed spot (`ShadowedSpotIndex`), resolution `SpotResolution`.
- **Forensic Debug Visualization Modes**: shadow factor, cascade index, cascade depth slices, spot shadow factor.

Contact shadows were removed (they were non-functional). See `Docs/RENDERER_CONTRACT.md`.

---

## 2. Mathematical Foundations

### 2.1 Practical Split Scheme
For $N$ cascade partitions between near plane $z_{\text{near}}$ and far shadow distance $z_{\text{far}}$, the split distance $z_i$ ($i \in \{1, \dots, N\}$) is:
$$z_{\text{log}, i} = z_{\text{near}} \left( \frac{z_{\text{far}}}{z_{\text{near}}} \right)^{i / N}$$
$$z_{\text{lin}, i} = z_{\text{near}} + (z_{\text{far}} - z_{\text{near}}) \frac{i}{N}$$
$$z_i = \lambda z_{\text{log}, i} + (1 - \lambda) z_{\text{lin}, i}$$
where $\lambda = 0.85$ provides high near-plane resolution while maintaining balanced far coverage.

### 2.2 Bounding Sphere Enclosure & Texel Snapping
To prevent sub-texel shimmering when the camera rotates:
1. The 8 world-space corners $\{ \mathbf{c}_k \}_{k=0}^7$ of each frustum slice are extracted via the inverse view-projection matrix:
   $$\mathbf{c}_k = \mathbf{V}^{-1} \mathbf{P}^{-1} \mathbf{ndc}_k$$
2. The centroid $\mathbf{C}$ and bounding radius $R$ are computed:
   $$\mathbf{C} = \frac{1}{8} \sum_{k=0}^7 \mathbf{c}_k, \quad R = \max_{k} \|\mathbf{c}_k - \mathbf{C}\|$$
3. The light-space position of $\mathbf{C}$ is snapped to texel increments:
   $$\Delta x = \frac{2R}{\text{Resolution}}$$
   $$\mathbf{C}_{\text{snapped}, x} = \text{floor}\left(\frac{\mathbf{C}_{\text{light}, x}}{\Delta x}\right) \cdot \Delta x$$
   $$\mathbf{C}_{\text{snapped}, y} = \text{floor}\left(\frac{\mathbf{C}_{\text{light}, y}}{\Delta x}\right) \cdot \Delta x$$

### 2.3 Normal Offset & Depth Bias
Shadow tests use the **rasterized face normal** \(\mathbf{N}_{\mathrm{face}} = \mathrm{normalize}(\partial\mathbf{p}/\partial x \times \partial\mathbf{p}/\partial y)\) (fallback: interpolated vertex normal). A Phong or detail normal would not match the triangle stored in the shadow map.

To prevent surface self-shadowing (acne) on angled geometry without disconnecting shadows from contact bases:
$$\text{slopeFactor} = 1.0 - \max(\mathbf{N}_{\mathrm{face}} \cdot \mathbf{L}, 0.0)$$
$$\tan\theta = \min\left(\frac{\sin\theta}{\max(\mathbf{N}_{\mathrm{face}} \cdot \mathbf{L},\, 0.08)},\, 8\right)$$
$$\mathbf{p}' = \mathbf{p} + \mathbf{N}_{\mathrm{face}} \cdot (\text{normalBias} \cdot \text{slopeFactor})$$
$$\text{bias} = \text{constBias} + \text{slopeBias} \cdot \tan\theta$$
$$z_{\text{test}} = z_{\text{light}}(\mathbf{p}') - \text{bias}$$

PCF / Poisson taps add a receiver-plane term (GPU Gems 3) so neighboring shadow texels are compared against the predicted \(z\) of the same triangle:
$$z_{\text{tap}} = z_{\text{test}} + \frac{\partial z}{\partial u}\Delta u + \frac{\partial z}{\partial v}\Delta v$$

Vertical receivers under a near-vertical light occupy ~1 shadow texel (the caster silhouette). Depth bias cannot move that sample; the UV is pushed along the projected normal by 4–8 texels. Shadow maps use `GL_NEAREST` comparison (software PCF only — hardware 2×2 LINEAR was mixing roof/ground at the sliver). Casters are drawn with polygon offset `(2, 4)`.

At grazing incidence the receiver is faded with \(\mathrm{smoothstep}(0, 0.22, \mathbf{N}_{\mathrm{face}}\cdot\mathbf{L})\). Defaults: `ConstantBias=0.001`, `SlopeBias=0.0035`, `NormalBias=0.04`.

### 2.4 Poisson Disk Filtering & Interleaved Gradient Noise
The 16 taps are a **fixed offset table** (LearnOpenGL Poisson disk), rotated per pixel with Jimenez interleaved gradient noise — not a generated Vogel spiral.

---

## 3. Shader Diagnostic Modes

| Mode | Name | Description |
| :--- | :--- | :--- |
| **0** | Composite Shading | Full PBR shading with shadows, IBL, bloom, tone mapping |
| **24** | Direct Shadow Factor | Direct shadow illumination ($1.0 = \text{lit}, 0.0 = \text{occluded}$) |
| **25** | Cascade Slice False-Color | Partition visualization: Cascade 0 (Red), 1 (Green), 2 (Blue), 3 (Yellow) |
| **26** | Spot Shadow Factor | Shadowed-spot occlusion (`1` = lit) |
| **27** | Cascade 0 Depth Map | Depth buffer visualization of cascade 0 |
| **28** | Cascade 1 Depth Map | Depth buffer visualization of cascade 1 |
| **29** | Cascade 2 Depth Map | Depth buffer visualization of cascade 2 |
| **30** | Cascade 3 Depth Map | Depth buffer visualization of cascade 3 |

---

## 4. Keyboard Shortcuts (Sandbox)

- `F11`: Cycle Shadow Forensic Debug Views (Composite $\to$ Factor $\to$ False-Color $\to$ Contact $\to$ Depth 0..3).
- `F12`: Cycle Shadow Filtering Mode (`Hard` $\to$ `PCF 3x3` $\to$ `PCF 5x5` $\to$ `Poisson Disk`).
