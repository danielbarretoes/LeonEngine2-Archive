# LeonEngine2 — Architecture & System Design

This document details the architectural principles, dependency hierarchy, subsystem responsibilities, and extension guidelines for **LeonEngine2**.

---

## 1. Architectural Principles & Dependency Direction

LeonEngine2 follows a strict, unidirectional dependency hierarchy adhering to **Dependency Inversion** and **Separation of Concerns**, with naming standards inspired by the **Unreal Engine C++ Coding Standard**.

```text
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
3. **Plugins $\rightarrow$ Engine (`include/`)**: **ALLOWED & REQUIRED.** Plugins depend on the abstract interfaces defined in `Engine/include/` (such as `renderer/GraphicsContext.hpp`, `renderer/Buffer.hpp`, `renderer/RenderDriver.hpp`, `core/Base.hpp`) in order to implement them.
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
│   ├── IBL_CACHE_DESIGN.md                # Image-Based Lighting cache specification (.libl v4)
│   ├── NAMING.md                          # UE-inspired naming conventions & coding standard
│   ├── RENDERER_FEATURE_AUDIT.md          # Comprehensive renderer capabilities & milestones audit
│   ├── RENDERER_MATERIALS.md              # Advanced Materials & Surface Detail Architecture (v0.8.0)
│   ├── RENDERER_POSTPROCESSING.md         # Post-Processing Pipeline Architecture (v0.7.0)
│   └── RENDERER_TEST_COVERAGE.md          # Renderer mathematical test coverage & mutation report
│
├── Scripts/                               # Developer build and run scripts (Python 3.14 / Ninja)
│   ├── build_incremental.py               # Fast incremental build runner
│   └── run_sandbox.py                     # Build & launch executable runner
│
├── Engine/                                # Core Engine Subsystems (Leon::Core)
│   ├── CMakeLists.txt
│   ├── Assets/                            # Built-in Engine Assets
│   │   ├── Fonts/                         # Engine typography assets
│   │   │   ├── Inter-Bold.ttf             # 3D In-World text TrueType font
│   │   │   └── Inter-Regular.ttf          # Diagnostics HUD TrueType font
│   │   ├── Shaders/                       # Core engine multi-stage shaders
│   │   │   ├── BloomBrightPass.glsl       # Soft-knee HDR luminance extraction pass
│   │   │   ├── BloomDownsample.glsl       # Jimenez 13-tap downsampling filter with Karis luma weighting
│   │   │   ├── BloomUpsample.glsl         # 9-tap tent upsampling filter with additive blending
│   │   │   ├── DebugFont.glsl             # 2D orthographic font & HUD panel shader
│   │   │   ├── DebugLine.glsl             # 3D line & wireframe gizmo shader
│   │   │   ├── FXAA.glsl                  # FXAA 3.11 Quality anti-aliasing shader
│   │   │   ├── PBR_Lit.glsl               # Cook-Torrance PBR multi-light shader with IBL, CSM & Debug Views
│   │   │   ├── ShadowDepth.glsl           # High-speed depth pass for Directional CSM & Spot Shadows
│   │   │   ├── Skybox.glsl                # Atmospheric physical HDR skybox shader (Rayleigh/Mie)
│   │   │   ├── ToneMapping.glsl           # Multi-operator tone mapper (ACES, Reinhard, Neutral, UC2) + Gamma 2.2
│   │   │   └── WorldText.glsl             # 3D in-world text geometry shader
│   │   └── Textures/                      # Precomputed offline textures
│   │       └── BRDF_LUT.bin               # Pre-baked 2D Cook-Torrance BRDF Look-Up Table (RG16F, 256x256)
│   │
│   ├── include/                           # Public exported headers
│   │   ├── LeonEngine.hpp                 # Master include header
│   │   ├── core/                          # Application foundation
│   │   │   ├── Application.hpp            # FApplication & FApplicationProps
│   │   │   ├── Base.hpp                   # TScope, TRef, MakeScope, MakeRef
│   │   │   ├── ConfigFile.hpp             # FConfigFile (.ini parser)
│   │   │   ├── EntryPoint.hpp             # Standard main() execution entry point
│   │   │   ├── Input.hpp                  # FInput polling (Keyboard, Mouse, Gamepad)
│   │   │   ├── Layer.hpp                  # FLayer base class
│   │   │   ├── LayerStack.hpp             # FLayerStack container
│   │   │   ├── Log.hpp                    # FLog & ELogLevel
│   │   │   ├── PlatformMemory.hpp         # FPlatformMemory (RAM, GPU queries)
│   │   │   ├── Timestep.hpp               # FTimestep delta-time wrapper
│   │   │   ├── Window.hpp                 # FWindow & FWindowProps
│   │   │   └── events/                    # Event dispatching subsystem
│   │   │       ├── ApplicationEvent.hpp
│   │   │       ├── Event.hpp
│   │   │       ├── KeyEvent.hpp
│   │   │       └── MouseEvent.hpp
│   │   ├── renderer/                      # Hardware abstraction interfaces
│   │   │   ├── AssetManager.hpp           # FAssetManager (Shaders, Textures, Materials)
│   │   │   ├── Buffer.hpp                 # FVertexBuffer, FIndexBuffer, FBufferLayout, FUniformBuffer
│   │   │   ├── DebugOverlay.hpp           # FDebugOverlay (F1 Performance & Stats HUD)
│   │   │   ├── DebugRenderer.hpp          # FDebugRenderer (F2 3D Light Gizmos & Lines)
│   │   │   ├── Framebuffer.hpp            # FFramebuffer RHI & offscreen render targets
│   │   │   ├── GraphicsContext.hpp        # IGraphicsContext
│   │   │   ├── IBLGenerator.hpp           # FIBLGenerator (Environment, Irradiance, Prefilter, .libl Cache)
│   │   │   ├── Light.hpp                  # FDirectionalLight, FPointLight, FSpotLight
│   │   │   ├── Material.hpp               # FMaterial & MaterialSerializer
│   │   │   ├── MaterialInstance.hpp       # FMaterialInstance & FPipelineState
│   │   │   ├── MeshPrimitives.hpp         # FMeshPrimitives (Cube, Sphere, Cylinder, Plane, Ramp, Pyramid)
│   │   │   ├── PerspectiveCamera.hpp      # FPerspectiveCamera
│   │   │   ├── PerspectiveCameraController.hpp # FPerspectiveCameraController
│   │   │   ├── PostProcessPipeline.hpp    # FPostProcessPipeline & FPostProcessSettings
│   │   │   ├── RenderAPI.hpp              # IRenderAPI & ERenderAPI
│   │   │   ├── RenderCommand.hpp          # FRenderCommand
│   │   │   ├── RenderDriver.hpp           # IRenderDriver & FRenderDriverRegistry
│   │   │   ├── RenderStats.hpp            # FRenderStats (DrawCalls, Tris, Vertices)
│   │   │   ├── Renderer.hpp               # FRenderer
│   │   │   ├── SceneRenderer.hpp          # FSceneRenderer (Multi-Pass Rendering Pipeline)
│   │   │   ├── Shader.hpp                 # FShader
│   │   │   ├── ShadowMath.hpp             # FShadowMath (CSM Practical Splits, Bounding Spheres, Texel Snapping)
│   │   │   ├── ShadowTypes.hpp            # EShadowFilterMode, ECascadeSplitScheme, FShadowSettings, FShadowCascade
│   │   │   ├── TextRenderer.hpp           # FTextRenderer (3D In-World Text Batching)
│   │   │   ├── Texture.hpp                # FTexture, FTexture2D, FTextureCube
│   │   │   └── VertexArray.hpp            # FVertexArray
│   │   └── scene/                         # Scene & Entity Component System (ECS)
│   │       ├── Components.hpp             # FTag, FTransform, FMesh, FMaterialComponent, FSkybox, FLights
│   │       ├── Entity.hpp                 # FEntity wrapper around EnTT handles
│   │       ├── Scene.hpp                  # FScene world container
│   │       └── SceneSerializer.hpp        # FSceneSerializer (.llevel scene deserializer)
│   │
│   └── src/                               # Internal engine implementations
│       ├── core/                          # Core subsystem implementations
│       │   ├── Application.cpp            # FApplication (Main loop & input routing)
│       │   ├── ConfigFile.cpp             # FConfigFile
│       │   ├── Input.cpp                  # FInput
│       │   ├── LayerStack.cpp             # FLayerStack
│       │   ├── Log.cpp                    # FLog
│       │   ├── PlatformMemory.cpp         # FPlatformMemory (Win32 & OpenGL queries)
│       │   └── Window.cpp                 # FWindow
│       ├── renderer/                      # Renderer & RHI implementations
│       │   ├── AssetManager.cpp           # FAssetManager
│       │   ├── Buffer.cpp                 # FVertexBuffer, FIndexBuffer, FUniformBuffer
│       │   ├── DebugOverlay.cpp           # FDebugOverlay HUD batcher
│       │   ├── DebugRenderer.cpp          # FDebugRenderer 3D line & gizmo batcher
│       │   ├── Framebuffer.cpp            # FFramebuffer
│       │   ├── GraphicsContext.cpp        # IGraphicsContext
│       │   ├── IBLGenerator.cpp           # FIBLGenerator (Quasi-Monte Carlo & Mip Filtering)
│       │   ├── Material.cpp               # FMaterial & MaterialSerializer
│       │   ├── MaterialInstance.cpp       # FMaterialInstance
│       │   ├── MeshPrimitives.cpp         # FMeshPrimitives procedural generation
│       │   ├── PerspectiveCamera.cpp      # FPerspectiveCamera
│       │   ├── PerspectiveCameraController.cpp # FPerspectiveCameraController
│       │   ├── RenderAPI.cpp              # IRenderAPI
│       │   ├── RenderCommand.cpp          # FRenderCommand
│       │   ├── RenderDriver.cpp           # FRenderDriverRegistry
│       │   ├── Renderer.cpp               # FRenderer
│       │   ├── SceneRenderer.cpp          # FSceneRenderer (Multi-Pass Engine Pipeline)
│       │   ├── Shader.cpp                 # FShader
│       │   ├── TextRenderer.cpp           # FTextRenderer 3D batching
│       │   ├── Texture.cpp                # FTexture2D & FTextureCube
│       │   └── VertexArray.cpp            # FVertexArray
│       └── scene/                         # Scene & ECS implementations
│           ├── Entity.cpp                 # FEntity
│           ├── Scene.cpp                  # FScene
│           └── SceneSerializer.cpp        # FSceneSerializer
│
├── Plugins/                               # Hardware Backends and Extensions
│   └── RHI/
│       └── OpenGL/                        # OpenGL 4.5 Core + DSA plugin library (Leon::OpenGL)
│           ├── CMakeLists.txt
│           ├── include/                   # Exported OpenGL backend headers
│           │   ├── OpenGLBuffer.hpp       # FOpenGLVertexBuffer, FOpenGLIndexBuffer, FOpenGLUniformBuffer
│           │   ├── OpenGLContext.hpp      # FOpenGLContext
│           │   ├── OpenGLFramebuffer.hpp  # FOpenGLFramebuffer
│           │   ├── OpenGLRenderAPI.hpp    # FOpenGLRenderAPI (CPU State Cache)
│           │   ├── OpenGLRenderDriver.hpp # FOpenGLRenderDriver
│           │   ├── OpenGLShader.hpp       # FOpenGLShader
│           │   ├── OpenGLTexture2D.hpp    # FOpenGLTexture2D (DSA 2D textures)
│           │   ├── OpenGLTextureCube.hpp  # FOpenGLTextureCube (DSA Cubemaps)
│           │   └── OpenGLVertexArray.hpp  # FOpenGLVertexArray (DSA VAOs)
│           └── src/                       # Internal OpenGL implementations
│               ├── OpenGLBuffer.cpp
│               ├── OpenGLContext.cpp
│               ├── OpenGLFramebuffer.cpp
│               ├── OpenGLRenderAPI.cpp
│               ├── OpenGLRenderDriver.cpp
│               ├── OpenGLShader.cpp
│               ├── OpenGLTexture2D.cpp
│               ├── OpenGLTextureCube.cpp
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
        ├── Config/
        │   └── DefaultEngine.ini          # Sandbox configuration file
        ├── Content/                       # Project Asset Directory
        │   ├── Assets/
        │   │   ├── Hdr/                   # HDR Maps & Binary Caches (.libl)
        │   │   │   ├── AutumnField1k.hdr
        │   │   │   └── Cache/AutumnField1k.libl
        │   │   └── Textures/              # PBR Albedo, Normal, AO textures
        │   ├── Maps/
        │   │   └── MainShowcase.llevel    # Primary showcase level asset
        │   └── Materials/                 # First-Class Material Assets (.lmat)
        │       ├── M_BrushedIron.lmat
        │       ├── M_ContainerCube.lmat
        │       ├── M_EmeraldRamp.lmat
        │       ├── M_Emissive.lmat
        │       ├── M_FloorTiles.lmat
        │       ├── M_GoldMetal.lmat
        │       ├── M_PolishedGold.lmat
        │       ├── M_RedPlastic.lmat
        │       ├── M_RubyDielectric.lmat
        │       └── M_WhitePlastic.lmat
        └── src/
            └── SandboxApp.cpp             # FLightingShowcaseLayer & FSandboxApp
```

