# Naming Conventions — LeonEngine2 (Unreal Engine Standard)

Official naming, directory layout, and coding standards for **LeonEngine2**, aligned with the
**Unreal Engine C++ Coding Standard**. Architecture details: [ARCHITECTURE.md](ARCHITECTURE.md).

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
        ├── Lightmass/Public/Lightmass/  # FLightmass, FLightBaker, FLightmapBuilder
        └── Lightmass/Private/
```

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
| Shaders | PascalCase | `PBR_Lit.glsl` |
| Textures | `T_<Name>_<Suffix>` | `T_Tiles_N.png` |
| Fonts | `Family-Weight` | `Inter-Regular.ttf` |

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
