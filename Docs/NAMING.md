# Naming Conventions — LeonEngine2 (Unreal Engine Standard)

Official naming, directory layout, and coding standards for **LeonEngine2**, aligned with the
**Unreal Engine C++ Coding Standard**. Architecture details: [ARCHITECTURE.md](ARCHITECTURE.md). Product build trees: [BUILD.md](BUILD.md).

---

## 0. Language

- **English only** for identifiers, comments, log messages, and in-repo technical docs.
- Comments must be **relevant**: intent, invariants, trade-offs, non-obvious flows — never narrate the next line.
- Spanish is reserved for user-facing chat only, not for source code.

---

## 1. Type & Symbol Prefixes

| Category | Prefix | Examples |
| :--- | :--- | :--- |
| Engine / UObject-style objects | `U` | `UWorld`, `UEngine`, `UWidget`, `UGameplayStatics` |
| Actors | `A` | `AActor`, `APawn`, `AGameModeBase`, `AHUD` |
| Structs / value types | `F` | `FApplication`, `FWindow`, `FWorldRenderer`, `FTimestep` |
| Interfaces | `I` | `IGraphicsContext`, `IRenderAPI`, `IRenderDriver` |
| Enums | `E` | `EShaderDataType`, `ELightMobility`, `EComponentMobility` |
| Templates / smart pointers | `T` | `TRef<T>`, `TScope<T>` |
| Booleans | `b` | `bRunning`, `bCastShadows`, `bWireframeEnabled` |
| Parameters | `In` + PascalCase | `InDeltaTime`, `InWidth` |
| Members | PascalCase / `b*` | **No** `m_` or `s_` — e.g. `AppWindow`, `NativeWindow`, `bMinimized`, static `Instance` |
| Macros | `LE_` | `LE_CORE_INFO`, `LE_BIND_EVENT_FN` |
| Namespace | `Leon` | `Leon::`, `Leon::Key` |

**Forbidden:** unprefixed aliases (`using Application = FApplication`), legacy `Scene` API names (`FSceneRenderer`, `GetSceneRenderer`). Member names must not collide with their type (`TScope<FWindow> AppWindow`, not `TScope<FWindow> FWindow`).

**File name = primary type:** `UWorld.hpp` / `UWorld.cpp`, `FWorldRenderer.hpp`, `IRenderAPI.hpp`.

### Allowed exceptions

These exist in the tree and must not be mass-renamed without a dedicated sweep. `Scripts/verify_ue_naming.py` allowlists the unprefixed aliases.

| Exception | Why |
| :--- | :--- |
| `template <typename T> using Ref = TRef<T>` and `Scope = TScope<T>` plus `CreateRef` / `CreateScope` in `Core/Base.hpp` | Dual convenience aliases used throughout the engine; canonical names remain `TRef` / `TScope` / `MakeRef` / `MakeScope` |
| `Tick(float DeltaSeconds)` on actors and components | Matches Unreal’s override signature; new non-override parameters still use `In*` |
| Aggregation headers `Components.hpp`, `*Types.hpp`, `*Widgets.hpp` | Intentional bundles; tests skip the file=primary-type check |
| `BIT`, `EVENT_CLASS_TYPE`, `EVENT_CLASS_CATEGORY` | Event-system macros kept beside `LE_*` |
| Headers whose stem is a family (`FApplicationEvent.hpp`, `FBuffer.hpp`, `EMobility.hpp`) | Historical grouping; do not add new mismatches |
| `FWorldRendererGeometry.cpp`, `FWorldRendererLighting.cpp`, `FWorldRendererPostProcess.cpp`, `FWorldRendererInternals.hpp` | Split compilation units of `FWorldRenderer`; do not invent a second renderer type |

---

## 2. Runtime Module Layout (Unreal-style)

