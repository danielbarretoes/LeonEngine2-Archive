# Architecture Decisions & Industry Best Practices — LeonEngine2

This document details the architectural decisions, design rationale, and industry best practice alignments that govern **LeonEngine2** as a modern, scalable **Micro-Unreal Engine** (C++20, Dear ImGui Docking, EnTT, CMake, and Python tooling).

---

## 1. Design Vision: The "Micro-Unreal" Philosophy

Unreal Engine provides one of the industry's most battle-tested and extensible game engine architectures. However, for solo and small-team development, Unreal's full toolchain introduces significant overhead:
- A custom C# build toolchain (*UnrealBuildTool* / *UnrealAutomationTool*).
- An expensive code-generation pre-pass (*UnrealHeaderTool*) for reflection and garbage collection.
- Monolithic compile times and complex memory management models.

**LeonEngine2** adopts Unreal Engine's core architectural patterns while implementing them with modern C++20 and standard tools:
- **Unreal Gameplay Framework**: `UWorld` $\rightarrow$ `AGameModeBase` $\rightarrow$ `APlayerController` $\rightarrow$ `APawn` $\rightarrow$ `AHUD` $\rightarrow$ `UWidget`.
- **Strict Naming & Type Prefixes**: `U` (Engine/Managed), `A` (Actors), `F` (Structs/Values), `I` (Interfaces), `E` (Enums), `T` (Templates), `b` (Booleans).
- **Fast Modern C++**: Native `std::shared_ptr` / `TRef<T>`, `TScope<T>`, and high-performance ECS via **EnTT** with zero GC overhead.
- **Lightweight Tooling**: Standard **CMake + Ninja + Python 3** providing instant configure and compile cycles.

---

## 2. Product Separation & Build Model

### The Three Independent CMake Products
Rather than a single monolithic build, LeonEngine2 separates code into three distinct CMake products, writing to isolated output directories under `out/`:

```text
┌────────────────────────────────────────────────────────────────────────────────┐
│ 1. ENGINE PRODUCT (cmake -S . -B out/Engine -DLEON_PRODUCT=Engine)             │
│    Outputs: LeonEngineCore.lib, Plugins (OpenGL, Jolt, ENet), LeonAssetTool,   │
│             and unit/GPU tests (out/Engine/)                                   │
├────────────────────────────────────────────────────────────────────────────────┤
│ 2. EDITOR PRODUCT (cmake -S Editor -B out/Editor)                              │
│    Outputs: LeonEditor.exe (out/Editor/)                                       │
│    Consumes: LeonEngineCore + Plugins + Dear ImGui Docking + Editor Panels     │
├────────────────────────────────────────────────────────────────────────────────┤
│ 3. PROJECT PRODUCT (cmake -S . -B out/Projects/<Name> -DLEON_PRODUCT=Project)  │
│    Outputs: _project/<Game>.exe (out/Projects/<Name>/)                         │
│    Consumes: LeonEngineCore + Plugins + Single Game Source (Zero Editor/ImGui) │
└────────────────────────────────────────────────────────────────────────────────┘
```

### Rationale:
1. **Zero Editor Bloat in Runtime Games**: The game executable (`_project/<Game>.exe`) never includes ImGui or editor panels.
2. **Multi-Project Scalability**: The engine does not hardcode any game names. Any external `.lproject` can be built against the engine using `python Scripts/build_project.py --project <path>`.
3. **Clean Incremental Builds**: Changes to editor UI do not invalidate game runtime compilation artifacts, and vice versa.

---

## 3. Asset Pipeline & Virtual Path Mounting

LeonEngine2 strictly decouples physical file locations on disk from asset references in code and maps:

```text
/Engine/Shaders/PBR_Lit.glsl  ──►  Engine/Assets/Shaders/PBR_Lit.glsl
/Game/Maps/ShowcaseLevel      ──►  Projects/<Project>/Content/Maps/ShowcaseLevel.lmap
/Game/Materials/M_Floor       ──►  Projects/<Project>/Content/Materials/M_Floor.lmat
```

