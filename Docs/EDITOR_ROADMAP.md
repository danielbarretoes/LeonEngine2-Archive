# LeonEngine2 — Editor Roadmap

## Overview & Architecture

Leon Engine 2 Editor follows an **Unreal Engine–inspired Context-Driven architecture** centered around a unified `FEditorContext` (managing `FEditorSelection` and `FEditorHistory` undo/redo stacks) that coordinates all panels without spaghetti coupling.

- Dedicated out-of-process product under [`Editor/`](../Editor/) with its own CMake entry: `cmake -S Editor -B out/Editor`.
- Single source of truth: `FEditorContext` (`World`, `Selection`, `History`, `ActiveProject`).
- Reuses `FApplication` / `FWindow`, `UWorld`, and `FWorldRenderer` (embedded level viewport).
- Clean Unreal-style naming prefixes (`F*`, `U*`, `A*`, `E*`, `T*`) under namespace `Leon` / `Leon::Editor`.

Build & Run:

```bat
python Scripts/build_editor.py --config Debug
python Scripts/build_editor.py --run
```

---

## Editor Directory Structure

```text
Editor/
├── Source/
│   ├── Public/Editor/
│   │   ├── FEditorApp.hpp               # Out-of-process editor application host
│   │   ├── Context/                     # Single source of truth for editing state
│   │   │   ├── FEditorContext.hpp       # Central coordinator (World, Map, Selection, History)
│   │   │   ├── FEditorSelection.hpp     # Multi-actor and multi-asset selection state
│   │   │   └── FEditorHistory.hpp       # Undo / Redo command stack
│   │   ├── Commands/                    # Reversible action command interface
│   │   │   └── IEditorCommand.hpp       # Base command contract (Execute, Undo)
│   │   ├── Panels/                      # Independent ImGui panels
│   │   │   ├── FViewportPanel.hpp       # 3D interactive viewport + camera + gizmos + drop target
│   │   │   ├── FOutlinerPanel.hpp       # World outliner actor hierarchy tree
│   │   │   ├── FDetailsPanel.hpp        # Property inspector & component editor
│   │   │   ├── FContentBrowserPanel.hpp # Asset browser (Grid/List, search, import, breadcrumbs)
│   │   │   ├── FToolbarPanel.hpp        # Top toolbar actions (Save, Bake, Hub, Run, Layout)
│   │   │   ├── FPlaceActorsPanel.hpp    # Fast actor spawn palette + drag source
│   │   │   ├── FProjectHubPanel.hpp     # Project launcher & recent projects
│   │   │   ├── FOutputLogPanel.hpp      # Console logs & diagnostics
│   │   │   ├── FWorldSettingsPanel.hpp  # Level gravity, gamemode, audio settings
│   │   │   └── FProjectSettingsPanel.hpp# Project descriptor settings (.lproject)
│   │   ├── Gizmos/                      # 3D interactive manipulation tools
│   │   │   └── FTransformGizmo.hpp      # Translate, Rotate, Scale gizmos with snapping
│   │   ├── UI/                          # Reusable UI styling, widgets and vector icons
│   │   │   ├── FEditorTheme.hpp         # Unreal Dark theme & typography loader
│   │   │   ├── FLucideIcons.hpp         # Vector-based Lucide icon rasterizer
│   │   │   └── FEditorWidgets.hpp       # Shared ImGui controls (XYZ colored vectors, search)
│   │   ├── Utils/                       # Platform utilities
│   │   │   └── FEditorFileDialog.hpp    # Native file open/save dialogs
│   │   └── Window/                      # GLFW and docking window orchestration
│   │       └── FEditorWindow.hpp        # Window wrapper and configuration state
│   └── Private/                         # Matching .cpp implementations
└── Resources/                           # Fonts (Inter), icons, brand textures
```

---

## Implemented Phases

### Phase 0 — Build Split & Host Skeleton [DONE]
- Standalone CMake target `LeonEditor` linking against engine modules.
- Window config serialization and Unreal Dark UI theme with Inter typography.

### Phase 1 — Context-Driven Architecture & Core Panels [DONE]
- **FEditorContext**: Centralized coordination of `World`, `Selection`, and `History`.
- **Level Viewport (`FViewportPanel`)**: Unreal-style camera, raycasting actor picking, marquee multi-selection box, 3D translation/rotation/scale gizmo (`FTransformGizmo`), and drag-and-drop actor/mesh spawning.
- **World Outliner (`FOutlinerPanel`)**: Hierarchy tree with category filters (Meshes, Lights, Cameras), visibility/lock toggles, parent/child drag reparenting, and context menus.
- **Details Panel (`FDetailsPanel`)**: Live inspector for Transform (XYZ color badges with reset button via `FEditorWidgets`), Static Mesh, Materials, Lights, Camera, and Collision components.
- **Content Browser (`FContentBrowserPanel`)**: Grid and List modes, thumbnail caching, directory tree, breadcrumb bar, asset search filter, asset creation, and import.
- **Place Actors Palette (`FPlaceActorsPanel`)**: Category tabs (Basic, Lights, Shapes, Volumes) with click-to-spawn and drag-to-viewport spawning.
- **Toolbar & Project Hub (`FToolbarPanel`, `FProjectHubPanel`)**: Top-level actions (Save Map, Bake Lightmass, Launch Game, Reset Layout) and recent projects launcher.

---

## Next Roadmap Phases

1. **Material Graph / Shader Inspector** — visual or node-based material instance authoring with live shader recompilation.
2. **PIE (Play-In-Editor) In-Process Mode** — toggle viewport between Editor Camera and Game Camera with active ticking simulation.
3. **Behavior Tree / AI Blackboard Visualizer** — read/write `.lbt` and view live active execution node highlights during play.
4. **UMG Canvas Editor** — visual drag-and-drop placement of UI widgets (`UCanvasPanel`, `UButton`, `UTextBlock`).