```text
Engine/
├── Assets/                          # Engine shaders, fonts, LUTs
└── Source/
    └── Runtime/
        ├── Core/Public/Core/       # FWindow, FLog, FInput, FLayer, …
        ├── Core/Private/
        ├── Engine/Public/Engine/   # UEngine, UWorld, UGameInstance, serializers
        ├── Engine/Private/
        ├── Gameplay/Public/Gameplay/
        ├── Gameplay/Private/
        ├── UMG/Public/UMG/         # UWidget hierarchy, FUIRenderer
        ├── UMG/Private/
        ├── Renderer/Public/Renderer/
        ├── Renderer/Private/
        ├── RHI/Public/RHI/
        ├── RHI/Private/
        ├── Assets/Public/Assets/
        ├── Assets/Private/
        ├── AI/Public/AI/               # UBehaviorTree, UBlackboard*, UAIPerceptionComponent
        ├── AI/Private/
        ├── Audio/Public/Audio/         # FAudioDevice, USoundWave, UAudioComponent
        ├── Audio/Private/
        ├── Physics/Public/Physics/     # IPhysicsScene, FHitResult, FSimplePhysicsScene
        ├── Physics/Private/
        ├── Lightmass/Public/Lightmass/  # FLightmass, FLightBaker, FLightmapBuilder
        └── Lightmass/Private/
```

Plugins (not Runtime modules): `Plugins/RHI/OpenGL` (`FOpenGL*`), `Plugins/Physics/Jolt` (`FJolt*`), `Plugins/Networking/ENet` (`FENetTransport`).

`AAIController` lives under **Gameplay** (it is an `A*` actor). Behavior tree / perception types live under **AI**.

Module folder names are **short** (`Core`, `Engine`, …) — no `Leon` prefix on modules.
CMake exposes aliases `Leon::Core`, `Leon::Engine`, … for linking.
C++ code stays in namespace `Leon`.

Includes:

```cpp
#include "Core/FWindow.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Renderer/FWorldRenderer.hpp"
```

ECS POD components (EnTT): `F*Component` / `FTag`.
`U*Component` only when the type inherits `UActorComponent` / `UObject`.

### Editor product layout

```text
Editor/
├── Source/Public/Editor/
│   ├── FEditorApp.hpp               # Application host
│   ├── Context/                     # FEditorContext, FEditorSelection, FEditorHistory
│   ├── Commands/                    # IEditorCommand
│   ├── Panels/                      # FViewportPanel, FOutlinerPanel, FDetailsPanel, ...
│   ├── Gizmos/                      # FTransformGizmo
│   ├── UI/                          # FEditorTheme, FLucideIcons, FEditorWidgets
│   ├── Utils/                       # FEditorFileDialog
│   └── Window/                      # FEditorWindow
├── Source/Private/                  # matching .cpp + main.cpp
└── Resources/                       # fonts, icons, brand
```

- Same prefixes and `Leon::` / `Leon::Editor` as Runtime.
- Includes: `#include "Editor/FEditorApp.hpp"`, `#include "Editor/Context/FEditorContext.hpp"`, `#include "Editor/Panels/FViewportPanel.hpp"` (via `Editor/Source/Public`).
- Do not introduce new `<leon/...>` or `leon::` symbols in `Editor/Source/`.

---

## 3. Scripts

| Ecosystem | Convention | Example |
| :--- | :--- | :--- |
| Python | `snake_case` | `build_incremental.py`, `verify_ue_naming.py` |
| PowerShell | `VerbNoun` | `BuildIncremental.ps1` |

---

## 4. Assets

| Kind | Convention | Example |
| :--- | :--- | :--- |
| Shaders | PascalCase | `PBR_Lit.glsl`, `PBR_Common.glsl` |
| Textures | `T_<Name>_<Suffix>` | `T_StudioFloor_Color.ltex` |
| Fonts | `Family-Weight` | `Inter-Regular.ttf` |

**Materials:** on-disk `.lmat` assets are loaded into runtime `FMaterial` / `FMaterialInstance` (value types under Renderer — **not** `UObject`). Prefer `F*` for GPU/material data; do not rename to `UMaterial` unless a true UObject asset system is introduced.

---

## 5. Member Variables (Unreal style)

```cpp
// Correct
bool bRunning = true;
TRef<FWindow> MainWindow;
static FApplication* Instance;

// Forbidden
bool m_bRunning;
TRef<FWindow> m_Window;
static FApplication* s_Instance;
```

If a member name collides with a type (`FWindow` vs `FWindow`), prefer a clear UE-style name (`MainWindow`, `GameWindow`).
