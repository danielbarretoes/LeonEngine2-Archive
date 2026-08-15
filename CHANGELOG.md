# Changelog

All notable changes to the **LeonEngine2** game engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Post-Processing & Anti-Aliasing Stack (Dual-Kawase Bloom pyramid + FXAA 3.11).
- Frustum Culling with AABB / Bounding Sphere hierarchy.
- Render Queue sorting (Opaque front-to-back, Transparent back-to-front).
- GPU Dynamic Instancing (`glDrawElementsInstanced` / SSBOs).

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
