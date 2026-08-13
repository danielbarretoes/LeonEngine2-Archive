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
- Namespace subdirectories inside `include/` follow module conventions (e.g., `include/engine/core/`, `include/engine/renderer/`, `include/plugin_opengl/`).

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
  - `LeonEngine.hpp`
  - `EntryPoint.hpp`

---

## 5. Architectural Directory Hierarchy

```
LeonEngine2/
├── CMakeLists.txt                         # Root CMake project orchestrator
├── .clang-format                          # Official C++20 code formatting rules
├── .clangd                                # Clangd language server configuration
├── .gitignore                             # Build artifacts and cache ignores
├── .vscode/                               # VS Code and IntelliSense configs
│   ├── c_cpp_properties.json
│   └── settings.json
│
├── Docs/                                  # Technical documentation & standards
│   ├── ARCHITECTURE.md                    # Architecture and dependency specifications
│   └── NAMING.md                          # Naming conventions and coding standard
│
├── scripts/                               # Developer workflow automation
│   ├── BuildIncremental.ps1               # Fast incremental build (PowerShell)
│   ├── CleanRebuild.ps1                   # Clean rebuild from scratch (PowerShell)
│   ├── RunSandbox.ps1                     # Build & run demo app (PowerShell)
│   ├── FormatCode.ps1                     # Code formatter (PowerShell)
│   ├── build_incremental.py               # Fast incremental build (Python)
│   ├── clean_rebuild.py                   # Clean rebuild from scratch (Python)
│   ├── run_sandbox.py                     # Build & run demo app (Python)
│   └── format_code.py                     # Code formatter (Python)
│
├── Engine/                                # Core Engine Shared Library (Leon::Core)
│   ├── CMakeLists.txt
│   ├── include/                           # Public Engine Header Files
│   │   └── engine/
│   │       ├── LeonEngine.hpp             # Master include header for clients
│   │       ├── core/                      # Application lifecycle & foundation
│   │       │   ├── Application.hpp        # FApplication & FApplicationProps
│   │       │   ├── Base.hpp               # TScope, TRef, MakeScope, MakeRef
│   │       │   ├── EntryPoint.hpp         # Standard engine main() entry point
│   │       │   ├── Input.hpp              # Key/Mouse/Gamepad polling
│   │       │   ├── Layer.hpp              # FLayer base class
│   │       │   ├── LayerStack.hpp         # FLayerStack container
│   │       │   ├── Log.hpp                # FLog & ELogLevel
│   │       │   ├── Timestep.hpp           # FTimestep wrapper
│   │       │   ├── Window.hpp             # FWindow & FWindowProps
│   │       │   └── events/                # Event dispatching subsystem
│   │       │       ├── ApplicationEvent.hpp # FWindowResizeEvent, FWindowCloseEvent
│   │       │       ├── Event.hpp          # FEvent, EEventType, EEventCategory, FEventDispatcher
│   │       │       ├── KeyEvent.hpp       # FKeyEvent, FKeyPressedEvent, FKeyReleasedEvent
│   │       │       └── MouseEvent.hpp     # FMouseMovedEvent, FMouseButtonPressedEvent, etc.
│   │       └── renderer/                  # RHI Interfaces and contracts
│   │           ├── Buffer.hpp             # FVertexBuffer, FIndexBuffer, FBufferLayout, EShaderDataType
│   │           ├── GraphicsContext.hpp    # IGraphicsContext interface
│   │           ├── PerspectiveCamera.hpp  # FPerspectiveCamera
│   │           ├── PerspectiveCameraController.hpp # FPerspectiveCameraController
│   │           ├── RenderAPI.hpp          # IRenderAPI interface & ERenderAPI
│   │           ├── RenderCommand.hpp      # FRenderCommand static dispatcher
│   │           ├── RenderDriver.hpp       # IRenderDriver & FRenderDriverRegistry
│   │           ├── Renderer.hpp           # FRenderer high-level API
│   │           ├── Shader.hpp             # FShader interface
│   │           ├── Texture.hpp            # FTexture & FTexture2D
│   │           └── VertexArray.hpp        # FVertexArray interface
│   └── src/                               # Internal Engine Implementations
│       ├── core/                          # Core subsystem implementations
│       │   ├── Application.cpp            # FApplication lifecycle & main loop
│       │   ├── Input.cpp                  # FInput polling implementations
│       │   ├── LayerStack.cpp             # FLayerStack implementation
│       │   ├── Log.cpp                    # FLog implementation
│       │   └── Window.cpp                 # FWindow implementation
│       └── renderer/                      # Renderer & RHI implementations
│           ├── Buffer.cpp                 # FVertexBuffer & FIndexBuffer factory dispatch
│           ├── GraphicsContext.cpp        # IGraphicsContext factory dispatch
│           ├── PerspectiveCamera.cpp      # FPerspectiveCamera implementation
│           ├── PerspectiveCameraController.cpp # FPerspectiveCameraController implementation
│           ├── RenderAPI.cpp              # IRenderAPI factory dispatch
│           ├── RenderCommand.cpp          # FRenderCommand static dispatcher
│           ├── RenderDriver.cpp           # FRenderDriverRegistry factory registry
│           ├── Renderer.cpp               # FRenderer high-level pipeline
│           ├── Shader.cpp                 # FShader factory dispatch
│           ├── Texture.cpp                # FTexture2D factory dispatch
│           └── VertexArray.cpp            # FVertexArray factory dispatch
│
├── Plugins/                               # Engine Plugins & Graphics Backends
│   └── RHI/                               # Render Hardware Interface Plugins
│       └── OpenGL/                        # OpenGL Backend Library (Leon::OpenGL)
│           ├── CMakeLists.txt
│           ├── include/
│           │   └── opengl/                # Exported OpenGL backend headers
│           │       ├── OpenGLBuffer.hpp   # FOpenGLVertexBuffer, FOpenGLIndexBuffer
│           │       ├── OpenGLContext.hpp  # FOpenGLContext
│           │       ├── OpenGLRenderAPI.hpp # FOpenGLRenderAPI
│           │       ├── OpenGLRenderDriver.hpp # FOpenGLRenderDriver
│           │       ├── OpenGLShader.hpp   # FOpenGLShader
│           │       ├── OpenGLTexture2D.hpp # FOpenGLTexture2D
│           │       └── OpenGLVertexArray.hpp # FOpenGLVertexArray
│           └── src/                       # Internal OpenGL implementations
│               ├── OpenGLBuffer.cpp
│               ├── OpenGLContext.cpp
│               ├── OpenGLRenderAPI.cpp
│               ├── OpenGLRenderDriver.cpp
│               ├── OpenGLShader.cpp
│               ├── OpenGLTexture2D.cpp
│               └── OpenGLVertexArray.cpp
│
├── ThirdParty/                            # Third-Party Dependencies (Vendored)
│   ├── glad/                              # OpenGL loader library (Leon::Glad)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── KHR/
│   │   │   │   └── khrplatform.h
│   │   │   └── glad/
│   │   │       └── glad.h
│   │   └── src/
│   │       └── glad.c
│   └── stb/                               # stb image decoding library
│       ├── CMakeLists.txt
│       ├── stb_image.h
│       └── stb_image.cpp
│
└── Projects/                              # Client Applications & Demos
    └── Sandbox/                           # Sandbox Demo App (Sandbox)
        ├── CMakeLists.txt
        └── src/
            └── SandboxApp.cpp             # FCubeLayer & FSandboxApp client demo
```
