# Changelog

All notable changes to the **LeonEngine2** game engine will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Dear ImGui debug and editor overlay layer (`FImGuiLayer`).
- Transform component and multi-object 3D scene rendering.
- 3D Model loading pipeline with Assimp (glTF / OBJ).

---

## [0.3.0] - 2026-08-14

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
