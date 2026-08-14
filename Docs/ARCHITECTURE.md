# LeonEngine2 - Architecture & System Design

This document details the architectural principles, dependency hierarchy, subsystem responsibilities, and extension guidelines for **LeonEngine2**.

---

## 1. Architectural Principles & Dependency Direction

LeonEngine2 follows a strict, unidirectional dependency hierarchy adhering to **Dependency Inversion** and **Separation of Concerns**, with naming standards inspired by the **Unreal Engine C++ Coding Standard**.

```
┌─────────────────────────────────────────────────────────────┐
│                          PROJECTS                           │
│             (Sandbox, Editor, Game Applications)            │
└──────────────────────────────┬──────────────────────────────┘
                               │
                ┌──────────────┴──────────────┐
                │                             │
                ▼ (uses engine)               ▼ (links backend)
┌──────────────────────────────┐     ┌────────────────────────┐
│            ENGINE            │     │      PLUGINS (RHI)     │
│   (Core, Window, Lifecycle,  │◄────┤  (OpenGL, Vulkan, etc.)│
│   Renderer Interfaces, RHI)  │     │                        │
└──────────────────────────────┘     └────────────────────────┘
```

### Strict Rules:

1. **Engine $\rightarrow$ Projects**: **FORBIDDEN.** The engine is an agnostic reusable library. It must never reference or include any code from `Projects/`.
2. **Plugins $\rightarrow$ Projects**: **FORBIDDEN.** Hardware plugins/drivers are low-level rendering backends. They must never know about client applications.
3. **Plugins $\rightarrow$ Engine (`include/`)**: **ALLOWED & REQUIRED.** Plugins depend on the abstract interfaces defined in `Engine/include/engine/` (such as `IGraphicsContext`, `FVertexBuffer`, `IRenderDriver`, `Base.hpp`) in order to implement them.
4. **Engine $\rightarrow$ Plugins**: **FORBIDDEN.** The `Engine` core contains **zero `#include` directives** pointing to plugin implementation headers (e.g., `opengl/...`). All hardware object instantiation is mediated via the **`FRenderDriverRegistry`** factory registry.

---

## 2. Directory Hierarchy

