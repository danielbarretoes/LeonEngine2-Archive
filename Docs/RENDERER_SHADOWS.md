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
  - `Poisson Disk`: 16-tap Vogel spiral with per-pixel interleaved gradient noise jitter rotation.
- **Smooth Cascade Blending**: Linear interpolation across cascade transition boundaries (`u_ShadowParams.w`).
- **Far Shadow Distance Soft Fadeout**: Exponential decay to 0.0 at far cascade distance.
- **Alpha Masked Caster Support**: Cutoff discard for masked materials (`u_AlphaMode == 1`).
- **Screen-Space Contact Shadows**: Ray-marched screen-space occlusion for high-frequency fine geometry contact details.
- **Forensic Debug Visualization Modes**: Real-time diagnostic overlays for shadow factors, cascade partitioning, contact shadows, and individual depth maps.

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
To prevent surface self-shadowing (acne) on angled geometry without disconnecting shadows from contact bases:
$$\text{slopeFactor} = 1.0 - \max(\mathbf{N} \cdot \mathbf{L}, 0.0)$$
$$\mathbf{p}' = \mathbf{p} + \mathbf{N} \cdot (\text{normalBias} \cdot \text{slopeFactor})$$
$$\text{bias} = \text{constBias} + \text{slopeBias} \cdot \text{slopeFactor}$$
$$z_{\text{test}} = z_{\text{light}}(\mathbf{p}') - \text{bias}$$

### 2.4 Poisson Disk Filtering & Interleaved Gradient Noise
The 16 Poisson taps follow the Vogel distribution:
$$\theta_i = i \cdot \Phi, \quad r_i = \sqrt{\frac{i + 0.5}{16}}, \quad \Phi \approx 2.399963229728653 \text{ rad}$$
Each pixel rotates the sampling disc by an angle derived from Jimenez's Interleaved Gradient Noise (IGN):
$$\text{IGN}(\mathbf{x}) = \text{fract}(52.9829189 \cdot \text{fract}(0.06711056 x + 0.00583715 y))$$
$$\mathbf{R} = \begin{bmatrix} \cos(2\pi \cdot \text{IGN}) & -\sin(2\pi \cdot \text{IGN}) \\ \sin(2\pi \cdot \text{IGN}) & \cos(2\pi \cdot \text{IGN}) \end{bmatrix}$$
$$\mathbf{p}_i = \mathbf{p}_{\text{uv}} + \mathbf{R} \cdot \mathbf{d}_i \cdot \text{diskRadius} \cdot \text{texelSize}$$

### 2.5 Screen-Space Contact Shadows
For fine contact geometry (e.g. crevices, small bevels):
$$\mathbf{x}(t) = \mathbf{x}_{\text{start}} + t \cdot (\mathbf{x}_{\text{end}} - \mathbf{x}_{\text{start}}), \quad t \in [0, 1]$$
At each ray step $k \in \{0, \dots, M-1\}$, the scene depth $z_{\text{scene}} = \text{texture}(u_{\text{DepthMap}}, \mathbf{x}_k.\text{xy})$ is queried:
$$\text{if } (z_{\text{ray}} > z_{\text{scene}} \text{ and } z_{\text{ray}} - z_{\text{scene}} < \text{thickness}) \implies \text{occluded}$$
Integrated multiplicatively with shadow map factor:
$$S_{\text{total}} = 1.0 - (1.0 - S_{\text{CSM}}) \cdot S_{\text{Contact}}$$

---

## 3. Shader Diagnostic Modes

| Mode | Name | Description |
| :--- | :--- | :--- |
| **0** | Composite Shading | Full PBR shading with shadows, IBL, bloom, tone mapping |
| **24** | Direct Shadow Factor | Direct shadow illumination ($1.0 = \text{lit}, 0.0 = \text{occluded}$) |
| **25** | Cascade Slice False-Color | Partition visualization: Cascade 0 (Red), 1 (Green), 2 (Blue), 3 (Yellow) |
| **26** | Contact Shadow Factor | Screen-space ray-marched occlusion factor |
| **27** | Cascade 0 Depth Map | Depth buffer visualization of cascade 0 |
| **28** | Cascade 1 Depth Map | Depth buffer visualization of cascade 1 |
| **29** | Cascade 2 Depth Map | Depth buffer visualization of cascade 2 |
| **30** | Cascade 3 Depth Map | Depth buffer visualization of cascade 3 |

---

## 4. Keyboard Shortcuts (Sandbox)

- `F11`: Cycle Shadow Forensic Debug Views (Composite $\to$ Factor $\to$ False-Color $\to$ Contact $\to$ Depth 0..3).
- `F12`: Cycle Shadow Filtering Mode (`Hard` $\to$ `PCF 3x3` $\to$ `PCF 5x5` $\to$ `Poisson Disk`).
