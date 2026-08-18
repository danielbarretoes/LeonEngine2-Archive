# Changelog

All notable changes to the **LeonEngine2** game engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased] — 0.15.0

### Consolidation (Engine + LeonTournament)

#### Changed
- LeonTournament sources reorganized under `Public/Private` domains (`Types`, `UI`, `Game`, `Characters`, `Combat`); widgets split per class; arena spawn, damage rules, weapon VFX/presets extracted as `F*` helpers.
- EnTT POD components renamed to `F*Component` (`FStaticMeshComponent`, `F*LightComponent`, `FCameraComponent`) per `Docs/NAMING.md`.
- `FLoopbackNetDriver` renamed to `ULoopbackNetDriver` (subclasses `UNetDriver`); file names match the primary type.
- Shared `FStringUtils::Trim`; PBR fragment BRDF/shadow helpers in `PBR_Common.glsl` with OpenGL shader `#include` resolve.
- Split large translation units: `FWorldRenderer` (Lighting/Geometry/PostProcess), `FMapSerializer` deserialize unit, `UEngineTravel`.
- `Scripts/verify_ue_naming.py` enforces EnTT `F*Component` and forbids `class F* : public U*`.

#### Removed
- Legacy `Projects/MultiverseTournament` product tree (already absent from the active tree; product is LeonTournament only).

#### Fixed
- Anim Lab: opposing teams + friendly fire so dummy damage works; lab weapon pickups with 5s respawn; capsule foot plant; TAB scoreboard grouped by team.

#### Added
- Component overlap Begin/End events (`UPrimitiveComponent::OnComponentBeginOverlap` / `EndOverlap`) driven by `UWorld::UpdateComponentOverlaps`.
- `UGameplayStatics::ApplyPointDamage` / `ApplyRadialDamage` (authority-only) plus `AGameModeBase::NotifyActorDamaged` / `NotifyActorKilled`.
- `AActor::FindComponentByClass<T>()`.
- `APickup` / `ALaunchPad` bases and `ACharacter::LaunchCharacter`.
- Framed net RPCs (`ENetRPCKind`, `UNetConnection` IncomingRPC/OutgoingRPC, `AActor::CallServerRPC` / `CallClientRPC` / `CallMulticastRPC`) over loopback and IP demux; LeonTournament reload uses ServerRPC (fire-held stays on control bits).
- Net actor spawn/destroy in snapshots plus relevancy lite (`bReplicates`, `bAlwaysRelevant`, `NetCullDistanceSquared`); `AProjectile` replicates by default.
- `UCharacterMovementComponent::ResolvePenetration` (WorldStatic SAT MTD). SimulatedProxy pawns skip movement and physics write-back; Jolt `System::Update` runs on authority worlds only.

### Renderer math contract (CPU / GPU / baker)

#### Fixed
- GGX NDF no longer floors the microfacet denominator at `1e-7` (smooth metals were clipped to \(D \approx 25.6\) instead of \(1/(\pi r^4)\)).
- Atmospheric HDR equirect mapping matches the runtime sampler (\(v=0\) is +Y, \(u=0.5\) is +X).
- Lightmass cosine misses now store sky irradiance \(E = \pi L_{\mathrm{env}}\) (procedural atmosphere or HDR). Runtime still skips diffuse IBL when a lightmap is bound.
- Display encode after tone mapping uses IEC 61966-2-1 sRGB (not `pow(x, 1/2.2)`).
- Mesh import honors `bGenerateTangents` (Lengyel from UV0).
- GI bounce albedo samples the albedo map at UV0 instead of a whole-texture average.
- Directional/spot shadow tests use the rasterized face normal (not the normal map), tan(θ) slope bias, per-tap receiver-plane PCF, UV normal offset off the caster silhouette, `GL_NEAREST` comparison, and caster polygon offset.

#### Changed
- IBL disk cache is `.libl` **v6** (Karis cubemap `saTexel`). v5 files are ignored and rebuilt on first load.
- BRDF LUT disk header is `LEONBRDF` **v2** (24 bytes, stores sample count).
- Lightmass bake-input hash algorithm version is **4** (includes skybox / HDR). Existing `.llightmap` files must be rebaked.
- Missing lightmap UV1 is persisted to `.lmesh` **before** the bake-input hash, so validation is not stale after the first bake.

### Renderer correctness (canonical pipeline)

#### Added
- Canonical mesh vertex (`FCanonicalMeshVertex`, `.lmesh` v3): `Tangent.xyz` + handedness `w`, explicit shader locations 0–5.
- Shared color (`FColorSpace`), spotlight attenuation (`FLightAttenuation`), and numerical helpers (`FRenderingMath`).
- Renderer contract documentation (`Docs/RENDERER_CONTRACT.md`).
- Mathematical / GPU regression tests for TBN, winding, Lambert/IBL energy (`Lo = albedo/π * E`), sRGB 128/255, spotlight cone, shadow NDC, FBO resize, bounce count, receptor emissive, and analytic-plane irradiance.

#### Changed
- Diffuse IBL and lightmaps evaluate `Lo = albedo / PI * irradiance` (irradiance is the cosine-weighted integral, not outgoing radiance).
- IBL bake/cache (`.libl` v6) stores linear HDR; scene exposure is applied only in tone mapping.
- Lightmaps (`.llightmap` v2) store baked diffuse irradiance; GI is off when `NumIndirectBounces == 0`.
- Window resize rebuilds HDR, planar, and post-process targets.
- Transparent draws sort back-to-front with depth write off.
- Negative scale flips culling and tangent handedness.
- Spotlight shadows use configurable `SpotResolution` and an explicit single shadowed index.

#### Removed
- Contact-shadow shader/UI/tests (the feature was non-functional).
- Unused `PostProcess.glsl` path (tone mapping lives in `ToneMapping.glsl`).
- Dual bitangent vertex attributes and manual `pow(rgb, 2.2)` on hardware-sRGB textures.
- Obsolete `Engine/src` and `Engine/include` trees (not compiled; contradicted the live `Engine/Source/Runtime` pipeline).

#### Fixed
- Procedural sphere/plane/cylinder winding and TBN (`T × B ≈ N`).
- Baker albedo sRGB decode; baker/runtime spotlight Hermite attenuation.
- Lightmap UV generation no longer last-write-wins on shared vertices.
- Camera basis at pitch ±90°; pawn Euler matches camera look direction.

### Lightmass correctness & Stationary v1

#### Fixed
- Bake cache hash no longer includes stamped `.lmap` metadata (stable `ComputeBakeInputHash` from world + settings).
- Bake albedo reads material base color and samples albedo textures at UV0 on CPU (was constant 0.7).
- Generated lightmap UV1 is persisted back to `.lmesh` when missing.
- BRDF LUT disk test reads the `LEONBRDF` v2 24-byte header plus RG float payload.

#### Added
- `ELightMobility::Stationary`: indirect-only bake + dynamic direct/shadows at runtime (`IsLightmassBakeLight` / `DoesLightmassBakeDirect`).
- Environment knobs `AORadius`, `TexelPadding`, `WorldScale` (AO radius scaled by WorldScale).
- Robust `validate_lightmaps` (atlas presence, hash freshness, chart metadata). Sets project root before resolving `/Game` paths.

#### Changed
- Sandbox maps use Stationary sun/moon (and one Stationary spot) so runtime CSM/spot shadows coexist with baked irradiance lightmaps (`.llightmap` v2).
- Bake AO flags cleared (irradiance already includes ray visibility). `IndirectIntensity` is 1.0.

### Sandbox content (Showcase + Night)

#### Added
- CC0 1k PBR textures (studio floor, street, metal, brick, roof, bark, wood) and two Poly Haven 1k HDRs (`DaySky1k`, `NightSky1k`).
- NightLevel static meshes authored in Blender (`House`, `Car`, `PalmTree`, `StreetLamp`, `Ground`) with per-slot materials.
- NightLevel cottage (`Cottage_FREE.fbx`) with Dirt PBR atlas (`Dirt_Base_Color`, `Dirt_Roughness`, `Dirt_Metallic`).

