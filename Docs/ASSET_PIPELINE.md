# LeonEngine2 — Native Asset Import Pipeline

LeonEngine2 features a high-performance, deterministic **Native Asset Import Pipeline** and CLI toolchain. The engine decouples raw authoring formats (FBX, PNG, TGA, JPG, BMP, HDR, EXR) from runtime rendering formats (`.lmesh`, `.lskeleton`, `.lskeletalmesh`, `.lanim`, `.lblend`, `.ltex`, `.lhdr`, `.lmat`, `.lmi`, `.llightmap`), ensuring zero runtime decoding overhead and rapid engine startup.

---

## Architecture Overview

```
 [Raw Authoring Files] (FBX, OBJ, PNG, TGA, JPG, BMP, HDR, EXR)
          │
          ▼
   [Specialized Tools] (AssetTool import/export / LightmassTool)
          │
          ├─── Texture Importer (stb_image / software box-filtered mips / gloss inversion)
          │         └─── Native Texture (.ltex) [sRGB/Linear, precalculated mips 0..N]
          │
          ├─── HDR Importer (stb_image / float payload / metadata container)
          │         └─── Native HDR Environment (.lhdr) [RGBA32F equirectangular panorama]
          │
          ├─── Mesh Importer (ufbx right-handed Y-up / static + skeletal)
          │         ├─── Native Static Mesh (.lmesh v5)
          │         ├─── Native Skeleton (.lskeleton)
          │         ├─── Native Skeletal Mesh (.lskeletalmesh v3)
          │         └─── Native Animation (.lanim) / Blend Space (.lblend v2)
          │
          ├─── Lightmass Tool (offline CPU bake)
          │         └─── Native Lightmap (.llightmap v2) [RGBA32F irradiance atlas + bake-input hash]
          │
          ├─── Material Extractor
          │         ├─── Master Material (.lmat) [Declarative Key-Value text]
          │         └─── Material Instance (.lmi) [Sparse overrides]
          │
          └─── Asset Manifest (Intermediate/AssetManifest.json) [64-bit FNV-1a content hashing]
```

---

## Versioning policy

| Rule | Behavior |
|------|----------|
| Magic | Unique 4-byte LE ASCII tag per format; never reuse |
| Exact-match versions | `.ltex`, `.lhdr`, `.llightmap`, `.lanim`, `.lskeleton` — unsupported version → Load fails |
| Range / convert | `.lmesh` v1–v5, `.lskeletalmesh` v1–v3, `.lblend` v1–v2 — older files upgrade on load |
| Bump when | On-disk field layout changes or semantics of an existing field change |
| Endianness | Little-endian host assumed (Win/x64); no BE marker |

---

## 1. Native Binary Formats

### 1.1 Native Texture Format (`.ltex`)
- **Magic**: `0x5845544C` (ASCII `LTEX`)
- **Version**: 1 (exact)
- **Header**: **68-byte** packed (`FLTexHeader`) — UUID, Width, Height, Channels, MipCount, Format (RGBA8 only), ColorSpace, Semantic, Wrap, Filter, **TotalDataSize** (sum of all mip headers + pixel bytes)
- **Mipchain**: Offline box-filtered mips to 1×1
- **Gloss inversion**: `_gloss` semantic inverts **RGB** channels (`255 - c`) during import (stored as roughness)
- **Load guards**: max dim 16384, max 32 mips, `DataSize == W*H*Channels`, `TotalDataSize` validated when non-zero

### 1.2 Native Static Mesh Format (`.lmesh`)
- **Magic**: `0x48534D4C` (ASCII **`LMSH`**, not the string "LMESH")
- **Version**: **5** (loads v1–v5)
  - v1/v2: legacy vertex layouts converted at load
  - v3+: packed canonical vertex (68 B: Pos, N, UV0, Tangent.xyzw, Color, LightmapUV)
  - v4: unique LightmapUV flag
  - v5: reduced LOD blobs
- **Load guards**: vertex/index/name length caps

### 1.2c Native Skeletal Assets
- **`.lskeleton`**: Magic `LSKL` (`0x4C4B534C`), version 1. Max 128 bones.
- **`.lskeletalmesh`**: Magic `LSKM` (`0x4D4B534C`), version **3**
  - v2: per-mesh inverse bind poses
  - v3: **`AssetForwardAxis`** (`EngineNegZ` / `SourcePosZ`); v1–v2 default to `SourcePosZ`