```text
LeonEngine2/
├── CMakeLists.txt                         # Root CMake build orchestrator
├── .clang-format                          # C++20 code formatting rules
├── .clangd                                # Clangd language server config
├── Docs/                                  # Technical specifications
│   ├── ARCHITECTURE.md                    # System architecture guide (this file)
│   └── NAMING.md                          # UE-inspired naming conventions & coding standard
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
├── Engine/                                # Core Engine Subsystems (Leon::Core)
│   ├── CMakeLists.txt
│   ├── Assets/                            # Built-in Engine Assets
│   │   ├── Fonts/                         # Engine typography assets
│   │   │   └── Inter-Regular.ttf          # Official Inter TrueType font
│   │   └── Shaders/                       # Core engine multi-stage shaders
│   │       ├── DebugFont.glsl             # 2D orthographic font & HUD panel shader
│   │       ├── DebugLine.glsl             # 3D line & wireframe gizmo shader
│   │       ├── DefaultLit.glsl            # Multi-light Blinn-Phong shader with shadows
│   │       ├── PBR_Lit.glsl               # Cook-Torrance PBR multi-light shader with IBL & ACES Tonemapping
│   │       ├── ShadowDepth.glsl           # High-speed depth pre-pass shader for directional shadow maps
│   │       └── Skybox.glsl                # Atmospheric physical HDR skybox shader (Rayleigh/Mie)
│   ├── include/                           # Public exported headers
│   │   └── engine/
│   │       ├── LeonEngine.hpp             # Master include header
│   │       ├── core/                      # Application foundation
│   │       │   ├── Application.hpp        # FApplication & FApplicationProps
│   │       │   ├── Base.hpp               # TScope, TRef, MakeScope, MakeRef
│   │       │   ├── EntryPoint.hpp         # Standard main() execution entry point
│   │       │   ├── Input.hpp              # FInput polling (Keyboard, Mouse, Gamepad)
│   │       │   ├── Layer.hpp              # FLayer base class
│   │       │   ├── LayerStack.hpp         # FLayerStack container
│   │       │   ├── Log.hpp                # FLog & ELogLevel
│   │       │   ├── PlatformMemory.hpp     # FPlatformMemory & FMemoryStats (RAM, GPU queries)
│   │       │   ├── Timestep.hpp           # FTimestep wrapper
│   │       │   ├── Window.hpp             # FWindow & FWindowProps
│   │       │   └── events/                # Event dispatching subsystem
│   │       │       ├── ApplicationEvent.hpp
│   │       │       ├── Event.hpp
│   │       │       ├── KeyEvent.hpp
│   │       │       └── MouseEvent.hpp
│   │       ├── renderer/                  # Hardware abstraction interfaces
│   │       │   ├── Buffer.hpp             # FVertexBuffer, FIndexBuffer, FBufferLayout
│   │       │   ├── DebugOverlay.hpp       # FDebugOverlay (F1 Performance & Stats HUD)
│   │       │   ├── DebugRenderer.hpp      # FDebugRenderer (F2 3D Light Gizmos & Lines)
│   │       │   ├── Framebuffer.hpp        # FFramebuffer RHI & offscreen render targets
│   │       │   ├── GraphicsContext.hpp    # IGraphicsContext
│   │       │   ├── Light.hpp              # FDirectionalLight, FPointLight, FSpotLight
│   │       │   ├── MeshPrimitives.hpp     # FMeshPrimitives (Cube, Cylinder, Quad, Sphere, Plane with Tangents)
│   │       │   ├── PerspectiveCamera.hpp  # FPerspectiveCamera
│   │       │   ├── PerspectiveCameraController.hpp # FPerspectiveCameraController
│   │       │   ├── RenderAPI.hpp          # IRenderAPI & ERenderAPI
│   │       │   ├── RenderCommand.hpp      # FRenderCommand
│   │       │   ├── RenderDriver.hpp       # IRenderDriver & FRenderDriverRegistry
│   │       │   ├── RenderStats.hpp        # FRenderStats (DrawCalls, Tris, Vertices)
│   │       │   ├── Renderer.hpp           # FRenderer
│   │       │   ├── Shader.hpp             # FShader
│   │       │   ├── Texture.hpp            # FTexture & FTexture2D
│   │       │   └── VertexArray.hpp        # FVertexArray
│   │       └── scene/                     # Scene & Entity Component System (ECS)
│   │           ├── Components.hpp         # FTag, FTransform, FMesh, FPBRMaterial, FSkybox, FLight, FCamera components
│   │           ├── Entity.hpp             # FEntity wrapper around EnTT handles
│   │           └── Scene.hpp              # FScene world container, shadow pass, skybox & render dispatcher


│   └── src/                               # Internal engine implementations
│       ├── core/                          # Core subsystem implementations
│       │   ├── Application.cpp            # FApplication (F1/F2 key handlers & auto-render)
│       │   ├── Input.cpp                  # FInput
│       │   ├── LayerStack.cpp             # FLayerStack
│       │   ├── Log.cpp                    # FLog
│       │   ├── PlatformMemory.cpp         # FPlatformMemory (Win32 & OpenGL queries)
│       │   └── Window.cpp                 # FWindow
│       ├── renderer/                      # Renderer & RHI implementations
│       │   ├── Buffer.cpp                 # FVertexBuffer, FIndexBuffer
│       │   ├── DebugOverlay.cpp           # FDebugOverlay HUD batcher & 8x8 font
│       │   ├── DebugRenderer.cpp          # FDebugRenderer 3D line & gizmo batcher
│       │   ├── Framebuffer.cpp            # FFramebuffer
│       │   ├── GraphicsContext.cpp        # IGraphicsContext
│       │   ├── MeshPrimitives.cpp         # FMeshPrimitives procedural generation
│       │   ├── PerspectiveCamera.cpp      # FPerspectiveCamera
│       │   ├── PerspectiveCameraController.cpp # FPerspectiveCameraController
│       │   ├── RenderAPI.cpp              # IRenderAPI
│       │   ├── RenderCommand.cpp          # FRenderCommand
│       │   ├── RenderDriver.cpp           # FRenderDriverRegistry
│       │   ├── Renderer.cpp               # FRenderer
│       │   ├── Shader.cpp                 # FShader
│       │   ├── Texture.cpp                # FTexture2D
│       │   └── VertexArray.cpp            # FVertexArray
│       └── scene/                         # Scene & ECS implementations
│           ├── Entity.cpp                 # FEntity
│           └── Scene.cpp                  # FScene
│
├── Plugins/                               # Hardware Backends and Extensions
│   └── RHI/
│       └── OpenGL/                        # OpenGL plugin library (Leon::OpenGL)
│           ├── CMakeLists.txt
│           ├── include/
│           │   └── opengl/                # Exported OpenGL backend headers
│           │       ├── OpenGLBuffer.hpp   # FOpenGLVertexBuffer, FOpenGLIndexBuffer
│           │       ├── OpenGLContext.hpp  # FOpenGLContext
│           │       ├── OpenGLFramebuffer.hpp # FOpenGLFramebuffer
│           │       ├── OpenGLRenderAPI.hpp # FOpenGLRenderAPI
│           │       ├── OpenGLRenderDriver.hpp # FOpenGLRenderDriver
│           │       ├── OpenGLShader.hpp   # FOpenGLShader
│           │       ├── OpenGLTexture2D.hpp # FOpenGLTexture2D
│           │       └── OpenGLVertexArray.hpp # FOpenGLVertexArray
│           └── src/                       # Internal OpenGL implementations
│               ├── OpenGLBuffer.cpp
│               ├── OpenGLContext.cpp
│               ├── OpenGLFramebuffer.cpp
│               ├── OpenGLRenderAPI.cpp
│               ├── OpenGLRenderDriver.cpp
│               ├── OpenGLShader.cpp
│               ├── OpenGLTexture2D.cpp
│               └── OpenGLVertexArray.cpp

│
├── ThirdParty/                            # External Dependencies
│   ├── glad/                              # OpenGL loader (Leon::Glad)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── KHR/khrplatform.h
│   │   │   └── glad/glad.h
│   │   └── src/glad.c
│   └── stb/                               # stb image and font utilities (Leon::Stb)
│       ├── CMakeLists.txt
│       ├── stb_image.h
│       ├── stb_image.cpp
│       └── stb_truetype.h
│
└── Projects/                              # Client Applications & Demos
    └── Sandbox/                           # Interactive demo application (Sandbox)
        ├── CMakeLists.txt
        ├── Assets/                        # Project-specific Assets
        │   └── Textures/
        │       └── T_Container_D.png
        └── src/
            └── SandboxApp.cpp             # FLightingShowcaseLayer & FSandboxApp
```