#### Changed
- ShowcaseLevel uses only primitives on a larger studio-floor base and the day HDRI.
- NightLevel uses imported static meshes on an asphalt ground with the night HDRI.
- Sandbox HUD chip travels ShowcaseLevel ↔ NightLevel (was Night-only).
- `M_Car_Body` is painted metal (no tiling plate maps). The body is a long box whose smart-project UV0 stretched a 1:1 tile on the sides.

#### Added
- Showcase Movable glass sphere (transparency sort) and negative-scale cube (cull/TBN flip).
- `Projects/Sandbox/Scripts/bake.py` force-rebakes both maps.

#### Removed
- Unused Sandbox maps, leftover PNG textures, unused materials, `NightField1k` HDR, and Raw download scratch.

### Gameplay framework (Unreal-lite lifetime, identity, collision, listen-server)

#### Added
- Actor `Class` + `GUID` on `.lmap` v2.1 (old maps without those keys still load). Runtime-only framework actors are not serialized.
- `APlayerStart` and `AGameModeBase::ChoosePlayerStart` / `FindPlayerStart`; Login uses the placed start instead of a hardcoded location.
- AABB overlap/sweep on `UWorld` (`FBoxCollisionComponent` and static mesh bounds). `ACharacter` XZ movement is blocked by static geometry.
- `AWorldSettings` / `FWorldSettingsComponent` for Lightmass knobs (migrated off the skybox). Legacy Skybox bake keys copy into WorldSettings on load.
- Lighting quality presets (`Preview` / `Draft` / `Production`) with LeonAssetTool `--quality=` and `bake_lightmaps.py --quality`.
- Lean listen-server: `ENetMode` / `ENetRole`, `UNetDriver` snapshots, in-process `ULoopbackNetDriver`. Replicates GameState elapsed time, PlayerState, and possessed pawn transforms. Clients have no GameMode.
- `LeonEnginePipeline` static library (FBX/material import, Lightmass, lightmap UV). `LeonEngineCore` no longer links `ufbx`. Native `.ltex` / `.lhdr` loaders stay in Core so games (Sandbox) can load cooked textures without Pipeline.

#### Fixed
- `UWorld` ticks each actor once (GameState `ElapsedTime` no longer doubles). `Tick` is a no-op before `BeginPlay`; `EndPlay` clears `HasBegunPlay`.
- `DestroyActor` unbinds PlayerController / GameMode / GameState / possess / ViewTarget aliases and defers erase while ticking.
- `InitGame` does not spawn a second GameState. `TravelToMap` loads into a new world first and keeps the old world if the map is missing.
- Lightmass skip requires **both** atlas `ContentHash` and world `LightmapBakeHash` to match and be non-zero. `ValidateMap` fails on hash `0`.
- Bake hash includes material overrides and mesh slot `.lmat` paths. FBX import no longer copies UV0 onto UV1; unique UV1 is flagged only by `GenerateBoxPackedLightmapUVs`.
- Procedural meshes write unique 0–1 UV1 for lightmaps; the renderer samples UV1 instead of tiled UV0.
- `GetMaterialInstance(".lmat")` always creates a unique instance. `UnloadUnused` drops unused cache entries after travel.
- Runtime `.ltex` load (`FNativeTextureData`) lives in Core so OpenGL/Sandbox link without `LeonEnginePipeline`.

#### Changed
- Simple AO is applied in `FLightBaker` using existing AO radius/intensity settings.

### Engine Scripts (Unreal-like project contract)

#### Added
- `Scripts/_leon_paths.py` (`LEON_ENGINE_ROOT` / `LEON_PROJECT`); `bake_lightmaps.py`; `create_project.py` + BlankProject templates.
- `Docs/SCRIPTS.md`; Sandbox shortcuts under `Projects/Sandbox/Scripts/`.
- Root CMake out-of-tree `add_subdirectory` binary dir for external games.

#### Changed
- Engine Scripts require `--project` / `LEON_PROJECT` (no Sandbox defaults).
- Removed Engine `run_sandbox` / `validate_sandbox` / hardcoding PS1 wrappers.

### Static Lighting & Lightmap Baking

#### Added
- `ELightMobility` / `EComponentMobility`; lightmap UV1 on `.lmesh` v2; native `.llightmap` (`FLightmapAsset`).
- Offline `FLightmass` / `FLightBaker` / `FLightmapBuilder`; LeonAssetTool `bake_lightmaps` / `validate_lightmaps`.
- Runtime PBR lightmap sampling (slot 12); `.lmap` static lighting metadata + hash cache.
- Docs: `Docs/STATIC_LIGHTING.md`, `Docs/implementation_plan.md`; tests `Tests/Lightmass/StaticLightingTests.cpp`.

### Unreal Runtime layout & naming sweep

#### Added
- Project Boot Contract documented in `Docs/ARCHITECTURE.md` (`.lproject` + flat Multi-INI priority).
- `FProjectPaths::LocateProjectFile`, `UEngine::BuildGameModeConfig` / `ResolveStartupMap`, fullscreen window API, planar/shadow project defaults from INI.
- `Tests/Gameplay/ProjectBootTests.cpp` for boot config priority and Sandbox map resolve.
- Engine/project isolation: `build_project.py` / `run_project.py` / `validate_project.py` with `--project`; Engine has zero Sandbox path defaults.

#### Changed
- Headers/sources renamed to match primary types (`FWorldRenderer`, `UAssetManager`, `UStaticMesh`, `UGameplayStatics`, `FMapSerializer`, …).
- Includes use module prefixes (`#include "Engine/UWorld.hpp"`).
- Removed legacy `Engine/include` + `Engine/src` trees and `GetSceneRenderer` / `FSceneRenderer` API.
- OpenGL plugin files/types use `FOpenGL*` names; Sandbox gameplay files use `ASandbox*` / `USandboxMainMenuWidget`.
- Stripped remaining unprefixed `using` aliases; fixed type/member name collisions (`AppWindow`, `NativeWindow`, `LayerStack`, dispatcher `Event`).
- CI runs `Scripts/verify_ue_naming.py` before configure.
- Project boot: generic `/Script/<Project>.GameMode` INI, fatal missing default map, `LocateProjectFile` parent walk, Fullscreen + shadow/planar project defaults.

#### Removed
- One-shot migration helpers (`migrate_runtime_layout.py`, `rename_ue_members.py`, `finish_ue_naming.py`, `fix_member_type_collisions.py`) after the sweep completed.

#### Notes
- CMake splits `LeonEngineCore` (runtime, including native `.ltex`/`.lhdr` load) and `LeonEnginePipeline` (FBX/material import + Lightmass). Sandbox links Core only.
- Member `m_`/`s_` stripped to Unreal-style members (`bFlag`, `PascalCase`); collisions fixed (`CurrentAPI`, `CachedProjectDir`, `bIsLoaded`).

---

## [Unreleased] — 0.14.0

### Runtime shell (P0–P3)

Historical notes below (0.11–0.13) may mention earlier project layouts (`Coral`, `.llevel`, `FScene`). The **current** tree uses `Projects/Sandbox`, `.lmap`, and `UWorld` under `Engine/Source/Runtime/Engine/`.

#### Added
- `FInputSettings` loaded from `DefaultInput.ini` and consumed by pawns.
- Canvas anchors / margins (`FAnchors`, `FMargin`) with resize-safe Sandbox menu.
- Thin `ACharacter` (ground XZ movement); `ADefaultPawn` remains fly spectator.
- CPU frustum culling for static/procedural meshes + render stats.
- Lite `UActorComponent` lifecycle on `AActor`.
- `UImage` textured UI widget.
- `FWorldRenderer` (renamed from `FSceneRenderer`).
- `Scripts/validate_sandbox.py` and GitHub Actions validate workflow.

