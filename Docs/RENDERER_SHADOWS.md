# Shadow System

Cascaded shadow maps (CSM) and one shadowed spotlight for LeonEngine2. Contracts: [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md). Remediation: [RENDERER_REMEDIATION.md](RENDERER_REMEDIATION.md).

## Overview

- **CSM**: 4-layer `GL_TEXTURE_2D_ARRAY`, `GL_DEPTH_COMPONENT32F` (resolution from `FShadowSettings`, often 2048²).
- **Stabilization**: frustum-slice bounding sphere + texel snapping to reduce swimming.
- **Splits**: Practical (hybrid log/linear) scheme, λ ≈ 0.85.
- **Bias**: constant + slope-scale + normal offset.
- **Filters**: Hard, PCF 3×3, PCF 5×5, Poisson disk (fixed LearnOpenGL offsets + interleaved gradient noise).
- **Cascade blend**: mix over `max(cascadeLength × CascadeBlendWidth, 3 m)` (capped); out-of-bounds next-map samples skipped.
- **Far fade**: linear fade ~15 m past the last split.
- **Spot**: one map (`ShadowedSpotIndex`), `SpotResolution`.
- Contact shadows: removed.

CSM uses front-face cull + polygon offset `(2, 4)`. Spot uses back-face cull. Pass entry/exit calls `ResetDefaultMeshRasterState()`.

### Known gaps (P1)

- No frustum / light-space cull of casters (every cascade redraws the full caster set).
- Alpha mask on casters: procedural meshes yes; static/skinned force `u_AlphaMode = 0`.

## Practical split scheme

For \(N\) partitions between \(z_{\text{near}}\) and shadow far \(z_{\text{far}}\):

$$z_{\text{log}, i} = z_{\text{near}} \left( \frac{z_{\text{far}}}{z_{\text{near}}} \right)^{i / N}$$
$$z_{\text{lin}, i} = z_{\text{near}} + (z_{\text{far}} - z_{\text{near}}) \frac{i}{N}$$
$$z_i = \lambda z_{\text{log}, i} + (1 - \lambda) z_{\text{lin}, i}$$

## Bounding sphere & texel snap

1. Eight world corners of each slice from inverse view-projection.
2. Centroid \(\mathbf{C}\) and radius \(R\).
3. Snap light-space \(\mathbf{C}\) to texel size \(2R / \text{Resolution}\).

## Bias

Shadow tests use the **rasterized face normal** (fallback: vertex normal). Detail normals would not match the depth map.

$$\text{slopeFactor} = 1.0 - \max(\mathbf{N}_{\mathrm{face}} \cdot \mathbf{L}, 0.0)$$
$$\tan\theta = \min\left(\frac{\sin\theta}{\max(\mathbf{N}_{\mathrm{face}} \cdot \mathbf{L},\, 0.08)},\, 8\right)$$
$$\mathbf{p}' = \mathbf{p} + \mathbf{N}_{\mathrm{face}} \cdot (\text{normalBias} \cdot \text{slopeFactor})$$
$$\text{bias} = \text{constBias} + \text{slopeBias} \cdot \tan\theta$$

PCF taps add a receiver-plane depth prediction. Maps use `GL_NEAREST` compare (software PCF). Grazing receivers fade with \(\mathrm{smoothstep}(0, 0.22, \mathbf{N}_{\mathrm{face}}\cdot\mathbf{L})\). Defaults: `ConstantBias=0.001`, `SlopeBias=0.0035`, `NormalBias=0.04`.

## Debug modes

| Mode | Name |
| :--- | :--- |
| 24 | Direct shadow factor |
| 25 | Cascade false-color |
| 26 | Spot shadow factor |
| 27–30 | Cascade depth visualizations |

Sandbox: `F11` cycles shadow debug views; `F12` cycles filter modes.
