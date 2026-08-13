# Naming Conventions and Coding Standards (Unreal Engine Inspired) - LeonEngine2

This document establishes the official conventions for naming, directory hierarchy, and coding standards for **LeonEngine2**, directly inspired by the **Unreal Engine C++ Coding Standard**. For in-depth architectural principles, refer to [ARCHITECTURE.md](file:///c:/Users/Daniel/Desktop/Code/LeonEngine2/Docs/ARCHITECTURE.md).

---

## 1. Unreal Engine Inspired Type & Symbol Naming Standards

LeonEngine2 strictly adheres to the standard Unreal Engine prefixing conventions to ensure clean readability, explicit type contracts, and professional consistency:

| Category | Prefix / Convention | Description | Examples in LeonEngine2 |
| :--- | :--- | :--- | :--- |
| **Classes & Structs** | `F` | Plain C++ classes and structures | `FApplication`, `FWindow`, `FLayer`, `FLayerStack`, `FRenderer`, `FRenderCommand`, `FShader`, `FVertexBuffer`, `FIndexBuffer`, `FVertexArray`, `FBufferElement`, `FBufferLayout`, `FWindowProps`, `FApplicationProps`, `FTimestep`, `FLog` |
| **Interfaces** | `I` | Pure abstract interfaces & RHI contracts | `IGraphicsContext`, `IRenderAPI`, `IRenderDriver` |
| **Enumerations** | `E` | Enum classes and scoped enumerations | `EShaderDataType`, `ERenderAPI`, `ELogLevel`, `EEventType`, `EEventCategory` |
| **Templates / Smart Pointers** | `T` | Template classes, smart pointer aliases | `TScope<T>`, `TRef<T>`, `MakeScope<T>`, `MakeRef<T>` |
| **Booleans** | `b` | Boolean variables and flags | `bRunning`, `bMinimized`, `bVSync`, `bHandled`, `bIsRepeat`, `bNormalized` |
| **Function Parameters** | `In` (`PascalCase`) | Input parameters to functions/methods | `InProps`, `InWidth`, `InHeight`, `InDeltaTime`, `InShader`, `InVertexArray`, `InName` |
| **Member Variables** | `m_` / `b` (`PascalCase`) | Private/protected member variables | `m_Window`, `m_LayerStack`, `m_Data`, `bRunning`, `bMinimized` |
| **Static Variables** | `s_` (`PascalCase`) | Static/global internal variables | `s_Instance`, `s_API`, `s_GLFWWindowCount`, `s_Drivers` |
| **Macros & Constants** | `LE_` (`SCREAMING_SNAKE_CASE`)| Logging, assertion, event binding macros | `LE_CORE_INFO`, `LE_BIND_EVENT_FN`, `BIT(x)` |
| **Namespaces** | `PascalCase` | Top-level engine namespace | `Leon`, `Leon::Key`, `Leon::Mouse` |

---

## 2. Automation & Scripting Naming Standards (`scripts/`)

Tooling and build automation scripts follow the official naming standards of their respective ecosystems:

### 2.1 Python Scripts (`.py`): `snake_case` (PEP 8)
- **Convention**: `<action>_<modifier_or_target>.py`
- **Rule**: All lowercase, words separated by underscores (`_`). Names must be explicit and self-explanatory.
- **Inventory**:
  - `build_incremental.py`: Compiles only modified files since the last build (ultra-fast).
  - `clean_rebuild.py`: Cleans all previously compiled binaries and rebuilds 100% of the project from zero.
  - `run_sandbox.py`: Compiles pending changes and launches `Sandbox.exe`.
  - `format_code.py`: Automatically formats all C++ sources and headers using `clang-format`.

### 2.2 PowerShell Scripts (`.ps1`): `PascalCase` / `VerbNoun` (Microsoft PowerShell Standard)
- **Convention**: `<Verb><ModifierOrTarget>.ps1`
- **Rule**: Capitalized words without separators (`PascalCase`). Matches standard PowerShell command structure.
- **Inventory**:
  - `BuildIncremental.ps1`: Compiles only modified files since the last build (ultra-fast).
  - `CleanRebuild.ps1`: Cleans all previously compiled binaries and rebuilds 100% of the project from zero.
  - `RunSandbox.ps1`: Compiles pending changes and launches `Sandbox.exe`.
  - `FormatCode.ps1`: Automatically formats all C++ sources and headers using `clang-format`.

---

## 3. Directory Naming Standards

### 3.1 General Rule: `PascalCase`
- **All module, subsystem, and layer directories must be named in `PascalCase`**, with the explicit exception of standard source/header directories (`include` and `src`).
- Examples:
  - `Engine/`
  - `Engine/Core/`
  - `Engine/Renderer/`
  - `Plugins/`
  - `Plugins/RHI/`
  - `Plugins/RHI/OpenGL/`
  - `ThirdParty/`
  - `Projects/`
  - `Projects/Sandbox/`
  - `Docs/`
  - `scripts/`

### 3.2 Source and Header Folders: `minúsculas` / `lowercase`
- Standard source/header folders within each module are named in lowercase:
  - `src/`: Implementation files (`.cpp`, `.c`).
  - `include/`: Public/exported header files (`.hpp`, `.h`).
- Namespace subdirectories inside `include/` follow module conventions (e.g., `include/engine/core/`, `include/engine/renderer/`, `include/opengl/`).

---

## 4. File Naming Standards

### 4.1 C++ / C Source Files (`.cpp`, `.hpp`, `.h`)
- All C++ source and header files use **`PascalCase`**, matching the primary subsystem or class they define:
  - `Application.cpp` / `Application.hpp`
  - `Window.cpp` / `Window.hpp`
  - `Layer.hpp` / `LayerStack.hpp`
  - `GraphicsContext.hpp` / `GraphicsContext.cpp`
  - `RenderDriver.hpp` / `RenderDriver.cpp`
  - `RenderAPI.cpp` / `RenderAPI.hpp`
  - `Buffer.cpp` / `Buffer.hpp`
  - `OpenGLShader.cpp` / `OpenGLShader.hpp`
  - `OpenGLRenderDriver.cpp` / `OpenGLRenderDriver.hpp`
  - `SandboxApp.cpp`
### 4.2 Asset Naming Standards (Unreal Engine Standard)
- **Shaders (`.glsl`)**: Use `PascalCase` with descriptive purpose and standard lighting models:
  - `DefaultLit.glsl` (Multi-light Blinn-Phong shading model)
  - `DebugLine.glsl` (3D debug wireframe gizmo rendering)
  - `DebugFont.glsl` (2D orthographic text & HUD overlay)
- **Textures (`.png`, `.jpg`)**: Use `T_<Asset>_<Suffix>` prefix:
  - `T_Container_D.png` (`_D` for Diffuse/Albedo)
  - `T_Container_N.png` (`_N` for Normal map)
  - `T_Container_S.png` (`_S` for Specular/Roughness)
- **Fonts (`.ttf`, `.otf`)**: Use `<FontFamily>-<Weight>.ttf`:
  - `Inter-Regular.ttf`

---

## 5. Architectural Directory Hierarchy

For the official, single-source-of-truth directory tree and module dependency specifications, refer to:
👉 [**`Docs/ARCHITECTURE.md` — Section 2: Directory Hierarchy**](ARCHITECTURE.md#2-directory-hierarchy)