#### Changed
- Docs (`ARCHITECTURE`, `RENDERER_FEATURE_AUDIT`, README) aligned to `world/` + Sandbox.
- Post-process audit entries updated (Bloom + FXAA already shipped).
- **Planar reflections**: FBO corrected from LDR `RGBA8` → HDR `RGBA16F`; specular mix uses Fresnel only (no split-sum BRDF LUT re-weight), fixing white blob clamps on wet/mirror surfaces.
- **Planar reflections (follow-up)**: mip chain + `textureLod(roughness)` blur; Karis composition `mix(IBL, planarLi) * (F*scale+bias)` so wet-floor emissive stamps are BRDF-scaled instead of raw Li.
- **Planar reflections (materials)**: static-mesh reflection pass now resolves `MaterialOverrides` + textures (house/car/lamps were drawn untextured).

---

## [0.13.0] - 2026-08-15

### Project Runtime, Coral Preview & HDR Asset Pipeline
This milestone expands the native asset pipeline with full HDR environment asset compilation, introduces generic developer launchers and automated smoke testing, and enforces 100% decoupling between LeonEngine2 and project content.

#### Added & Improved
- **Native HDR Asset Pipeline (`.lhdr` & `FHDRImporter`)**:
  - Introduced native binary `.lhdr` container format with packed metadata (`FLHDRHeader`), 32-bit floating point pixel payloads (`FNativeHDRData`), and equirectangular projection descriptors.
  - Implemented `FHDRImporter` (`Engine/include/asset/HDRImporter.hpp`, `Engine/src/asset/HDRImporter.cpp`) for offline Radiance `.hdr` conversion, saving compiled assets to `<Project>/Content/Assets/HDR/<Name>.lhdr`.
  - Added native `.lhdr` support to `FOpenGLTexture2D` and `FAssetManager::GetHDRTexture(...)`.
  - Updated `FIBLGenerator` to consume `.lhdr` assets and save `.libl` pre-baked IBL caches directly in project asset cache directories (`<Project>/Content/Assets/Cache/IBL/`).
- **Structured Sandbox Runtime Diagnostics & First-Frame Notification**:
  - Implemented structured startup diagnostics output (`[Project]`, `[Level]`, `[Renderer]`).
  - Added first-frame completion signal `[Sandbox] READY` to stdout for automated testing frameworks.
  - Removed all hardcoded fallbacks in `SandboxApp.cpp`, using dynamic `Projects/` scanning.
- **Developer Launchers & Automated Smoke Testing**:
  - **`Scripts/run_project.py`**: Convenient CLI launcher for any project (`python Scripts/run_project.py --project Projects/Coral [--level <name>]`).
  - **`Scripts/smoke_test_project.py`**: Automated end-to-end integration tester verifying project structure, level references, and actual process startup to `[Sandbox] READY`.
  - **`LeonAssetTool run`**: Subcommand `LeonAssetTool run --project <path> [--level <name>]`.
  - Updated `LeonAssetTool` with `inspect <file.lhdr>` and automated HDR import in `import --project <path>`.
- **Coral Project HDR Environment & Showcase Level**:
  - Generated calibrated oceanic atmosphere raw HDR `Projects/Coral/Content/Assets/Raw/Environment/OceanSky.hdr`.
  - Compiled and integrated native `Projects/Coral/Content/Assets/HDR/OceanSky.lhdr` into `CoralShowcase.llevel`.
- **Automated Regression Test Suite (`HDRAssetTests.cpp`)**:
  - Added 5 new test cases covering atmospheric synthesis, `.lhdr` save/load roundtrips, raw `.hdr` import, and corrupt payload rejection. Total test count reached 100 passing suites (8,102,660 assertions).

## [0.12.0] - 2026-08-15

### Unreal-Style Project, Level & Scene Architecture Refactoring
This milestone introduces a formal Unreal Engine-inspired architectural separation in LeonEngine2:
- Decouples persistent on-disk world assets (**Level**, `.llevel`, `FLevelSerializer`) from in-memory runtime world states (**Scene**, `FScene`).
- Introduces the **Project Subsystem** (`FProject`, `<ProjectName>.project`), allowing games and demo projects to be completely decoupled from the engine runtime.
- Transforms **Sandbox** into a project-agnostic previewer (`Sandbox.exe --project <Path> [--level <Name>]`).
- Eliminates all hardcoded project paths and project-specific conditionals across the engine core, establishing unified virtual path resolution.

#### Added & Improved
- **Project Subsystem (`Engine/include/project/Project.hpp`, `Engine/src/project/Project.cpp`)**:
  - `FProject` container and `FProjectConfig` parsing `<ProjectName>.project` descriptor files.
  - Deterministic virtual asset path (`/Assets/...` -> `<Project>/Content/Assets/...`) and level path resolution.
  - Active project management (`FProject::SetActive`) with automatic `FAssetManager` content root synchronization.
- **Level Serialization & Standardized Naming (`LevelSerializer.hpp`, `LevelSerializer.cpp`)**:
  - Renamed and refactored `FSceneSerializer` to `FLevelSerializer`.
  - Standardized level asset files to `.llevel` extension (deprecated `.lscene`).
  - Added backward compatibility aliases (`using FSceneSerializer = FLevelSerializer`).
- **Complete Decoupling of Projects & Generic Sandbox Previewer (`SandboxApp.cpp`, `AssetManager.cpp`)**:
  - Removed all hardcoded `"Projects/Coral/Content/Assets"` and `if (startupLevel.find("Coral") != ...)` logic.
  - Added command-line options: `--project <path>` and optional `--level <name>`.
- **Project Descriptors & Test Projects**:
  - Created `Projects/Coral/Coral.project` and updated level to `Projects/Coral/Content/Levels/CoralShowcase.llevel`.
  - Created `Projects/Minimal/Minimal.project` and `Projects/Minimal/Content/Levels/Empty.llevel`.
- **Tooling & Validation CLI (`LeonAssetTool`, `validate_project.py`, `validate_level.py`)**:
  - Added `LeonAssetTool validate_project --project <path>` (or `project`).
  - Added `LeonAssetTool validate_level --level <path> [--project <path>]` (or `level`).
  - Added validation checks for zero absolute Windows/Unix paths in level assets.
- **Architecture Documentation & Automated Tests**:
  - Created `Docs/PROJECT_LEVEL_ARCHITECTURE.md`.
  - Added `Tests/Project/ProjectTests.cpp` and updated `Tests/Scene/CoralShowcaseSceneTests.cpp`.

## [0.11.0] - 2026-08-15

### Coral Asset Showcase & Dynamic Lighting Validation
This milestone introduces a playable, visual showcase level for LeonEngine2 featuring real native assets imported from the Coral project. It validates the end-to-end rendering pipeline, ECS scene architecture, multi-submesh material slot overrides, and all three dynamic light types (Directional CSM, Point, Spot) with distinctive visual response.

#### Added & Improved
- **Coral Showcase Level Asset (`Projects/Coral/Content/Levels/CoralShowcase.lscene`)**:
  - Full oceanic underwater environment with calibrated atmospheric skybox lighting.
  - Multi-tiered static mesh composition including seabed formations (`CoralRocks.lmesh`), hero coral clusters (`Corals.lmesh`), deep reef boundary walls (`CoralGroups.lmesh`, 1.05M vertices, 44 submeshes), and ambient seaweed foliage (`Seaweeds.lmesh`).
  - Material instance variations: `MI_Coral_Warm.lmi` (amber tone, 0.65 roughness), `MI_Rock_Wet.lmi` (glossy wet rock, 0.18 roughness), `MI_Coral_Blue.lmi` (deep azure, 0.35 roughness).
- **Scene Serialization & Dynamic Components (`SceneSerializer.cpp`, `Components.hpp`)**:
  - Added native static mesh serialization (`StaticMesh: Asset: ...`, `CastShadows`, `ReceiveShadows`, `VisibleInReflection`).
  - Added per-slot material overrides parsing and serialization (`MaterialOverrides: - Slot: X, Asset: ...`).
  - Added primary perspective camera serialization (`Camera: Primary: true, FOV: 45.0, NearPlane: 0.1, FarPlane: 1000.0`).