### Rationale:
- **Portability**: Projects can be relocated anywhere on disk without breaking map references or shader includes.
- **Layer Separation**: Engine fallback assets (default white texture, BRDF LUT, PBR shaders) live in `/Engine/`, while game-specific assets live in `/Game/`.

---

## 4. Out-of-Process Baking & Static Lighting

Baking operations (lightmaps, reflection probes, IBL prefiltering) are computationally heavy and require tools like FBX mesh importers (`ufbx`) and raytracing calculators.

```text
[ Raw FBX / Maps ] ──► [ Tools/LeonAssetTool bake_lightmaps ] ──► [ Cooked .llightmap / .libl ]
                                                                             │
                                              [ Game Runtime (Lightmass-free) ]
```

### Rationale:
- **Lean Runtime**: Game clients only load lightweight, binary-cooked files (`.llightmap`, `.libl`). They do not link FBX importers or baking raytracers into the game binary.
- **Deterministic Quality Settings**: `bake_lightmaps.py` exposes explicit `--quality` tiers (`Preview`, `Draft`, `Production`) executed headlessly.

---

## 5. Shipping & Distribution Pipeline

Production game releases are staged and packaged via [`Scripts/package_project.py`](../Scripts/package_project.py):

```powershell
python Scripts/package_project.py --project Projects/Sandbox/Sandbox.lproject --config Release
```

### Package Process:
1. **Optimized Build**: Compiles in `Release` configuration with maximum optimization flags (`/O2`, link-time code generation where supported).
2. **Symbol Stripping**: Runs `strip --strip-unneeded` on the executable (on applicable toolchains) to reduce binary size and protect intellectual property.
3. **Staging Directory Assembly**: Collects only runtime essentials into `Projects/<Name>/Dist/Shipping/<Name>/`:
   - `<Name>.exe` and `<Name>.lproject`
   - `Content/` (game assets)
   - `Config/` (Multi-INI configurations)
   - `Engine/Assets/` (runtime shaders, fonts, fallback textures)
   - Compiler runtime DLLs (MSVC / MinGW redistributables)
4. **Archive Creation**: Compresses the staging directory into `<Name>-Win64-Shipping.zip` for one-click distribution.

---

## 6. Industry Best Practices Comparison

| Architectural Principle | LeonEngine2 Implementation | Industry Standard (Unreal / Godot / AAA) | Assessment |
| :--- | :--- | :--- | :---: |
| **Product Decoupling** | Discrete CMake targets (`Engine`, `Editor`, `Project`) | Unreal TargetType (`Game`, `Editor`, `Program`) / Godot `target=` | ⭐ State of the Art |
| **Asset Virtualization** | `/Engine/...` vs `/Game/...` virtual mount points | Unreal Package Paths (`/Engine/`, `/Game/`) / Godot `res://` | ⭐ Industry Standard |
| **RHI Backend Abstraction** | `IRenderDriver` interface + `FRenderDriverRegistry` | Unreal `FRHICommandList` / RHI backends | ⭐ Extensible |
| **Modular Driver Model** | Plugins in `Plugins/RHI/`, `Plugins/Physics/` | Unreal Engine Plugins (`.uplugin`) | ⭐ Modular |
| **Bake Tool Isolation** | Headless CLI `LeonAssetTool` (`.llightmap`, `.libl`) | Unreal *UnrealLightmass* / *Cooker* Out-of-Process | ⭐ Clean Runtime |
| **Linter & Policy Checks** | `verify_ue_naming.py` validating naming & boundaries | AAA Commit Hooks / Clang-Tidy CI | ⭐ High Reliability |
| **Automated Packaging** | `package_project.py` staging clean Shipping trees | Unreal *UAT* (Unreal Automation Tool) BuildCookRun | ⭐ Production Ready |
