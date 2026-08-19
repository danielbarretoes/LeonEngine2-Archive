# IBL Cache Design

Binary cache for Image-Based Lighting (IBL) and the BRDF LUT. Math contracts: [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md).

## Responsibilities

Offline / first import bakes expensive convolutions; runtime uploads GPU-ready buffers.

```text
HDR (.hdr / .lhdr)
  → FHDREquirectangularMipChain
  → Irradiance (32×32×6) + Specular prefilter (128…8×6)
  → Serialize .libl

Runtime: verify header + FNV-1a hash → GPU upload (~ms on hit)
```

## BRDF LUT (`BRDF_LUT.bin`)

Path: `Engine/Assets/Textures/BRDF_LUT.bin`

- Header: `LEONBRDF` **v2** (`Size`, `SampleCount`) + `256×256×2` float RG payload.
- Split-sum scale/bias with IBL Smith (`k = a²/2`), texel centers `(x+0.5)/size`.
- Process cache: `FIBLGenerator::GenerateBRDFLUT`; released via `ReleaseStaticCaches()` on engine shutdown.

## `.libl` asset

Path pattern: `<HDR_dir>/Cache/IBL/<HDR_Stem>.libl`

### Header (`FIBLCacheHeader`, 64 bytes)

| Field | Typical | Notes |
| :--- | :--- | :--- |
| Magic | `LEONIBL` | |
| Version | **6** | Karis `saTexel`; exposure not baked |
| HDRSourceHash | FNV-1a 64-bit of HDR file bytes | Content invalidation |
| EnvSize / IrradSize / PrefilterBaseSize / PrefilterMips | 128 / 32 / 128 / 5 | Must match request |
| SampleCountIrradiance / Prefilter | 512 / 256 | |

### Payload

1. Environment cubemap: 6 × 128² × RGBA32F  
2. Irradiance cubemap: 6 × 32² × RGBA32F  
3. Prefiter mips: 6 × (128²+…+8²) × RGBA32F  

## Cache validity

A hit requires: file exists, magic/version match, HDR hash match, sizes/mips match request, file size matches header + payload.

On miss: rebuild mip chain → convolve → write `.libl` → upload.

Exposure and `EnvironmentIntensity` are **runtime** multipliers and must not be baked into the cache.