- **Dynamic Multi-Light Lighting Rig**:
  - **Directional Sunlight (Surface Penetration)**: Downward penetrating primary sunlight with 4-split Cascaded Shadow Maps (CSM).
  - **Underwater Ambient Point Light**: Broad cyan/blue ambient bounce fill ($r = 22.0$).
  - **Warm Bioluminescent Point Light**: Localized amber glow accent ($r = 14.0$).
  - **Specular Accent Spotlight**: Greenish high-intensity raking light ($r = 20.0$, cone $14^\circ/28^\circ$) casting sharp spot shadows.
- **Engine Runtime & Sandbox Application CLI Support (`SandboxApp.cpp`, `Application.hpp`, `EntryPoint.hpp`)**:
  - Added `FApplicationCommandLineArgs` supporting level launching via CLI (`Sandbox.exe <PathToLevel.lscene>`).
  - Automatic `FAssetManager` content root resolution (`Projects/Coral/Content/Assets`).
  - Automatic camera viewport positioning from the scene's primary `FCameraComponent`.
- **CLI Subcommands & Level Validation (`LeonAssetTool`, `validate_level.py`, `create_coral_showcase.py`)**:
  - Added `LeonAssetTool showcase --project <Path>` generator subcommand.
  - Added `LeonAssetTool validate_level --level <Path>` validation subcommand ensuring zero broken links, mesh existence, light counts, and camera configurations.
  - Added Python wrapper `Scripts/validate_level.py` and level generation script `Scripts/create_coral_showcase.py`.
- **Automated Regression Test Suite (`CoralShowcaseSceneTests.cpp`)**:
  - Added doctest suite verifying scene deserialization, light counts, mesh bindings, submesh counts, and serialization roundtrip.

## [0.10.0] - 2026-08-15

### Native Asset Import Pipeline (Meshes, Textures, Materials, CLI Toolchain & Incremental Manifest)
This milestone delivers a complete, high-performance native asset import pipeline for LeonEngine2. It decouples external authoring formats (FBX, PNG, TGA, JPG) from runtime rendering formats (`.lmesh`, `.ltex`, `.lmat`, `.lmi`), eliminating runtime decoding overhead and enabling instantaneous scene startup.

#### Added & Improved
- **Native Texture Pipeline (`.ltex`, `TextureImporter.hpp`, `TextureImporter.cpp`)**:
  - 64-byte packed header with UUID, dimensions, channel count, color space (sRGB/Linear), and semantic metadata.
  - Software box-filtered mipmap generation down to $1\times 1$.
  - Semantic auto-detection (`diff`, `norm`, `gloss`, `rough`, `metal`, `illum`, `ao`).
  - Offline Gloss-to-Roughness inversion ($R = 255 - G$) for legacy roughness workflows.
  - Direct OpenGL GPU texture memory allocation and mip upload without runtime CPU decompression.
- **Native Static Mesh Pipeline (`.lmesh`, `StaticMesh.hpp`, `StaticMesh.cpp`, `MeshImporter.hpp`, `MeshImporter.cpp`)**:
  - Integrated `ufbx` for robust, high-performance FBX parsing and coordinate conversion (right-handed, Y-up).
  - 68-byte packed vertex stride (`Float3 Pos`, `Float3 Normal`, `Float2 UV`, `Float3 Tangent`, `Float3 Bitangent`, `Float3 Color`).
  - Multi-submesh preservation with per-submesh indices, local transforms, and material slot assignments.
  - Precalculated bounding boxes ($\mathbf{Min}, \mathbf{Max}$) and bounding spheres ($\mathbf{Center}, Radius$).
- **Material Extraction & Instances (`.lmat`, `.lmi`, `MaterialImporter.hpp`, `MaterialImporter.cpp`)**:
  - Fuzzy token matching for automatic material texture map binding.
  - Extraction and serialization of master materials (`.lmat`) and material instances (`.lmi`).
- **Asset Manifest & Incremental Import (`AssetManifest.hpp`, `AssetManifest.cpp`)**:
  - 64-bit FNV-1a content hashing for change detection.
  - Dependency graph tracking and JSON persistence (`manifest.json`).
  - Instantaneous skip of unmodified assets during subsequent import runs.
- **Engine & ECS Integration (`Components.hpp`, `AssetManager.hpp`, `SceneRenderer.cpp`)**:
  - Added `FStaticMeshComponent` with material overrides and shadow/reflection flags.
  - Extended `FAssetManager` with `GetStaticMesh`, `GetMaterialInstance`, and template `Load<T>`.
  - Added submesh-indexed rendering (`FRenderCommand::DrawIndexedOffset`) across Directional CSM, Spot Shadow, Planar Reflection, and Geometry passes.
- **CLI Tools & Python Wrappers**:
  - Added `Tools/LeonAssetTool/` CLI executable supporting `import`, `validate`, and `inspect` commands.
  - Added `Scripts/import_assets.py` and `Scripts/validate_assets.py`.
- **Coral Project Integration & Test Coverage**:
  - Successfully imported all raw Coral assets in `Projects/Coral/Content/Assets/` (39 native assets: 4 static meshes including 44-submesh `CoralGroups`, 23 textures up to 4K, 12 materials/instances).
  - Added 5 new automated test suites (`AssetIDTests`, `TextureImportTests`, `MeshImportTests`, `AssetValidationTests`, `CoralPipelineTests`).
  - Total test suite expanded to **87 test cases** and **8,102,532 assertions (100% passing)**.

---

## [0.9.0] - 2026-08-15

### Advanced Shadow System (Cascaded Shadow Maps, Practical Split Scheme, Stabilized Projection, Normal Offset Bias, Multi-Filtering & Contact Shadows)
This milestone delivers a complete, production-grade shadow rendering architecture for LeonEngine2. It introduces 4-cascade shadow mapping with analytical Practical Split Scheme ($\lambda = 0.85$), bounding sphere enclosure with world-space texel snapping to eliminate camera rotation shimmering, multi-term depth bias (constant, slope-scale, normal offset) eliminating shadow acne and Peter Panning, multiple filtering algorithms (Hard, PCF 3x3, PCF 5x5, 16-tap Poisson disk with interleaved gradient noise jitter), smooth cascade blending across boundaries, far shadow distance fadeout, alpha-masked shadow casters, screen-space ray-marched contact shadows, and 8 forensic shadow diagnostic modes.

#### Added & Improved
- **Shadow Mathematical Foundations & Stabilization (`ShadowMath.hpp`, `ShadowMath.cpp`)**:
  - Implemented analytical Practical Split Scheme ($z_i = \lambda z_{\text{log}} + (1-\lambda) z_{\text{lin}}$, $\lambda = 0.85$).
  - Implemented 8-corner frustum extraction in world space from inverse view-projection.
  - Implemented bounding sphere projection enclosure and sub-texel snapping to world-space texel grid ($\Delta x = 2R/\text{Res}$) completely eliminating camera rotation shimmering.
- **Resource Architecture & Framebuffer Expansion (`ShadowTypes.hpp`, `SceneRenderer.cpp`)**:
  - Created enums `EShadowFilterMode`, `ECascadeSplitScheme`, and structs `FShadowSettings`, `FShadowCascade`.
  - Expanded Directional Light CSM allocation from 3-viewport atlas to a 4-layer 2D Texture Array (`DEPTH32F_ARRAY_SHADOW` $2048 \times 2048 \times 4$).
  - Preserved dedicated Spot Light shadow map ($1024 \times 1024$ `DEPTH32F_SHADOW`).