---

## 3. Core Engine Subsystems

### 3.1 Application & Lifecycle (`Application.hpp`)

- Orchestrates the game loop, frame delta timing (`FTimestep`), and top-level window events.
- Manages the `FLayerStack`, calling `OnUpdate()` and routing input `OnEvent()` through active layers.

### 3.2 Modular Layer System (`Layer.hpp`, `LayerStack.hpp`)

- Allows game logic, debug tools, and UI systems to be isolated into distinct `FLayer` instances.
- Updates flow forward through the stack (`Layer 0 -> Layer N`), while events flow backwards from overlays down to base layers until marked handled (`bHandled = true`).

### 3.3 Window & Graphics Context (`Window.hpp`, `GraphicsContext.hpp`)

- `FWindow` creates the GLFW window surface and instantiates a `TScope<IGraphicsContext>` via `IGraphicsContext::Create()`.
- The graphics context initialization and buffer swap (`SwapBuffers()`) are owned and executed automatically by `FWindow`.

---

## 4. The Render Hardware Interface (RHI) Driver System

The RHI decoupling is powered by the **`IRenderDriver`** factory pattern:

```cpp
namespace Leon {

    class IRenderDriver {
    public:
        virtual ~IRenderDriver() = default;
        virtual TScope<IGraphicsContext> CreateGraphicsContext(void* InWindowHandle) = 0;
        virtual TScope<IRenderAPI> CreateRenderAPI() = 0;
        virtual TRef<FVertexBuffer> CreateVertexBuffer(unsigned int InSize) = 0;
        virtual TRef<FVertexBuffer> CreateVertexBuffer(const float* InVertices, unsigned int InSize) = 0;
        virtual TRef<FIndexBuffer> CreateIndexBuffer(const uint32_t* InIndices, unsigned int InCount) = 0;
        virtual TRef<FVertexArray> CreateVertexArray() = 0;
        virtual TRef<FShader> CreateShader(const std::string& InName, const std::string& InVertexSrc,
                                           const std::string& InFragmentSrc) = 0;
    };

    class FRenderDriverRegistry {
    public:
        static void RegisterDriver(ERenderAPI InAPI, TScope<IRenderDriver> InDriver);
        static IRenderDriver* GetDriver(ERenderAPI InAPI);
        static IRenderDriver* GetActiveDriver();
    };

}
```

### How Resources are Created Agnostically

When client code or engine code requests a GPU resource:

```cpp
Leon::TRef<Leon::FVertexBuffer> vb = Leon::FVertexBuffer::Create(vertices, size);
```

1. `FVertexBuffer::Create` queries `FRenderDriverRegistry::GetActiveDriver()`.
2. The active driver (e.g., `FOpenGLRenderDriver`) instantiates the concrete backend resource (`FOpenGLVertexBuffer`).
3. The engine never needs to know the concrete types or include backend headers.

---

## 5. Adding a New Graphics Backend (e.g., Vulkan or DirectX 12)

To add a new backend (e.g., Vulkan):

1. Create `Plugins/RHI/Vulkan/` with its own `CMakeLists.txt`.
2. Implement the engine interfaces:
   - `FVulkanContext : public IGraphicsContext`
   - `FVulkanRenderAPI : public IRenderAPI`
   - `FVulkanVertexBuffer : public FVertexBuffer`, `FVulkanIndexBuffer : public FIndexBuffer`
   - `FVulkanVertexArray : public FVertexArray`
   - `FVulkanShader : public FShader`
3. Implement `FVulkanRenderDriver : public IRenderDriver`.
4. Register the driver with `FRenderDriverRegistry::RegisterDriver(ERenderAPI::Vulkan, MakeScope<FVulkanRenderDriver>())`.
5. **No changes are needed inside `Engine/`!**