---

## 3. Core Engine Subsystems

### 3.1 Application & Lifecycle (`Application.hpp`)
* Orchestrates the main loop, frame delta timing (`FTimestep`), and top-level window events.
* Manages the `FLayerStack`, calling `OnUpdate()` and routing input `OnEvent()` through active layers.

### 3.2 Modular Layer System (`Layer.hpp`, `LayerStack.hpp`)
* Allows game logic, debug tools, and UI systems to be isolated into distinct `FLayer` instances.
* Updates flow forward through the stack (`Layer 0 -> Layer N`), while events flow backwards from overlays down to base layers until marked handled (`bHandled = true`).

### 3.3 Window & Graphics Context (`Window.hpp`, `GraphicsContext.hpp`)
* `FWindow` creates the GLFW window surface and instantiates a `TScope<IGraphicsContext>` via `IGraphicsContext::Create()`.
* Graphics context initialization and buffer swap (`SwapBuffers()`) are owned and executed automatically by `FWindow`.

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
        virtual TRef<FTexture2D> CreateTexture2D(uint32_t InWidth, uint32_t InHeight) = 0;
        virtual TRef<FTexture2D> CreateTexture2D(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat) = 0;
        virtual TRef<FTexture2D> CreateTexture2D(const std::string& InPath) = 0;
        virtual TRef<FTextureCube> CreateTextureCube(uint32_t InWidth, uint32_t InHeight, bool InbHDR) = 0;
        virtual TRef<FFramebuffer> CreateFramebuffer(const FFramebufferSpecification& InSpec) = 0;
        virtual TRef<FUniformBuffer> CreateUniformBuffer(uint32_t InSize, uint32_t InBinding) = 0;
    };

    class FRenderDriverRegistry {
    public:
        static void RegisterDriver(ERenderAPI InAPI, TScope<IRenderDriver> InDriver);
        static IRenderDriver* GetDriver(ERenderAPI InAPI);
        static IRenderDriver* GetActiveDriver();
    };

}
```

### Agnostic Resource Creation:
When client or engine code requests a GPU resource:
```cpp
Leon::TRef<Leon::FVertexBuffer> vb = Leon::FVertexBuffer::Create(vertices, size);
```
1. `FVertexBuffer::Create` queries `FRenderDriverRegistry::GetActiveDriver()`.
2. The active driver (e.g., `FOpenGLRenderDriver`) instantiates the concrete backend resource (`FOpenGLVertexBuffer`).
3. The engine never references backend headers directly.

---

## 5. First-Class Material System (`FMaterial` & `FMaterialInstance`)

The Material System decouples shader logic and asset properties into a shared, reusable resource hierarchy:

```text
┌─────────────────────────────────────────────────────────────┐
│                          FMaterial                          │
│               (Master Shader & Parameter Layout)            │
└──────────────────────────────┬──────────────────────────────┘
                               │ creates
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                      FMaterialInstance                      │
│        (Resolved Scalars, Textures & FPipelineState)        │
└──────────────────────────────┬──────────────────────────────┘
                               │ attached to
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                     FMaterialComponent                      │
│                  (ECS Entity Mesh Binding)                  │
└─────────────────────────────────────────────────────────────┘
```

* **`FPipelineState`**: Encapsulates `CullMode`, `bDepthTest`, `bDepthWrite`, `DepthFunc`, `bBlend`, `SrcBlend`, and `DstBlend`.
* **Serialization**: Material assets are stored as human-readable `.lmat` YAML files and loaded via `FAssetManager`.

---

## 6. Image-Based Lighting & Binary Cache Subsystem (`IBLGenerator.cpp`)

Provides physical Cook-Torrance ambient lighting using the Split-Sum approximation:

1. **2D BRDF LUT**: Pre-baked $256 \times 256$ `RG16F` lookup table loaded in **$1.4\text{ ms}$**.
2. **Diffuse Irradiance Cubemap (v4)**: Convolved using Quasi-Monte Carlo Hammersley cosine-weighted hemisphere sampling ($N=512$) with source-HDR solid angle Mip-filtering ($\text{lod} = 5.50$).
3. **Specular Prefiltered Cubemap (v4)**: 5 mip levels ($128 \to 8$) using importance-sampled GGX with Karis solid angle filtering against the original $1024 \times 512$ HDR image.
4. **Binary Disk Cache (`.libl` v4)**: Serialized with a 64-byte header and 64-bit FNV-1a content hash, enabling instantaneous startup (**$\approx 11\text{ ms}$**).

---

## 7. Multi-Pass Scene Rendering Pipeline (`FSceneRenderer`)

The frame rendering loop executes 6 distinct passes:

```text
PASS 1: Cascaded Shadow Pass (CSM)  ──► 4 Cascades in OpenGL Texture2DArray
PASS 2: Spot Shadow Pass            ──► 2D Depth Framebuffer (1024x1024)
PASS 3: Planar Reflection Pass      ──► Mirrored Camera Offscreen FBO (1280x720)
PASS 4: Main Geometry Pass          ──► HDR Scene Framebuffer (GL_RGBA16F)
PASS 5: Atmospheric Skybox Pass     ──► Rayleigh/Mie Procedural Sky or HDR Cubemap
PASS 6: 3D In-World Text Pass       ──► Batched Inter-Bold Typography with Depth
PASS 7: Post-Process Pass           ──► ACES Filmic Tone Mapping + Gamma 2.2
```

---

## 8. Diagnostic and Interactive Debugging Subsystem

* **HUD Overlay (`F1`)**: Real-time diagnostic panel rendering FPS, Frame Time (CPU/GPU), VRAM allocation, RAM usage, triangle counts, and draw call metrics.
* **Light Gizmos (`F2`)**: 3D wireframe cones for Spot Lights, bounding attenuation spheres for Point Lights, and directional sunlight vectors.
* **Material & Lighting Debug Views (`Shift + F1 .. F12`, `Shift + N`, `Shift + R`)**: Interactive hotkeys to isolate individual cubemap mips, diffuse irradiance, BRDF LUT, direct lighting, specular IBL, world normals, and reflection vectors in real time.