- **Shader Pipeline (`PBR_Lit.glsl`, `ShadowDepth.glsl`)**:
  - Standardized `CameraData` UBO Binding 0 across vertex and fragment stages (544 bytes std140).
  - Implemented Normal Offset Bias ($\mathbf{p}' = \mathbf{p} + \mathbf{N} \cdot \text{normalBias} \cdot \text{slopeFactor}$) and dynamic slope-scale depth bias.
  - Implemented multi-mode filter switch: `Hard` (1 tap), `PCF 3x3` (9 taps), `PCF 5x5` (25 taps), and `Poisson Disk` (16 taps Vogel spiral with per-pixel Interleaved Gradient Noise jitter rotation).
  - Implemented smooth cascade boundary blending (`u_ShadowParams.w`) and far distance soft fadeout.
  - Implemented screen-space ray-marched contact shadows for micro-geometry contact occlusion.
  - Extended `ShadowDepth.glsl` with UV passing and fragment discard for alpha-masked caster materials (`u_AlphaMode == 1`).
- **Forensic Shadow Diagnostic Views & Interactive Controls (`SandboxApp.cpp`)**:
  - Added `F11` key handler cycling through 8 shadow diagnostic modes (Composite, Direct Shadow Factor, Cascade Slice False-Color [0:Red, 1:Green, 2:Blue, 3:Yellow], Contact Shadows, Cascade 0..3 Depth Maps).
  - Added `F12` key handler cycling through shadow filter modes (`Hard` $\to$ `PCF 3x3` $\to$ `PCF 5x5` $\to$ `Poisson Disk`).
- **Comprehensive Headless GPU Testing & Mutation Suite Expansion**:
  - Added 6 new GPU shadow test suites: `ShadowCascadeTests`, `ShadowPCFTests`, `ShadowBiasTests`, `ShadowAtlasTests`, `ShadowSelectionTests`, and `ShadowContactTests`.
  - Regression suite expanded to **68 test cases** and **8,102,406 assertions (100% passing)**.
  - Expanded GLSL shader mutation test suite to **56 mutations with 100.0% detection rate (56/56 caught)**.
  - Maintained C++ mutation audit at **13/15 (86.7%)**.

---

## [0.8.0] - 2026-08-15

### Advanced Materials & Surface Detail Pipeline (Normal Mapping, PBR Workflow, Emissive Decoupling, Alpha Modes, UV Transform, Material Debug Views & 100% Shader Mutation Coverage)
This milestone elevates the material subsystem of LeonEngine2 to a production-grade PBR surface detail architecture. It introduces robust tangent space reconstruction with Gram-Schmidt orthogonalization, full texture mapping across all physical PBR channels with strict sRGB-to-Linear color management, independent HDR emissive radiance feeding the Bloom pipeline, alpha testing/blending modes, UV coordinate transformations, and 11 forensic material debug views.

#### Added & Improved
- **Extended Material Data Model (`FMaterial`, `FMaterialInstance`, `FMaterialSerializer`)**:
  - Added `NormalScale`, `OcclusionStrength`, `AlphaCutoff`, `AlphaMode` (`Opaque`, `Mask`, `Blend`), `DoubleSided`, `UVTiling`, and `UVOffset`.
  - Implemented sparse override hierarchy in `FMaterialInstance` with fallback resolution to parent template.
  - Extended `.lmat` text serializer and deserializer with backward-compatible format support.
- **Normal Mapping & Tangent Space Orthogonalization (`PBR_Lit.glsl`)**:
  - Implemented authoritative Gram-Schmidt orthogonalization ($T, B, N$) in fragment shading to eliminate normal interpolation skew across curved geometry.
  - Added `u_NormalScale` bump perturbation scaling and strict fallback to geometric normals when disabled.
- **PBR Texture Workflow & Color Spaces**:
  - Strict sRGB-to-Linear decompression (`pow(..., vec3(2.2))`) for color channels (Albedo, Emissive).
  - Raw linear channel retention for physical data maps (Metallic, Roughness, Ambient Occlusion).
  - Standardized 12-unit texture slot allocation table without sampler conflicts.
  - Deterministic scalar multipliers and fallbacks when textures are unbound.
- **Emissive Radiance Decoupling**:
  - Physically independent emissive radiance accumulation ($Lo += \text{emissive}$), bypassing $N \cdot L$, directional sunlight, and shadow maps.
  - Full HDR intensity support ($> 1.0$) directly feeding the Dual-Kawase Bloom pipeline.
- **Alpha Modes & Dynamic Pipeline State (`SceneRenderer.cpp`)**:
  - Support for `Opaque`, `Mask` (alpha cutoff with fragment discard), and `Blend` (translucent blend states).
  - Automatic double-sided back-face culling bypass for foliage, glass, and thin surfaces.
- **Texture Coordinate Transformations**:
  - Uniform `u_UVTiling` scaling and `u_UVOffset` translation applied across all material samplers.
- **Forensic Material Debug Views & Hotkey (`SandboxApp.cpp`)**:
  - Added `F10` key handler to cycle through 11 Material Debug Modes (Full Composite, Base Color, Metallic, Roughness, Normal, AO, Emissive, Tangent $T$, Bitangent $B$, UV coordinates, and Direct $N \cdot L$).
- **Headless GPU Testing & Mutation Suite Expansion**:
  - Added 7 new headless GPU test suites: `PBRShaderNormalMappingTests`, `PBRShaderMaterialTextureTests`, `PBRShaderEmissiveTests`, `PBRShaderAlphaTests`, `PBRShaderUVTransformTests`, `PBRShaderColorSpaceTests`, and `PBRShaderTangentSpaceTests`.
  - Regression test suite expanded to **57 test cases** and **8,102,320 assertions (100% passing)**.
  - Expanded GLSL mutation testing to 40 mutations across all shaders with **100.0% detection rate (40/40 caught)**.
  - Preserved C++ mutation testing audit at 13/15 (86.7%).

---

## [0.7.0] - 2026-08-15

### Post-Processing & Anti-Aliasing Pipeline Stack (Dual-Kawase Bloom, ACES Tone Mapping, FXAA 3.11, Multi-Debug Views & 100% GPU Mutation Coverage)
This milestone introduces a professional, physically accurate, and headless-tested post-processing architecture to LeonEngine2. It seamlessly bridges HDR linear radiance output from the PBR/IBL pass into sRGB display space through a multi-pass pipeline featuring a Dual-Kawase/Jimenez Bloom pyramid, customizable Tone Mapping operators with gamma 2.2 correction, and FXAA 3.11 subpixel edge anti-aliasing.

#### Added & Improved
- **Post-Processing Pipeline (`FPostProcessPipeline` & `FPostProcessSettings`)**:
  - Modular, multi-pass GPU pipeline orchestrated via `FRenderCommand` and Direct State Access (DSA).
  - Dynamic viewport resizing (`OnViewportResize`) with automatic framebuffer and mip chain reallocation.
  - Interactive runtime control over Exposure, Bloom Threshold/Soft Knee/Intensity, and FXAA toggles.
- **Dual-Kawase / Jimenez Bloom Pyramid (`BloomBrightPass.glsl`, `BloomDownsample.glsl`, `BloomUpsample.glsl`)**:
  - **Soft-Knee Bright Pass Extraction**: Continuous quadratic threshold curve eliminating harsh cutoff boundaries.
  - **13-Tap Downsampling**: Jorge Jimenez filter kernel with Karis luma weighting on Mip 0 to prevent subpixel fireflies and energy-conserving box weights strictly summing to $1.0$.
  - **9-Tap Tent Upsampling**: Progressive additive blur expansion with configurable filter radius.
- **Tone Mapping & Color Grading (`ToneMapping.glsl`)**:
  - **ACES Filmic (Narkowicz Fit)**: Industry-standard photographic S-curve tone mapper.
  - **Multi-Operator Support**: Extended Reinhard, Neutral clamp, and Uncharted 2 operators.
  - **Gamma 2.2 Correction**: Accurate linear-to-sRGB display space mapping.
  - **Luma Alpha Encoding**: Packs perceptual Rec. 601 luma in the Alpha channel for single-pass FXAA consumption.
- **FXAA 3.11 Anti-Aliasing (`FXAA.glsl`)**:
  - Complete Timothy Lottes FXAA 3.11 Quality implementation.
  - Contrast threshold early-exit, horizontal vs. vertical edge detection, tangent endpoint search (up to 12 quality iterations), and subpixel blending.
- **Interactive Post-Process Debug System & Hotkeys (`SandboxApp.cpp`)**:
  - `F3`: Master toggle for post-processing pipeline.
  - `F4`: Toggle FXAA anti-aliasing.
  - `F5`: Cycle post-processing debug views (0: Full Composite, 1: Raw HDR Radiance, 2: Bloom Glow Only, 3: Bright Pass Extract, 4: Tone Map Only without Bloom).
  - `F6` / `F7`: Decrease / Increase camera exposure ($\pm 0.1$).
  - `F8` / `F9`: Decrease / Increase bloom glow intensity ($\pm 0.01$).
- **Headless GPU Testing & Shader Mutation Suite Expansion**:
  - Expanded test suite to **47 test cases** and **8,102,248 assertions** (100% passing).
  - Added dedicated GPU tests for Bloom quadratic thresholding, Jimenez/Tent energy conservation, ACES numerical curve monotonicity, FXAA contrast threshold invariants, and full pipeline resize lifetimes.
  - Expanded automated GLSL mutation testing to 24 mutations across `PBR_Lit.glsl`, `BloomBrightPass.glsl`, `BloomDownsample.glsl`, `BloomUpsample.glsl`, `ToneMapping.glsl`, and `FXAA.glsl` with **100.0% detection rate (24/24 caught)**.

---

## [0.6.0] - 2026-08-15

### Major Rendering Pipeline Consolidation, Mathematical QA & GPU Headless Testing Suite
This release consolidates the entire physical rendering pipeline of LeonEngine2 into a robust, deterministic, and physically correct architecture with full Direct State Access (DSA), instantaneous binary IBL caching, multi-pass shadows, planar reflections, and zero visual artifacts. Furthermore, it introduces a comprehensive **38-case regression testing framework** with **headless OpenGL 4.5 Core GPU shader execution** and **automated GLSL mutation testing**.

#### Added & Improved
- **Headless GPU Testing Infrastructure (`FHeadlessGLContext` & Doctest)**:
  - Invisible OpenGL 4.5 Core offscreen testing framework via GLFW and GLAD.
  - Pixel readback validation in 32-bit floating point FBOs (`GL_RGBA32F` / `GL_RGBA16F`).
  - Unit Quad mesh, dynamic std140 UBO management (`CameraData` and `LightingData`), and multi-unit texture slot fallbacks.
  - Direct hardware validation of `PBR_Lit.glsl` Cook-Torrance direct lighting, Fresnel-Schlick angle response, UE4 Point Light inverse-square falloff, Spot Light conical cutoffs, IBL cubemap sampling, cascaded shadow toggles, and planar reflections.
- **Automated GLSL Mutation Testing (`run_shader_mutations.py`)**:
  - 15 deliberate physical mutations introduced into `PBR_Lit.glsl` with **100.0% detection rate (15/15 caught)**.
- **Automated C++ Math Mutation Audit (`run_mutation_audit.py`)**:
  - 15 deliberate mutations on IBL/PBR mathematical functions with an **86.7% detection rate (13/15 caught)**.
- **Image-Based Lighting (IBL) Architecture V4 (`IBLGenerator.cpp` & `.libl`)**:
  - **Diffuse Irradiance Convolution**: Upgraded to Quasi-Monte Carlo Hammersley cosine-weighted hemisphere sampling ($N=512$) with source-HDR solid angle Mip-filtering ($\text{lod} = 5.50$), mathematically eliminating delta-sun discretization variance and discrete square artifacts.
  - **Specular Prefiltered Environment Cubemap**: Full 5-level mip chain ($128 \to 8$) using importance-sampled GGX with Karis source-texel solid angle filtering and $+1.0$ mip bias, eliminating specular fireflies on high-contrast metal surfaces.
  - **2D BRDF Look-Up Table Asset (`BRDF_LUT.bin`)**: Pre-baked $256 \times 256$ `RG16F` Cook-Torrance Split-Sum LUT loaded from disk in **$1.4\text{ ms}$**.
  - **High-Speed Binary Cache (`.libl` v4)**: Automated FNV-1a 64-bit source hashing and header versioning, reducing IBL initialization time from $15.5\text{ s}$ to **$11.5\text{ ms}$**.
  - **Cubemap Sampling**: Enabled `GL_TEXTURE_CUBE_MAP_SEAMLESS` across all cubemap sampling passes.
- **Dynamic Shadows & Real-Time Reflections (`SceneRenderer.cpp`)**:
  - **Cascaded Shadow Maps (CSM)**: 4 depth cascades rendered to `GL_TEXTURE_2D_ARRAY` ($2048 \times 2048$) with view-space $Z$ cascade selection and 16-tap Poisson disk PCF filtering.
  - **Spot Light Shadows**: Dedicated $1024 \times 1024$ depth render target with slope-scaled normal bias and soft penumbra.
  - **Planar Reflections**: Mirrored camera pass rendering to offscreen HDR FBO ($1280 \times 720$) with normal-perturbed screen-space UVs and Fresnel attenuation.
- **First-Class Material System & Asset Hierarchy**:
  - Modular `FMaterial`, `FMaterialInstance`, `FPipelineState`, and `FMaterialComponent`.
  - Human-readable declarative `.lmat` YAML material asset format.
  - `FSceneSerializer` for declarative level deserialization from `.llevel` files.
- **RHI OpenGL 4.5 Core Architecture & Direct State Access (DSA)**:
  - Full DSA implementation (`glCreateBuffers`, `glNamedBufferData`, `glCreateTextures`, `glTextureStorage2D`, `glCreateFramebuffers`, `glBindTextureUnit`).
  - CPU State Cache in `FOpenGLRenderAPI` (tracking Viewport, DepthTest, DepthMask, DepthFunc, CullFace, BlendState, and FBO bindings) preventing redundant driver state switches.
  - Dedicated Uniform Buffer Objects (UBO): Camera UBO (Binding 0, 432 bytes) and Lighting UBO (Binding 1, 1088 bytes).
- **Diagnostics, HUD & Interactive Debug System**:
  - **Performance Overlay (`F1`)**: Real-time HUD displaying FPS, CPU/GPU Frametime, VRAM Allocation, Draw Calls, and Triangle Counts.
  - **3D Light Gizmos (`F2`)**: Cones for spot lights, radius spheres for point lights, and sun direction vectors.
  - **3D In-World Typography (`FTextRenderer`)**: Dynamic batching with high-resolution Inter-Bold TrueType font atlas.
  - **Interactive Material & Lighting Debug Hotkeys (`Shift + F1..F12`, `Shift + N`, `Shift + R`)**: Real-time isolation of Environment Cubemap, Prefilter Mips 0..4, Diffuse Irradiance, BRDF LUT, Specular IBL Only, Direct Light Only, Planar Reflections, World Normals, and Reflection Vectors.
- **Geometry & Culling**:
  - Corrected CCW winding order and geometric vertex layout across Cube, Sphere, Cylinder (walls + top/bottom caps), Plane, Ramp, and Pyramid primitives.
- **Performance & Startup Time**:
  - Total startup time from window creation to first 3D frame under **$\approx 70\text{ ms}$**.

---

## [0.5.0] - 2026-08-14

### Added
- **Equirectangular 360° HDR Environment Texture Pipeline (`AutumnField1k.hdr`)**:
  - Added native 32-bit floating-point HDR image loading (`stbi_loadf`, `GL_RGB16F`, `GL_FLOAT`) in `FOpenGLTexture2D`.
  - Spherical UV sampling mapping (`SampleSphericalMap`) in `Skybox.glsl` and `PBR_Lit.glsl`.
  - Image-Based Lighting (IBL) with automatic mip-level sampling (`textureLod`) corresponding to material roughness for prefiltered specular reflections and blurred hemisphere diffuse irradiance.
- **Ambient Occlusion (AO) Pipeline (`FPBRMaterialComponent` & `PBR_Lit.glsl`)**:
  - Per-material scalar `AO` (0.0 to 1.0) and 2D texture `AOMap` support in `FPBRMaterial` and `FPBRMaterialComponent`.
  - Ambient occlusion modulation on indirect ambient lighting in `PBR_Lit.glsl` to add realistic contact shadows and depth in crevices.
- **Atmospheric HDR Skybox & Image-Based Lighting (IBL) Pipeline (`Skybox.glsl` & `FSkyboxComponent`)**:
  - Infinite depth Skybox rendering pass (`GL_LEQUAL`, `glDepthMask(GL_FALSE)`) on cube geometry with stripped camera translation.
  - Physical atmospheric scattering model with Rayleigh sky-to-horizon gradients, solar disc, and Mie scattering halo.
  - **Indirect Diffuse (Hemisphere Irradiance)** in `PBR_Lit.glsl` illuminating shadow regions naturally with sky/ground color bounce.
  - **Indirect Specular (Roughness-Filtered Environment Reflection)** providing metallic reflections (gold, iron, chrome) that reflect the sky and horizon without darkening unlit angles.
  - **Fresnel-Schlick with Roughness** attenuation on glancing angles.
  - Industry-standard **ACES Film Tonemapping** (Unreal Engine 5 curve) and gamma 2.2 color correction for rich contrast, deep blacks, and vibrant specular highlights.
  - ECS `FSkyboxComponent` for declarative sky, sun, horizon, ground colors, exposure, and intensity control.

- **Dynamic Shadow Mapping Subsystem (`ShadowDepth.glsl` & `FScene`)**:
  - Dedicated 2048x2048 high-resolution Depth Framebuffer in `FScene` using `DEPTH24STENCIL8` with border clamping (`GL_CLAMP_TO_BORDER`).
  - Orthographic Light Space Matrix generation for Directional sunlight (`u_LightSpaceMatrix`).
  - Shadow Depth Pre-pass with front-face culling (`GL_FRONT`) eliminating Peter Panning and self-shadow artifacts.
  - Multi-stage `ShadowDepth.glsl` shader for ultra-fast depth capture.
  - Anti-aliased 3x3 Percentage-Closer Filtering (PCF) with slope-scale bias in both `DefaultLit.glsl` and `PBR_Lit.glsl`.
- **Cook-Torrance PBR Material Pipeline (`PBR_Lit.glsl` & `FPBRMaterialComponent`)**:
  - Complete physically based rendering shader adhering to modern game engine standards:
    - **NDF**: Trowbridge-Reitz GGX distribution.
    - **Geometry**: Smith's Schlick-GGX attenuation.
    - **Fresnel**: Fresnel-Schlick with dielectric base reflectance ($F_0 = 0.04$) and metallic color interpolation.
  - Metallic, Roughness, Albedo, and Tangent-Space Normal Mapping support.
  - ECS `FPBRMaterial` and `FPBRMaterialComponent` enabling per-entity PBR material setup.
- **Mesh Primitives Tangent Space Generation (`FMeshPrimitives`)**:
  - Upgraded Cube, Quad, Plane, Cylinder, and Sphere generators with Tangents (`aTangent`) and Bitangents (`aBitangent`) (17 floats per vertex layout).
- **RHI Framebuffer Texture Binding Extensions**:
  - Added `GetDepthAttachmentRendererID()`, `BindDepthTexture(slot)`, and `BindTexture(index, slot)` to `FFramebuffer` and `FOpenGLFramebuffer`.
- **Upgraded Sandbox App**:
  - Live side-by-side PBR showcase with polished gold, brushed iron, matte dielectric, textured metallic objects, and ground receiving dynamic shadows under a luminous HDR sky.


---

## [0.4.0] - 2026-08-14


### Added
- **RHI Framebuffer & Offscreen Render Target Subsystem (`FFramebuffer`)**:
  - Abstract `FFramebuffer` interface with `EFramebufferTextureFormat` (`RGBA8`, `RED_INTEGER`, `DEPTH24STENCIL8`).
  - `FFramebufferSpecification` and `FFramebufferAttachmentSpecification` supporting multi-target attachments and sample counts.
  - Offscreen color and depth rendering, `Resize(width, height)`, `ReadPixel`, `ClearAttachment`, and swapchain presentation (`BlitToDefault`).
  - Integrated `CreateFramebuffer` factory in `IRenderDriver` and `FRenderDriverRegistry`.
- **OpenGL Framebuffer Backend Plugin (`FOpenGLFramebuffer`)**:
  - Vendor-isolated implementation in `Plugins/RHI/OpenGL/` managing multi-color textures and depth attachments.
  - Framebuffer validation via `glCheckFramebufferStatus` and multi-target drawing via `glDrawBuffers`.
  - Real-time VRAM allocation tracking via `FRenderer::OnGPUAlloc` and `FRenderer::OnGPUFree`.
- **Scene & Entity Component System (`FScene`, `FEntity`, EnTT)**:
  - Integrated header-only **EnTT (v3.14.0)** for fast, cache-coherent ECS entity querying.
  - Ergonomic `FEntity` handle wrapper supporting `AddComponent`, `GetComponent`, `HasComponent`, and `RemoveComponent`.
  - UE-style components:
    - `FTagComponent`: Entity naming and identification.
    - `FTransformComponent`: Translation, Euler rotation, scale, and `GetTransform()` matrix synthesis.
    - `FMeshComponent`: VertexArray, Shader, Texture, color tint, and texturing flags.
    - `FDirectionalLightComponent`, `FPointLightComponent`, `FSpotLightComponent`: Unified lighting ECS integration.
    - `FCameraComponent`: Camera binding and viewport synchronization.
  - Automated `FScene::OnRender(InCamera)` pipeline traversing lighting components, uploading uniforms, and dispatching all entity mesh draws.
- **Client Sandbox App Refactoring**:
  - `Sandbox` migrated from manual render submissions to full ECS `FScene` and offscreen `FFramebuffer` render pipeline.


### Added
- **Multi-Stage Shader File Loading Pipeline (`.glsl`)**:
  - `FShader::Create("Assets/Shaders/DirectionalLit.glsl")` loading external shaders directly from disk.
  - Multi-stage shader preprocessor supporting `#type vertex` and `#type fragment` / `#type pixel` within a single file.
  - Automatic shader name derivation from file stem.
- **2D Texture RHI Subsystem (`FTexture2D`)**:
  - Abstract `FTexture` and `FTexture2D` interfaces in engine core with slot binding (`Bind(slot)`) and data mutation (`SetData`).
  - `FOpenGLTexture2D` implementation supporting RGB8 and RGBA8 formats, trilinear filtering, mipmaps, and wrapping.
  - Integrated `stb_image` for fast, lightweight PNG and JPG image decoding.
- **Procedural 3D/2D Geometric Mesh Primitives (`FMeshPrimitives`)**:
  - `FMeshPrimitives::CreateCube(size)`: Procedural indexed 3D cube with per-face normals, UVs, and colors (24 vertices, 36 indices).
  - `FMeshPrimitives::CreateCylinder(bottomRadius, topRadius, height, segments, bCaps)`: Procedural 3D cylinder / cone with side normals and top/bottom cap generation.
  - `FMeshPrimitives::CreateQuad(width, height)`: 2D indexed quad on the XY plane.
  - `FMeshPrimitives::CreateSphere(radius, segments, rings)`: 3D UV Sphere generator.
  - `FMeshPrimitives::CreatePlane(width, depth, subX, subZ)`: 3D XZ ground plane grid.
- **Lighting Subsystem (`Light.hpp` & Multi-Light Shader Pipeline)**:
  - `FDirectionalLight`: Directional sunlight with ambient, diffuse, and specular terms.
  - `FPointLight`: Omni-directional point light with distance attenuation (`constant`, `linear`, `quadratic`).
  - `FSpotLight`: Directional cone spot light with smooth penumbra cutoff (`cutOff`, `outerCutOff`) and distance attenuation.
  - Upgraded multi-light Blinn-Phong shader (`Engine/Assets/Shaders/DirectionalLit.glsl`) calculating combined illumination from all light sources.
- **Performance & Diagnostics HUD Overlay (`FDebugOverlay` & `F1` Hotkey)**:
  - Global `F1` hotkey in `FApplication` toggling real-time performance telemetry.
  - Integrated official **Inter TrueType Font** (`Engine/Assets/Fonts/Inter-Regular.ttf`) rasterized with `stb_truetype` for crisp, anti-aliased typography.
  - Real-time **VRAM Telemetry** (dedicated video memory and current usage) via OpenGL `GL_NVX_gpu_memory_info` with Windows DXGI fallback.
  - Real-time smoothed **FPS**, **Frame Time (ms)**, **RAM usage (Process Working Set & Peak)**, **GPU model & OpenGL driver**, **Viewport resolution**, and **Render stats (Draw calls, Triangles, Vertices)**.
- **3D Light Debug Gizmos & Wireframe Pipeline (`FDebugRenderer` & `F2` Hotkey)**:
  - Global `F2` hotkey toggling 3D wireframe light visualizers.
  - `FDebugRenderer::DrawSpotLightGizmo`: Visualizes inner cone (`cutOff`), outer penumbra cone (`outerCutOff`), and center ray.
  - `FDebugRenderer::DrawPointLightGizmo`: Visualizes center 3D marker and spherical distance attenuation boundary (XY, XZ, YZ rings).
  - `FDebugRenderer::DrawDirectionalLightGizmo`: Visualizes sunlight directional rays and orientation arrows.
- **Render Telemetry & Line Rendering RHI Subsystem**:
  - `FRenderStats`: Tracks `DrawCalls`, `IndexCount`, `VertexCount`, and `TriangleCount` per frame in `FRenderer`.
  - Added `DrawLines` and `SetLineWidth` to `IRenderAPI`, `FRenderCommand`, and `FOpenGLRenderAPI`.
  - `FPlatformMemory`: Cross-platform RAM queries (Win32 `GetProcessMemoryInfo`), VRAM queries (`GL_NVX_gpu_memory_info` & DXGI), and OpenGL GPU strings.
- **Engine vs Project Asset Separation**:
  - `Engine/Assets/Fonts/Inter-Regular.ttf` for built-in engine typography.
  - `Engine/Assets/Shaders/DirectionalLit.glsl` for built-in engine shaders.
  - `Engine/Assets/Shaders/DebugLine.glsl` and `Engine/Assets/Shaders/DebugFont.glsl` for built-in debug rendering.
  - `Projects/Sandbox/Assets/Textures/Container_Diffuse.png` for project-specific textures.

---

## [0.2.0] - 2026-08-14

### Added
- **Interactive 3D Perspective Camera Subsystem**:
  - `FPerspectiveCamera`: View, Projection, and View-Projection matrix calculations with FOV, aspect ratio, and near/far clipping plane management.
  - `FPerspectiveCameraController`: Free-fly / FPS camera controller with WASD translation, vertical flight (`Space` / `LeftControl`), speed boost (`LeftShift`), mouse look (pitch and yaw with gimbal lock prevention), and scroll wheel zoom.
- **Xbox & Standard Gamepad Support**:
  - Native real-time gamepad polling in `FInput` (`IsGamepadConnected`, `IsGamepadButtonPressed`, `GetGamepadAxis`, `GetGamepadLeftStick`, `GetGamepadRightStick`).
  - Seamless gamepad navigation in `FPerspectiveCameraController` (Left Stick for movement, Right Stick for camera rotation, Triggers for elevation, L3/RB for boost) with configurable deadzone filtering.
- **Developer Automation Tooling (`scripts/`)**:
  - Explicit Python scripts: `build_incremental.py`, `clean_rebuild.py`, `run_sandbox.py`, `format_code.py`.
  - Matching native PowerShell scripts: `BuildIncremental.ps1`, `CleanRebuild.ps1`, `RunSandbox.ps1`, `FormatCode.ps1`.
- **Language Server & Formatting Configurations**:
  - Project-level `.clang-format` enforcing modern C++20 game engine code styling.
  - Root `.clangd` and `.vscode/settings.json` with `--query-driver` configuration for MinGW GCC toolchains.
- **Comprehensive Git Ignore**:
  - Professional `.gitignore` covering C++, CMake, Ninja, Clangd, Python tooling, Visual Studio, and VS Code.

### Changed
- **Unreal Engine C++ Coding Standard Alignment**:
  - Renamed classes, structs, interfaces, and enums to adhere to official UE naming conventions (`F` for classes/structs, `I` for pure interfaces, `E` for enumerations, `T` for templates/smart pointer aliases, `b` for booleans, `In` for input parameters).
  - Replaced legacy types with `FApplication`, `FWindow`, `FLayer`, `FLayerStack`, `FRenderer`, `FRenderCommand`, `IGraphicsContext`, `IRenderAPI`, `IRenderDriver`, `FVertexBuffer`, `FIndexBuffer`, `FVertexArray`, `FShader`, and `FLog`.
- **Renderer Scene Pipeline**:
  - Extended `FRenderer::BeginScene` to accept `FPerspectiveCamera`, automatically uploading and binding `u_ViewProjection` and `u_ViewPos` uniforms to submitted shaders.
- **Sandbox Target & Executable**:
  - Renamed client executable from `LeonSandbox` to `Sandbox` (`./build/Projects/Sandbox/Sandbox.exe`).

---

## [0.1.0] - 2026-08-13

### Added
- **Core Engine Architecture**:
  - `FApplication` main loop, lifecycle hooks (`OnInit`, `OnUpdate`, `OnEvent`, `OnShutdown`), and graceful shutdown.
  - Modular `FLayer` and `FLayerStack` for feature isolation and event dispatching.
  - `FWindow` cross-platform window management backed by GLFW with VSync and custom event callbacks.
  - `FLog` ANSI-colored logging with `std::format` support and severity levels (`Trace`, `Info`, `Warn`, `Error`, `Fatal`).
  - `FTimestep` frame delta time calculation.
  - `FEvent` event bus dispatching application, window, keyboard, and mouse events.
  - Automated client entry point (`EntryPoint.hpp`).
- **Render Hardware Interface (RHI) Decoupling**:
  - `IRenderDriver` and `FRenderDriverRegistry` abstract factory pattern eliminating core dependencies on graphics backends.
  - Abstract RHI interfaces: `IGraphicsContext`, `IRenderAPI`, `FVertexBuffer`, `FIndexBuffer`, `FBufferLayout`, `FVertexArray`, `FShader`.
  - Static execution dispatcher `FRenderCommand` and high-level `FRenderer` submission pipeline.
- **OpenGL RHI Plugin**:
  - Implemented `FOpenGLContext`, `FOpenGLRenderAPI`, `FOpenGLVertexBuffer`, `FOpenGLIndexBuffer`, `FOpenGLVertexArray`, `FOpenGLShader`, and `FOpenGLRenderDriver`.
  - Vendor-isolated in `Plugins/RHI/OpenGL/` linking with GLAD and GLFW.
- **3D Graphics & Directional Lighting Scene**:
  - Enabled hardware depth testing (`GL_DEPTH_TEST`) in RHI.
  - Integrated GLM 1.0.1 via CMake `FetchContent`.
  - 3D Cube mesh (36 vertices with positions, normals, vertex colors).
  - Blinn-Phong directional lighting shader (ambient, diffuse, specular).
- **Technical Documentation**:
  - Comprehensive `Docs/ARCHITECTURE.md` and `Docs/NAMING.md`.
