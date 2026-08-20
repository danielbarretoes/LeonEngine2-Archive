# LeonEngine2 — Editor Architecture & User Guide

The **Leon Engine Editor (`LeonEditor`)** is an out-of-process, Unreal Engine–inspired visual suite built with modern C++20 and Dear ImGui Docking.

---

## 1. Architectural Principles

### 1.1 Decoupled Editor vs Game Windows
Unlike embedded in-game debug overlays, `LeonEditor` runs as an independent executable with its own dedicated desktop window management:

```
LeonEngine/
  ├── Engine/Source/Runtime/Core/
  │    └── FWindow                --> Game runtime window (fixed/target aspect ratios, game scaling)
  └── Editor/Source/
       └── Window/
            ├── FEditorWindow.hpp  --> Dedicated desktop window for the editor suite
            └── FEditorWindow.cpp  --> Free resizing, OS maximize, geometry persistence (EditorWindow.ini)
```

- **Free OS Maximizing & Resizing**: Full support for 4K, 1440p, Ultrawide, and multi-monitor workstations.
- **Window Geometry Persistence**: Stores window position, size, and maximize state in `Editor/Saved/EditorWindow.ini`.
- **Dynamic Title Formatting**: `Leon Engine Editor - [<ProjectName>] - <MapName>`.

---

## 2. Panels and Default Docking Layout

The editor initializes with an Unreal Engine–style layout with zero empty margins:

```
+---------------------------------------------------------------------------------------+
|  TOOLBAR (Project Browser | Save Map | Bake Draft | Bake Prod | Play Game | Reset)    |
+-------------------+---------------------------------------+---------------------------+
|                   |                                       |  WORLD OUTLINER           |
|                   |                                       |  [World Settings Tab]     |
|                   |                                       |  [Project Settings Tab]   |
|  PLACE ACTORS     |               VIEWPORT                +---------------------------+
|  (Basic, Lights,  |               (Center)                |                           |
|   Shapes, Volumes)|                                       |  DETAILS (Inspector)      |
|                   |                                       |                           |
+-------------------+---------------------------------------+---------------------------+
|                          CONTENT BROWSER  |  OUTPUT LOG                               |
|                          (Shared Bottom Tab Bar)                                      |
+---------------------------------------------------------------------------------------+
```

### 2.1 Panel Breakdown

| Panel | Class | Description |
| :--- | :--- | :--- |
| **Welcome / Project Hub** | `FProjectHubPanel` | Standalone launcher screen for opening recent projects, creating new projects with templates, or browsing disk with native Windows Explorer dialogs (`FEditorFileDialog`). |
| **Main Toolbar** | `FToolbarPanel` | Quick-access actions: Project Browser, Save Map, Lightmass Baking (Draft/Production), Play Game (PIE), and Reset Layout. |
| **Viewport** | `FViewportPanel` | 3D interactive viewport running `FWorldRenderer` pipeline with shading modes (Lit, Wireframe, Normal, Lighting Only), camera speeds, and stats overlay. |
| **World Outliner** | `FOutlinerPanel` | Actor hierarchy tree with Lucide icons per component type, Drag & Drop parenting, and context menus (Duplicate, Detach, Delete, Focus). |
| **Details / Inspector** | `FDetailsPanel` | Property inspector for transforms (Local vs World space switch), static meshes, lights, and cameras. |
| **Place Actors** | `FPlaceActorsPanel` | Palette for instant actor spawning across categories: Basic, Lights, Shapes, and Volumes. |
| **Content Browser** | `FContentBrowserPanel` | Project asset browser (`/Game/Content`) with category cards and Lucide vector icons (`.lmap`, `.lmat`, `.lmesh`, `.ltex`, `.lhdr`). |
| **Output Log** | `FOutputLogPanel` | Filterable console for engine log messages (Info, Warning, Error) with search and autoscroll. |
| **World Settings** | `FWorldSettingsPanel` | Level-specific configuration (default game modes, gravity, environmental lighting). |
| **Project Settings** | `FProjectSettingsPanel` | `.lproject` metadata inspector (Project Name, default maps, game modes). |

---

## 3. Actor Hierarchy & Coordinate Systems

Actors support Unreal-style parent-child attachments:
- `AttachToActor(AActor* InParent)`: Attaches the actor as a child of another actor.
- `DetachFromActor()`: Detaches the actor to the world root level.
- **Local vs World Transform Space**: When an actor has a parent, the Details panel allows toggling between **World Space** (absolute world location) and **Local Space** (relative offset to parent).
- **Outliner Drag & Drop**: Dragging any actor onto another in the World Outliner attaches it as a child.

---

## 4. Typography, Iconography & Styling

- **Typography (`FEditorTheme`)**: High-legibility Inter font (`Inter-Regular.ttf`, `Inter-Bold.ttf`) loaded dynamically across binary and source paths.
- **Color Palette**: Dark theme matching Unreal Engine 5 design tokens (`#1c1c1f` surface, `#007acc` accents).
- **Vector Icons (`FLucideIcons`)**: Pure vector rendering into `ImDrawList` for crystal-clear icons at all display scaling levels without blurry texture artifacts.

---

## 5. Building and Running

```powershell
# Build Editor (Ninja + MSVC)
python Scripts/build_editor.py --config Debug

# Launch Editor
python Scripts/run_editor.py

# Format and verify conventions
python Scripts/verify_ue_naming.py
python Scripts/format_code.py
```
