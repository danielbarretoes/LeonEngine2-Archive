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
│   (Core, FWindow, Lifecycle,  │◄────┤  (OpenGL, Vulkan, etc.)│
│   Renderer Interfaces, RHI)  │     │                        │
└──────────────────────────────┘     └────────────────────────┘
```

### Strict Rules:

1. **Engine $\rightarrow$ Projects**: **FORBIDDEN.** The engine is an agnostic reusable library. It must never reference or include any code from `Projects/`, and must never hardcode a product name (e.g. Sandbox) or `Projects/Sandbox` path. Enforced by `Scripts/verify_ue_naming.py`.
2. **Plugins $\rightarrow$ Projects**: **FORBIDDEN.** Hardware plugins/drivers are low-level rendering backends. They must never know about client applications.
3. **Plugins $\rightarrow$ Engine Runtime Public headers**: **ALLOWED & REQUIRED.** Plugins depend on the abstract interfaces under `Engine/Source/Runtime/*/Public/` (such as `RHI/IGraphicsContext.hpp`, `RHI/FBuffer.hpp`, `RHI/IRenderDriver.hpp`, `Core/Base.hpp`) in order to implement them.
4. **Engine $\rightarrow$ Plugins**: **FORBIDDEN.** The `Engine` core contains **zero `#include` directives** pointing to plugin implementation headers (e.g., `opengl/...`). All hardware object instantiation is mediated via the **`FRenderDriverRegistry`** factory registry.

### Engine / Project isolation (Unreal-like)

Like Unreal, **build/run tools live with the Engine** and are invoked *against* a `.lproject`:

```text
python Scripts/build_project.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/run_project.py    --project Projects/Sandbox/Sandbox.lproject
python Scripts/validate_project.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/bake_lightmaps.py --project Projects/Sandbox/Sandbox.lproject
python Scripts/create_project.py --name MyGame --output D:/Games/MyGame
```

Environment: `LEON_ENGINE_ROOT` (optional), `LEON_PROJECT` (or `--project`). Full inventory: [SCRIPTS.md](SCRIPTS.md).

| Owner | Responsibility |
| :--- | :--- |
| Engine (`Engine/`, `Plugins/`, `Scripts/`, `Tools/`) | Runtime, RHI plugins, asset tool, build/verify scripts |
| Project (`Projects/<Name>/` or any external path) | `Main`, gameplay classes, Content, Config, `.lproject`, thin CMake target |

- `UEngine::Run` requires a `.lproject` (argv `--project=`, explicit path, or discovery) — **no Sandbox default inside Engine**.
- Root CMake selects the game via `LEON_PROJECT_DIR` (absolute path supported for external games; monorepo cache default may still be `Projects/Sandbox`).
- There is no UBT / `.Build.cs` yet; CMake + these Python scripts are the lite equivalent.
- Product shortcuts (e.g. Sandbox) live under `Projects/<Name>/Scripts/`, never as Engine defaults.

**Isolation checklist:** `grep` / CI must not find `Projects/Sandbox`, `Projects/LeonTournament`, or bare product names (`Sandbox`, `LeonTournament`) under `Engine/Source`. Enforced by `Scripts/verify_ue_naming.py` and `Tests/Gameplay/EngineGameSeparationTests.cpp`.

---

## 2. Directory Hierarchy

```text
LeonEngine2/
├── Docs/                                  # Technical specifications (NAMING, ARCHITECTURE, …)
├── Scripts/                               # Engine tooling — see Docs/SCRIPTS.md
├── Engine/
│   ├── Assets/                            # Engine shaders, fonts, BRDF LUT
│   ├── CMakeLists.txt                     # LeonEngineCore (single link unit today)
│   └── Source/Runtime/                    # Unreal-style modules (Public + Private)
│       ├── Core/Public/Core/             # FApplication, FWindow, FLog, …
│       ├── RHI/Public/RHI/                # IGraphicsContext, IRenderAPI, FBuffer, …
│       ├── Assets/Public/Assets/         # UAssetManager, UStaticMesh, importers
│       ├── Renderer/Public/Renderer/     # FWorldRenderer, FMaterial, IBL, post
│       ├── UMG/Public/UMG/               # UWidget hierarchy, FUIRenderer
│       ├── Engine/Public/Engine/         # UEngine, UWorld, serializers
│       ├── Gameplay/Public/Gameplay/     # AActor, APawn, UGameplayStatics, …
│       ├── AI/Public/AI/                 # Behavior trees, blackboard, perception
│       ├── Audio/Public/Audio/           # FAudioDevice, USoundWave
│       └── Physics/Public/Physics/       # IPhysicsScene, traces
├── Plugins/RHI/OpenGL/                    # FOpenGL* backend
├── Plugins/Physics/Jolt/                  # FJoltPhysicsDriver
├── Plugins/Networking/ENet/               # FENetTransport
├── Projects/Sandbox/
├── Tests/
└── ThirdParty/
```

> **Include form:** `#include "Engine/UWorld.hpp"`, `#include "Renderer/FWorldRenderer.hpp"`.  
> Namespace remains `Leon::`. Module folders are short (`Core`, not `LeonCore`).  
> CMake still links one `LeonEngineCore` static lib; Public trees are the modular contract.

> **Naming contract:** UE-style `U`/`A`/`F` prefixes are used without Unreal GC or reflection.
> `AActor` wraps an EnTT entity; render components are POD structs on the registry.
> There is no separate `Scene` / `Entity` type — use `UWorld` + `AActor`.

---

## 3. Project Architecture & Virtual Paths (`.lproject`, `/Game/...`, `/Engine/...`)

### 3.1 Project Descriptor (`.lproject`)
```json
{
  "FileVersion": 1,
  "EngineVersion": "0.15.0",
  "ProjectName": "Sandbox",
  "DefaultMap": "/Game/Maps/ShowcaseLevel",
  "DefaultGameMode": "ASandboxGameMode"
}
```

| Field | Required | Role |
| :--- | :--- | :--- |
| `FileVersion` | yes | Descriptor schema version |
| `EngineVersion` | yes | Informational engine version string |
| `ProjectName` | yes | Used for INI section `/Script/<ProjectName>.GameMode` |
| `DefaultMap` | yes | Fallback if `DefaultEngine.ini` omits `GameDefaultMap` |
| `DefaultGameMode` | yes | Fallback if INI omits `GlobalDefaultGameMode` / `GameModeClass` |

### 3.2 Project Boot Contract

Boot order (`UEngine::InternalRun`):

1. Locate `.lproject` (path as given, directory scan, or walk parents from cwd).
2. Load `FProjectDescriptor` and `FProjectPaths::SetProjectRoot`.
3. Load flat Multi-INI from `<Project>/Config/` (no Base.ini / user Saved hierarchy yet).
4. Create `FApplication` / window from DisplaySettings.
5. Create `UGameInstance` + `UWorld`, load default map (fatal if missing on boot).
6. Spawn GameMode from `UClassRegistry`, apply class config, `InitWorld` / `BeginPlay`.
7. Push viewport layer and run the loop.

**Config priority (highest wins):**

| Setting | Priority |
| :--- | :--- |
| Default map | `DefaultEngine.ini` `GameDefaultMap` → `.lproject` `DefaultMap` |
| GameMode class | `DefaultGame.ini` `/Script/<Project>.GameMode` `GameModeClass` → `GlobalDefaultGameMode` → `.lproject` `DefaultGameMode` |
| Pawn/PC/HUD/… | `/Script/<Project>.GameMode` → `/Script/Engine.GameModeBase` |
| Input | `DefaultInput.ini` only |

This is **not** full Unreal config stacking (`Base.ini` + project + `Saved/Config`). It is a flat per-project Multi-INI.

**Supported INI keys**

| Section | Keys | Applied to |
| :--- | :--- | :--- |
| `/Script/Engine.DisplaySettings` | `WindowTitle`, `WindowWidth`, `WindowHeight`, `VSync`, `Fullscreen` | `FApplication` / `FWindow` |
| `/Script/EngineSettings.GameMapsSettings` | `GameDefaultMap`, `GlobalDefaultGameMode` | Boot map + GameMode class |
| `/Script/Engine.RendererSettings` | `ShadowMapResolution`, `EnablePlanarReflection`, `PlanarReflectionQuality` (`Low`/`Medium`/`High`/`Epic`, default **Epic** = full viewport), optional `PlanarReflectionResolutionScale` (0.25–1.0) | `FWorldRenderer` project defaults (before first create) |
| `/Script/Engine.RendererSettings` | `Exposure`, `SunIntensity` | Documented only — **map skybox / lights own these** (not overwritten) |
| `/Script/Engine.InputSettings` | mouse look + Move* / Sprint keys | `FInputSettings` |
| `/Script/<Project>.GameMode` | `GameModeClass`, `DefaultPawnClass`, … | `FGameModeConfig` |

**Project executable duties:** register RHI driver + project `UClassRegistry` classes **before** `UEngine::Run`. Missing default map on initial boot returns non-zero.

### 3.3 Virtual Path Resolution (`FProjectPaths`)
* `/Game/Maps/ShowcaseLevel` → `<ProjectRoot>/Content/Maps/ShowcaseLevel.lmap`
* `/Game/Materials/M_StudioFloor` → `<ProjectRoot>/Content/Materials/M_StudioFloor.lmat`
* `/Engine/Shaders/PBR_Lit.glsl` → `Engine/Assets/Shaders/PBR_Lit.glsl`

### 3.4 Multi-INI Configuration
1. **`DefaultEngine.ini`**: window, maps, GameMode class, renderer project defaults.
2. **`DefaultGame.ini`**: pawn / PC / HUD / GameState / PlayerState classes.
3. **`DefaultInput.ini`**: `FInputSettings` consumed by pawns.

### 3.5 Entry point
The **project** executable registers the RHI driver and project classes, resolves its `.lproject`
(`--project=` → `LEON_PROJECT` → beside exe → project-local fallback), then calls
`UEngine::Run(args, projectPath)`. There is no `EntryPoint.hpp`. Prefer
`Scripts/run_project.py --project <path>` so the Engine never assumes a product path.

---

## 4. Core Engine Subsystems

### 3.1 FApplication & Lifecycle (`FApplication.hpp`)
* Orchestrates the main loop, frame delta timing (`FTimestep`), and top-level window events.
* Manages the `FLayerStack`, calling `OnUpdate()` and routing input `OnEvent()` through active layers.

### 3.2 Modular FLayer System (`FLayer.hpp`, `FLayerStack.hpp`)
* Allows game logic, debug tools, and UI systems to be isolated into distinct `FLayer` instances.
* Updates flow forward through the stack (`Layer 0 -> Layer N`), while events flow backwards from overlays down to base layers until marked handled (`bHandled = true`).

### 3.3 FWindow & Graphics Context (`FWindow.hpp`, `IGraphicsContext.hpp`)
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

## 7. Multi-Pass World Rendering Pipeline (`FWorldRenderer`)

Runtime path: `UEngine` → viewport layer → `UWorld::OnRender` → `FWorldRenderer::Render`.

Static lighting (offline): `LeonAssetTool bake_lightmaps` → `FLightmass` → `.llightmap`. Runtime sampling is documented in [STATIC_LIGHTING.md](STATIC_LIGHTING.md).

```text
PASS 1: Cascaded Shadow Pass (CSM)
PASS 2: Spot Shadow Pass
PASS 3: Planar Reflection Pass
PASS 4: Main Geometry Pass (HDR RGBA16F) + CPU frustum cull
PASS 5: Atmospheric Skybox Pass
PASS 6: 3D In-World Text Pass
PASS 7: Post-Process (Bloom + Tone Map + FXAA + Gamma)
```

**Planar pass (PASS 3):** mirrored-camera capture into HDR FBOs, not cubemap probes. Floor FBO is always filled when a plane is registered; an optional wall FBO fills only when the camera faces that plane. Games currently register the floor (`n = +Y`, `d = 0`). Materials with `UsePlanarReflection` sample the capture on surfaces near the plane; chrome spheres and other curved metals stay on cubemap IBL. Contract: [RENDERER_CONTRACT.md](RENDERER_CONTRACT.md#planar-reflections).

---

## 8. Diagnostic and Interactive Debugging Subsystem

LeonEngine2 provides direct single-key forensic debugging controls available across all viewport layers:

| Hotkey | Mode / Subsystem | Description |
| :--- | :--- | :--- |
| **`F1`** | **Diagnostics HUD** | Real-time panel rendering FPS, frametimes (CPU/GPU), VRAM allocation, RAM usage, triangle counts, and draw call metrics. |
| **`F2`** | **3D Light Gizmos** | 3D wireframe cones for Spot Lights, bounding attenuation spheres for Point Lights, and directional sunlight vectors. |
| **`F3`** | **Wireframe Toggle** | Toggles polygon rasterization between solid fill and wireframe (`glPolygonMode`). |
| **`F4`** | **Unlit / Albedo** | Isolates raw Base Color texture/scalar without lighting or reflections. |
| **`F5`** | **World Normals** | Displays perturbed normal vectors ($N \cdot 0.5 + 0.5$) with TBN normal map contributions. |
| **`F6`** | **Material Channels** | Cycles sequentially between **Roughness**, **Metallic**, and **Ambient Occlusion (AO)** channels. |
| **`F7`** | **Lighting Isolation** | Cycles: **Dynamic (Lo)** → **Baked only** → **Lightmap irradiance** → **Lightmap UV** → **Dynamic+Baked (no IBL)**. |
| **`F8`** | **Specular IBL & Environment**| Isolates Image-Based Lighting reflections and split-sum environment contributions. |
| **`F9`** | **CSM Cascade Slices** | Visualizes Cascaded Shadow Map splits via false-color (Cascade 0: Red, 1: Green, 2: Blue, 3: Yellow). |
| **`F10`** | **Shadow Occlusion Mask** | Renders the direct shadow occlusion factor ($1.0 = \text{lit}, 0.0 = \text{occluded}$). |
| **`F11`** | **Planar Reflections** | Inspects the mirrored camera offscreen planar reflection framebuffer texture. |
| **`F12`** | **Standard Lit (Default)** | Resets rendering to full multi-light Cook-Torrance PBR composite with IBL and tone mapping. |

---

## 3. LeonEngine2 Gameplay Framework (Unreal Engine Architecture)

LeonEngine2 implements a strict, faithful Unreal Engine gameplay framework architecture:

```text
UEngine
  │
  ├── UGameInstance (High-level persistent game state)
  │
  └── UWorld (Active loaded map)
        │
        ├── AGameModeBase (Match rules, player login flow, spawn rules)
        │     │
        │     ├── AGameStateBase (Global match state, PlayerArray)
        │     │
        │     ├── APlayerController (Player input, possession, camera manager, InputMode)
        │     │     │
        │     │     ├── APlayerState (Persistent player data: Name, ID, Score)
        │     │     ├── APawn / ADefaultPawn (Possessed 6-DOF actor)
        │     │     ├── APlayerCameraManager (Resolves ViewTarget, Pawn camera & Fallback)
        │     │     └── AHUD (Viewport widgets + DrawHUD + PrintString paint)
        │     │           │
        │     │           └── UUserWidget → UWidget tree (UCanvasPanel, UButton, UTextBlock, …)
        │     │
        │     └── AActor* (All world actors with attached UActorComponents)
        │
        └── FWorldRenderer (Consumes UWorld data and renders PBR, CSM, IBL, Post-Processing)
```

### HUD / UI Framework

LeonEngine ships a minimal Unreal-aligned UMG-style UI layer (not full Slate/UMG):

```text
UObject
  ├── UWidget
  │     ├── UUserWidget          (AddToViewport / RemoveFromParent)
  │     ├── UButton              (OnClicked, hover/pressed/disabled)
  │     ├── UTextBlock           (SetText / color / scale / alignment)
  │     ├── UImage
  │     ├── UProgressBar         (percent fill)
  │     └── UPanelWidget
  │           ├── UCanvasPanel   (anchors + offsets; FUILayout helpers)
  │           ├── UHorizontalBox
  │           └── UVerticalBox
  └── AActor
        └── AHUD                 (owned by APlayerController)
```

**Lifecycle**

```text
UWorld → GameMode → PlayerController → HUD → UserWidget → Viewport → FUIRenderer
```

**Critical BeginPlay ordering:** During `UWorld::BeginPlay`, `StartPlay`/`Login` runs with deferred actor `BeginPlay`. PC ↔ HUD wiring (`SetPlayerController`) completes before `AHUD::BeginPlay`, so `CreateWidget` / `AddToViewport` can succeed. Spawning HUD without this deferral called `BeginPlay` inside `SpawnActor` with a null PlayerController (widgets never registered).

**Frame render order**

```text
World 3D → Light gizmos (F2) → AHUD widgets + PrintString → F1 Diagnostics Overlay → Present
```

**Engine vs Sandbox responsibilities**

| Engine | Sandbox project |
|--------|-----------------|
| `AHUD`, widgets, `FUIRenderer`, `PrintString`, `OpenLevel`, InputMode, F1–F12 | `ASandboxGameMode`, `ASandboxHUD`, `USandboxMainMenuWidget`, INI / `.lproject` config |

**PrintString** (`UGameplayStatics::PrintString` / `Leon::PrintString`) queues on-screen debug messages via `FOnScreenDebugMessageManager`. Messages are distinct from `FLog` and are painted inside the viewport by `AHUD::DrawHUD`.

**OpenLevel** (`UGameplayStatics::OpenLevel("/Game/Maps/MyMap")`) requests a safe-frame travel on `UEngine`: EndPlay → Clear old World → Create World → Load `.lmap` (virtual path) → GameMode → Login (PC / Pawn / HUD) → BeginPlay. Sandbox `USandboxMainMenuWidget` uses this to toggle `ShowcaseLevel` ↔ `NightLevel`.

**FInput modes** (`APlayerController`): `SetInputModeGameOnly`, `SetInputModeUIOnly`, `SetInputModeGameAndUI`. GameAndUI allows pawn movement and UI mouse interaction simultaneously.

**Debug hotkeys (coexist with gameplay HUD)**

| Key | Behavior |
|-----|----------|
| F1 | Diagnostics performance overlay (`FDebugOverlay`) |
| Shift+F1 | Gameplay debug: line traces, hitboxes, and colliders (`FDebugRenderer`) |
| F2 | 3D light gizmos (`FDebugRenderer`) |
| F3–F12 | Existing render debug views / wireframe (unchanged) |

### Unreal Naming & Prefix Standards:
- **`U`** = Engine objects, worlds, components, assets, widgets (`UObject`, `UWorld`, `UGameInstance`, `UEngine`, `UWidget`, `UUserWidget`, `UButton`, `UTextBlock`, `UPanelWidget`, `UCanvasPanel`, `UGameplayStatics`, `UClassRegistry`).
- **`A`** = Spawnable world actors (`AActor`, `APawn`, `ADefaultPawn`, `APlayerController`, `APlayerState`, `AGameModeBase`, `AGameStateBase`, `ACameraActor`, `APlayerCameraManager`, `AHUD`).
- **`F`** = Structs and value types (`FTransformComponent`, `FUIRenderer`, `FOnScreenDebugMessage`, `FGameModeConfig`, `FTimestep`).
- **`E`** = Enumerations (`ETextAlignment`, `ESlateVisibility`, `EInputMode`, `EButtonState`, `EShadowFilterMode`).
- **`T`** = Templates and container wrappers (`TRef`, `TScope`).
