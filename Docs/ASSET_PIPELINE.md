# LeonEngine2 — Native Asset Import Pipeline

LeonEngine2 features a high-performance, deterministic **Native Asset Import Pipeline** and CLI toolchain. The engine decouples raw authoring formats (FBX, PNG, TGA, JPG, HDR) from runtime rendering formats (`.lmesh`, `.lskeleton`, `.lskeletalmesh`, `.lanim`, `.ltex`, `.lhdr`, `.lmat`, `.lmi`), ensuring zero runtime decoding overhead and rapid engine startup.

---

## Architecture Overview

```
 [Raw Authoring Files] (FBX, PNG, TGA, JPG, HDR)
          │
          ▼
   [LeonAssetTool]
          │
          ├─── Texture Importer (stb_image / software box-filtered mips / gloss inversion)
          │         └─── Native Texture (.ltex) [sRGB/Linear, precalculated mips 0..N]
          │
          ├─── HDR Importer (stb_image / float payload preservation / metadata container)
          │         └─── Native HDR Environment (.lhdr) [RGBA32F equirectangular panorama]
          │
          ├─── Mesh Importer (ufbx right-handed Y-up / static + skeletal)
          │         ├─── Native Static Mesh (.lmesh v2)
          │         ├─── Native Skeleton (.lskeleton)
          │         ├─── Native Skeletal Mesh (.lskeletalmesh)
          │         └─── Native Animation (.lanim)
          │
          ├─── Lightmass (offline CPU bake)
          │         └─── Native Lightmap (.llightmap) [RGBA32F irradiance atlas + header hash]
          │
          ├─── Material Extractor (Fuzzy token mapper & material slot generator)
          │         ├─── Master Material (.lmat) [Declarative Key-Value text]
          │         └─── Material Instance (.lmi) [Sparse overrides]
          │
          └─── Asset Manifest (Intermediate/AssetManifest.json) [64-bit FNV-1a content hashing & dependency graph]
```

---

## 1. Native Binary Formats

### 1.1 Native Texture Format (`.ltex`)
- **Magic**: `0x5845544C` (`'LTEX'`)
- **Version**: 1
- **Header**: 64-byte packed header containing UUID, Width, Height, Channels, MipCount, Format, ColorSpace, Semantic, and TotalDataSize.
- **Mipchain**: All mip levels down to $1\times 1$ are generated offline via box filtering ($2\times 2$ pixel area averaging).
- **Direct GPU Upload**: Loaded in a single I/O read and uploaded directly via `glTexSubImage2D` without runtime stb_image decompression.
- **Gloss Inversion**: If a texture has the `gloss` semantic, roughness is inverted ($R = 255 - G$) during import.

### 1.2 Native Static Mesh Format (`.lmesh`)
- **Magic**: `0x48534D4C` (`'LMESH'`)
- **Version**: 2 (v1 still loads; UV1 defaults from UV0)
- **Vertex Layout**: Pos, Normal, UV0, **UV1 (LightmapUV)**, Tangent, Bitangent, Color.

### 1.2c Native Skeletal Assets (`.lskeleton`, `.lskeletalmesh`, `.lanim`)
Skinned FBX (and Mixamo animation takes with bones but no mesh) import through the same `FMeshImporter` path — never as `.lmesh`.

- **`.lskeleton`**: Magic `LSKL` (`0x4C4B534C`). Bone names, parent indices, rest pose, inverse bind matrices. Max 128 bones.
- **`.lskeletalmesh`**: Magic `LSKM` (`0x4D4B534C`). Canonical skinned vertices (locations 0–5 + `ivec4` bone indices + `vec4` weights) and GPU palette via `PBR_Skinned.glsl`.
- **`.lanim`**: Magic `LANM` (`0x4D4E414C`). Per-bone TRS key tracks. Linked to a skeleton by bone name (`mixamorig:` prefix is stripped). Pose is never replicated; gameplay sends `FAnimRepState` (speed, direction, aim pitch, flags).
- **`.lblend`**: Magic `LBLD` (`0x444C424C`). 1D or 2D blend space samples (sequence paths + coordinates). Game projects own the sample layout.

### 1.2b Native Lightmap Format (`.llightmap`)
- **Magic**: `LLLM` (`0x4D4C4C4C`)
- **Version**: 1
- **Payload**: RGBA32F HDR irradiance atlas + content/bake hash. See [STATIC_LIGHTING.md](STATIC_LIGHTING.md).

### 1.3 Native HDR Environment Format (`.lhdr`)
- **Magic**: `0x5244484C` (`'LHDR'`)
- **Version**: 1
- **Header**: 64-byte packed header (`FLHDRHeader`) specifying UUID, Width, Height, Channels (4), Format (RGBA32F), Projection (0 = Equirectangular 2:1, 1 = Cubemap), ColorSpace (Linear), and ExposureBias.
- **Payload**: Full 32-bit floating point pixel buffers loaded directly into GPU textures or processed into `.libl` precalculated IBL cubemaps with zero runtime Radiance `.hdr` parsing overhead.

### 1.4 Material Assets (`.lmat`, `.lmi`)
- `.lmat`: Master material defining base colors, roughness, metallic, normal scale, textures, alpha cutoff, blend modes, and rasterizer states.
- `.lmi`: Material Instance referencing a parent `.lmat` with sparse property overrides.

---

## 2. CLI Toolchain (`LeonAssetTool` + Engine Scripts)

Prefer Engine Scripts (resolve Content from `.lproject`). See [SCRIPTS.md](SCRIPTS.md).

```bash
python Scripts/validate_project.py --project <path.lproject>
python Scripts/import_assets.py --project <path.lproject> [--force]

Sandbox layout: `Raw/HDR/*.hdr`, `Raw/Textures/T_*_*.jpg`, `Raw/Meshes/*.fbx` import into `Content/` as `.lhdr` / `.ltex` / `.lmesh`. Maps use `DaySky1k` and `NightSky1k`; `AutumnField1k.lhdr` is kept as the IBL/HDR test fixture.
python Scripts/validate_assets.py --project <path.lproject>
python Scripts/bake_lightmaps.py --project <path.lproject> [--map /Game/Maps/Name] [--force]

# Direct tool (paths absolute):
LeonAssetTool validate_project --project <path.lproject>
LeonAssetTool import --raw <raw_dir> --content <content_dir> [--force]
LeonAssetTool validate --content <content_dir>
LeonAssetTool validate_map --map <path.lmap>
LeonAssetTool bake_lightmaps --map <path.lmap> [--force]
LeonAssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lmat | file.lmi | file.llightmap>
```

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