- **`.lanim`**: Magic `LANM` (`0x4D4E414C`), version 1
- **`.lblend`**: Magic `LBLD` (`0x444C424C`), version **2** (UUID); v1 loads without UUID (derived from path)

### 1.2b Native Lightmap Format (`.llightmap`)
- **Magic**: `LLLM` (`0x4D4C4C4C`)
- **Version**: **2**
- **Header**: 56-byte packed; **ContentHash** = bake-input hash (cache invalidation). Pixel FNV via `ComputePixelHash()`.
- **Payload**: RGBA32F irradiance (`rgb` = E, `a` = coverage). See [STATIC_LIGHTING.md](STATIC_LIGHTING.md).

### 1.3 Native HDR Environment Format (`.lhdr`)
- **Magic**: `0x5244484C` (ASCII `LHDR`)
- **Version**: 1 (exact)
- **Header**: **64-byte** packed. Cooked assets are always **RGBA32F + Equirectangular**, `MipCount=1`.
- **ExposureBias**: importer bakes the bias into pixels and stores **1.0** as remaining scale (Load never double-applies).
- **Load guards**: max dim 16384, Format must be RGBA32F, TotalDataSize checked when set

### 1.4 Material Assets (`.lmat`, `.lmi`)
- `.lmat`: master material (colors, roughness, metallic, textures, blend/raster state)
- `.lmi`: instance with `Parent` path + sparse overrides (`Roughness`, `Metallic`, `NormalScale`, `Albedo`/`AlbedoColor`)

### 1.5 Other
- **`.lphy`**: magic ASCII **`LPSH`** (`0x4853504C`), version 3
- **`.libl`**: `LEONIBL` cache header (64 B natural align); Load verifies file size ≥ header + face payloads

---

## 2. CLI Toolchain (`AssetTool`, `LightmassTool`, `ProjectTool` + Engine Scripts)

Prefer Engine Scripts (resolve Content from `.lproject`). See [SCRIPTS.md](SCRIPTS.md).

```bash
python Scripts/validate_project.py --project <path.lproject>
python Scripts/import_assets.py --project <path.lproject> [--force]
python Scripts/export_assets.py --project <path.lproject> [--force] [--format auto|obj|fbx|png|tga|jpg|bmp|hdr|exr]
python Scripts/validate_assets.py --project <path.lproject>
python Scripts/bake_lightmaps.py --project <path.lproject> [--map /Game/Maps/Name] [--force]

# Direct specialized tools:
AssetTool import --raw <raw_dir> --content <content_dir> [--force]
AssetTool export --content <content_dir> --raw <raw_dir> [--force] [--format auto|obj|fbx|png|tga|jpg|bmp|hdr|exr]
AssetTool export --asset <file.lmesh|ltex|lhdr> --out <path> [--format ...]
AssetTool validate --content <content_dir>
AssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lmat | file.lmi | file.llightmap>

LightmassTool bake --map <path.lmap> [--quality=Preview|Draft|Production] [--force]
LightmassTool validate --map <path.lmap>

ProjectTool validate_project --project <path.lproject>
ProjectTool validate_map --map <path.lmap>
```

Round-trip export: `.lmesh`↔OBJ/FBX, `.ltex`↔PNG/TGA/JPG/BMP, `.lhdr`↔HDR/EXR.

---

## 3. Dependency Graph & Incremental Import (`Intermediate/AssetManifest.json`)
The pipeline uses 64-bit FNV-1a hashing on source files stored in the project's `Intermediate/` cache directory. On subsequent import runs, unchanged assets are skipped with zero processing overhead, keeping the `Content/` directory 100% clean and free of tracking files.

---

## 4. Virtual Paths (`/Game/...`, `/Engine/...`) & Gameplay Framework Integration
- `FProjectPaths`: Resolves package virtual paths (`/Game/Maps/...`, `/Game/Meshes/...`, `/Game/Materials/...`, `/Game/Textures/...`, `/Game/HDR/...`) dynamically to the active project's physical `Content/` directory.
- `UWorld`: In-memory runtime world and ECS registry owning `AActor` instances and components.
- `MapSerializer`: Serializes and deserializes persistent map asset files (`.lmap`).
- `FStaticMeshComponent`: Component holding a reference to `FStaticMesh`, per-submesh material overrides, shadow flags, and reflection flags.
- `FAssetManager`: Deduplicates all loaded textures, static meshes, materials, and material instances using virtual path resolution.
