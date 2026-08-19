# Post-Processing & Anti-Aliasing

Transforms linear HDR radiance from the forward PBR path into LDR display output. Order matches `FPostProcessPipeline::Render`. Index: [RENDERER.md](RENDERER.md).

## Pipeline flow

Order: **SSAO → Bloom → Tone map (exposure) → FXAA**.

```
┌────────────────────────────────────────────────────────┐
│                   HDR Scene Buffer                     │
│               (RGBA16F, Linear Radiance)               │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│  SSAO (half-res depth, nearest samples, bilateral blur)│
│  Composite keeps high-luminance specular (planar/IBL)  │
└───────────────────────────┬────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
┌───────────────────────────┐             │
│        Pass 1: Bloom      │             │
│  - Soft-Knee Bright-Pass  │             │
│  - 13-Tap Jimenez Down    │             │
│  - 9-Tap Tent Additive Up │             │
└─────────────┬─────────────┘             │
              │ (Bloom Glow HDR)          │ (Scene HDR)
              └─────────────┬─────────────┘
                            ▼
┌────────────────────────────────────────────────────────┐
│             Pass 2: Tone Mapping & Composite           │
│  - Radiance Composite: SceneHDR + BloomHDR * Intensity │
│  - Camera Exposure Multiplier                          │
│  - Operator: ACES Filmic / Reinhard / Neutral / UC2    │
│  - Linear -> sRGB Display Gamma 2.2 Correction         │
│  - Perceptual Luma Encoding in Alpha Channel           │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│              Pass 3: FXAA Anti-Aliasing                │
│  - FXAA 3.11 Quality Algorithm                         │
│  - Contrast Delta Check & Subpixel Blend               │
│  - Tangent Search with Iteration Clamping              │
└───────────────────────────┬────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│           Target FFramebuffer / LDR Backbuffer          │
│                (RGBA8, sRGB Presented)                 │
└────────────────────────────────────────────────────────┘
```

---

## 2. Bloom Pipeline (Dual-Kawase / Jimenez Pyramid)

### 2.1 Soft-Knee Bright-Pass Extraction (`BloomBrightPass.glsl`)
Standard thresholding causes harsh, pixelated edges at the cutoff boundary. LeonEngine2 applies a continuous quadratic soft-knee threshold curve:

$$\text{luma} = 0.2126 R + 0.7152 G + 0.0722 B$$
$$\text{knee} = \max(\text{SoftKnee}, 10^{-4})$$
$$\text{soft} = \text{clamp}(\text{luma} - \text{Threshold} + \text{knee}, 0, 2 \times \text{knee})$$
$$\text{soft} = \frac{\text{soft}^2}{4 \times \text{knee} + 10^{-5}}$$
$$\text{contribution} = \frac{\max(\text{soft}, \text{luma} - \text{Threshold})}{\max(\text{luma}, 10^{-4})}$$
$$\text{Color}_{\text{bright}} = \text{Color} \times \max(\text{contribution}, 0)$$

### 2.2 Jimenez 13-Tap Downsampling (`BloomDownsample.glsl`)
Downsampling uses Jorge Jimenez's 13-tap filter pattern (sampling 36 bilinear texels across 4 box groupings) with Karis luminance weighting on Mip 0 to prevent subpixel fireflies:

$$\text{Weight}_{\text{Karis}}(C) = \frac{1}{1 + \text{Luma}(C)}$$

The 13-tap normalized filter weights sum strictly to $1.0$:
- 4 Central box bilinear taps: weight $0.125$ each ($0.5$ total)
- 4 Corner bilinear taps (top-left, top-right, bottom-left, bottom-right): weight $0.03125 \times 4 = 0.125$ per box ($0.5$ total)

### 2.3 9-Tap Tent Upsampling with Additive Blending (`BloomUpsample.glsl`)
Upsampling expands progressively from the lowest downsampled mip back up to Mip 0 using a $3\times 3$ tent filter kernel with configurable blur radius:

$$\begin{bmatrix} 1 & 2 & 1 \\ 2 & 4 & 2 \\ 1 & 2 & 1 \end{bmatrix} \times \frac{1}{16}$$

Each upsampling pass additively blends (`GL_ONE`, `GL_ONE`) the upsampled higher-blur mip with the corresponding downsampled mip, synthesizing a smooth, cinematic glow.

---

## 3. Tone Mapping & Color Grading (`ToneMapping.glsl`)

### 3.1 ACES Filmic Operator (Narkowicz Fit)
The default photographic tone mapper utilizes Krzysztof Narkowicz's industry-standard rational approximation of the ACES S-curve:

$$\text{ACESFilm}(x) = \text{clamp}\left( \frac{x(2.51x + 0.03)}{x(2.43x + 0.59) + 0.14}, 0.0, 1.0 \right)$$

### 3.2 Extended Reinhard Operator
$$\text{ReinhardExtended}(x) = \frac{x \left(1 + \frac{x}{W^2}\right)}{1 + x}, \quad W = 4.0$$

### 3.3 Uncharted 2 (John Hable) Operator
Filmic curve offering independent toe, shoulder, and linear control points.

### 3.4 Neutral / encode
- **Neutral Clamp**: Direct linear clamp to $[0, 1]$ for diagnostics.
- **Display encode**: IEC piecewise `LinearToSRGB` after the tone operator (not a blind `pow(1/2.2)`).
- **Perceptual luma in alpha**: Rec. 601 luma for FXAA:
  $$L = 0.299 R + 0.587 G + 0.114 B$$

---

## 4. FXAA 3.11 Anti-Aliasing (`FXAA.glsl`)

LeonEngine2 implements Timothy Lottes' complete FXAA 3.11 Quality algorithm operating on the sRGB LDR buffer:
1. **Local Contrast Check**: Early-exits flat regions when $\Delta L < \max(0.03125, L_{\max} \times 0.125)$.
2. **Edge Orientation Detection**: Evaluates horizontal vs vertical local gradients using $3\times 3$ corner and orthogonal luma samples.
3. **Tangent Search**: Traverses up to 12 iterations with variable step quality $[1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0]$ to identify edge endpoints.
4. **Subpixel Blending**: Blends subpixel offsets based on low-pass $3\times 3$ box filter delta to eliminate single-pixel aliasing.

---

## 5. Runtime Interactive Controls

| Hotkey | Action | Notes |
|:---|:---|:---|
| `F3` | Toggle Post-Processing Pipeline | Master on/off switch |
| `F4` | Toggle FXAA Anti-Aliasing | Toggles Pass 3 FXAA edge smoothing |
| `F5` | Cycle Post-Process Debug Views | Cycles through Composite (0), Raw HDR (1), Bloom Only (2), Bright Pass (3), Tone Map Only (4) |
| `F6` / `F7` | Decrease / Increase Exposure | Adjusts exposure multiplier by $\pm 0.1$ |
| `F8` / `F9` | Decrease / Increase Bloom Intensity | Adjusts bloom glow intensity by $\pm 0.01$ |
