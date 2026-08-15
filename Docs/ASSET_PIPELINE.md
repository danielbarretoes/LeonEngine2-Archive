# LeonEngine2 — Native Asset Import Pipeline

LeonEngine2 features a high-performance, deterministic **Native Asset Import Pipeline** and CLI toolchain. The engine decouples raw authoring formats (FBX, PNG, TGA, JPG, HDR) from runtime rendering formats (`.lmesh`, `.ltex`, `.lhdr`, `.lmat`, `.lmi`), ensuring zero runtime decoding overhead and rapid engine startup.

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
          ├─── Mesh Importer (ufbx right-handed Y-up parsing / multi-submesh preservation / tangents)
          │         └─── Native Static Mesh (.lmesh) [positions, normals, UVs, tangents, bitangents, bounds]
          │
          ├─── Material Extractor (Fuzzy token mapper & material slot generator)
          │         ├─── Master Material (.lmat) [Declarative Key-Value text]
          │         └─── Material Instance (.lmi) [Sparse overrides]
          │
          └─── Asset Manifest (manifest.json) [64-bit FNV-1a content hashing & dependency graph]
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
- **Magic**: `0x4853454D` (`'MESH'`)
- **Version**: 1
- **Vertex Layout**: 68-byte packed vertex stride (`Float3 Pos`, `Float3 Normal`, `Float2 UV`, `Float3 Tangent`, `Float3 Bitangent`, `Float3 Color`).
- **Submesh Preservation**: Preserves independent submeshes and material slot assignments without arbitrary mesh collapsing.
- **Bounding Volumes**: Stores precomputed Axis-Aligned Bounding Box ($\mathbf{Min}, \mathbf{Max}$) and Bounding Sphere ($\mathbf{Center}, Radius$).

### 1.3 Native HDR Environment Format (`.lhdr`)
- **Magic**: `0x5244484C` (`'LHDR'`)
- **Version**: 1
- **Header**: 64-byte packed header (`FLHDRHeader`) specifying UUID, Width, Height, Channels (4), Format (RGBA32F), Projection (0 = Equirectangular 2:1, 1 = Cubemap), ColorSpace (Linear), and ExposureBias.
- **Payload**: Full 32-bit floating point pixel buffers loaded directly into GPU textures or processed into `.libl` precalculated IBL cubemaps with zero runtime Radiance `.hdr` parsing overhead.

### 1.4 Material Assets (`.lmat`, `.lmi`)
- `.lmat`: Master material defining base colors, roughness, metallic, normal scale, textures, alpha cutoff, blend modes, and rasterizer states.
- `.lmi`: Material Instance referencing a parent `.lmat` with sparse property overrides.

---

## 2. CLI Toolchain (`LeonAssetTool`)
Located at `Tools/LeonAssetTool/`:

```bash
# Import all raw assets (Textures, Meshes, HDR) incrementally
LeonAssetTool import --raw <raw_dir> --content <content_dir> [--force]

# Validate all native assets in content directory
LeonAssetTool validate --content <content_dir>

# Validate level asset links, actors, lights, camera, and static meshes
LeonAssetTool validate_level --level <path.llevel>

# Inspect native binary header and metadata
LeonAssetTool inspect <file.lhdr | file.ltex | file.lmesh | file.lmat | file.lmi>
```

---

## 3. Dependency Graph & Incremental Import (`manifest.json`)
The pipeline uses 64-bit FNV-1a hashing on source files. On subsequent import runs, unchanged assets are skipped with zero processing overhead.

---

## 4. Integration with Scene & ECS
- `FScene`: In-memory runtime world and ECS registry owning entities and components.
- `FLevelSerializer`: Serializes and deserializes persistent level asset files (`.llevel`).
- `FStaticMeshComponent`: Component holding a reference to `FStaticMesh`, per-submesh material overrides, shadow flags, and reflection flags.
- `FAssetManager`: Deduplicates all loaded textures, static meshes, materials, and material instances using virtual path resolution (`Meshes/...`, `Materials/...`, `Textures/...`, `HDR/...`).
