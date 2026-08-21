# LeonEngine2 — Editor Polish Plan

Unreal-lite daily loop for a **single developer**: FPS / TPS, multiplayer, OpenGL, maps that run without a dedicated GPU.

Architecture stays **context-driven** (`FEditorContext` + `FEditorSelection` + `FEditorHistory`). This plan does **not** add Material Graph, UMG canvas, Behavior Tree visualizer, or a New Editor Window GLFW host.

Related: [NAMING.md](NAMING.md), [EDITOR_ARCHITECTURE.md](EDITOR_ARCHITECTURE.md), [EDITOR_ROADMAP.md](EDITOR_ROADMAP.md), [ASSET_PIPELINE.md](ASSET_PIPELINE.md).

---

## 0. Naming & layout (hard constraints)

From [NAMING.md](NAMING.md):

| Rule | Apply here |
| :--- | :--- |
| English only in identifiers, comments, logs, and this doc | User-facing chat may stay Spanish; **no Spanish in `Editor/Source/`** |
| Prefixes `F` / `A` / `U` / `I` / `E` / `T` / `b` | New types listed per phase below |
| Parameters `In*` | e.g. `MarkMapDirty(bool bInDirty)` |
| Members PascalCase / `b*`, never `m_` / `s_` | e.g. `bMapDirty`, `PlayWorld` |
| File name = primary type | `FSpawnActorsCommand.hpp` / `.cpp` |
| Editor includes | `#include "Editor/Context/FEditorContext.hpp"` |
| Namespace | `Leon::Editor` for editor types; `Leon` for new gameplay actors |
| Logs | `LE_CORE_INFO` / `LE_CORE_WARN` / `LE_CORE_ERROR` with English messages |

**Do not introduce:** `Scene*` names, unprefixed aliases, `<leon/...>` includes, a second selection owner beside `FEditorContext`.

---

## 1. Goal & acceptance (daily loop)

When this plan is done, a solo FPS/TPS author can:

1. Create / open a project, open a map, see `*` in the title when the map is dirty, **Ctrl+S** always saves (Untitled opens a native save dialog).
2. Place **Cube / Plane / Ramp / Sphere / Cylinder**, lights, **PlayerStart**, **BlockingVolume**, a real **Skybox**, and a real **TriggerVolume** — click or drag onto the **surface under the cursor**, not Y=0.
3. Translate / rotate / scale on the three axes (W/E/R), with grid + snap that does not dirty inactive axes.
4. Assign a static mesh in Details and **see it** (load via `UAssetManager`, same as viewport drop).
5. Import raw FBX/PNG/WAV through **AssetTool** into `.lmesh` / `.lskeletalmesh` / `.ltex` / engine audio — not `fs::copy_file` of authoring formats into Content.
6. Press Play: possess the map GameMode pawn, **mouse captured** for look, **Esc pauses the game**, **Stop** (toolbar or Shift+Esc) ends PIE. Outliner/Details do not mutate `EditorWorld` while `PlayWorld` is on screen.

Outliner hide/lock, spawn undo, and honest Play Settings (no fake New Editor Window) are part of the same loop.

---

## 2. Out of scope (keep the editor light)

Defer until the loop above is boringly reliable. Do **not** start these as polish work:

- Material node graph, UMG canvas, Behavior Tree visualizer
- `EPlayMode::NewEditorWindow` GLFW host (remove or disable the combo until a later product phase)
- Simulate / Possess / Eject / frame step
- Nanite / Lumen / Niagara / World Partition / Sequencer / Blueprints
- Full Unreal transaction system for every property (see Phase 4: spawn + transform + delete only)

---

## 3. Phase map

| Phase | Name | Theme | Suggested order |
| :--- | :--- | :--- | :--- |
| 2 | Daily loop correctness | No data loss, no dangling pointers, no lying buttons | First |
| 3 | Play In Editor (FPS/TPS) | Aim, pause, freeze editor mutation | After 2 |
| 4 | Level blockout | Grid, surface place, volumes, gizmos, spawn undo | After 2 (can overlap 3) |
| 5 | Content honesty | AssetTool import, repath, Place Actors completeness | After 4 |

Phase numbers continue [EDITOR_ROADMAP.md](EDITOR_ROADMAP.md) (0–1 done).

---

## Phase 2 — Daily loop correctness

**Outcome:** The editor never silently drops unsaved work; PIE cannot poison editor selection; Details mesh assignment matches viewport drop.

### 2.1 Single selection source

- Remove (or stop writing) `FEditorApp::SelectedActor`. Panels read `Context.GetSelection().GetPrimarySelectedActor()`.
- Viewport `Draw(...)` must not take a parallel `AActor* InSelectedActor` once Context is always set.
- Menu Edit Undo/Redo must refresh the same way as Ctrl+Z / Ctrl+Y (today the menu path skips the hotkey refresh).

### 2.2 Map dirty flag

On `FEditorContext` (not a new class):

```cpp
void MarkMapDirty(bool bInDirty = true);
[[nodiscard]] bool IsMapDirty() const;
```

Member: `bool bMapDirty = false`.

Set dirty on spawn, delete, duplicate, gizmo commit, Details property commit, Place Actors, World/Project settings that belong on the map. Clear on successful `SaveCurrentMap` and after `LoadMap`.

`FEditorApp::UpdateWindowTitle`: append ` *` when dirty. `LoadMap` / `OpenProject` / close: modal **Save / Don't Save / Cancel** if dirty.

### 2.3 Hotkeys (Unreal-like, English menu labels)

In `FEditorApp::OnUpdate` next to existing Ctrl+Z/Y/D/Delete, gated on `!ImGui::GetIO().WantTextInput`:

| Chord | Action |
| :--- | :--- |
| Ctrl+S | `SaveCurrentMap()` even if `ActiveMapPath` is empty (dialog) |
| Ctrl+P | Project Hub (`bShowProjectHub = true`) |
| Shift+Esc | `StopPlayInEditor()` (Phase 3 also stops using bare Esc) |

Enable File → Save Current Map even when the map is Untitled.

If Content Browser is focused, Delete must **not** call `DeleteSelectedActors` (asset delete stays in the browser).

### 2.4 PIE must not edit PlayWorld as if it were the editor

In `FViewportPanel::Draw`:

- Gate **marquee**, **placement ghost**, and `BeginDragDropTarget` with `!bPlayingInEditor` (pick/gizmo already gated).
- On entering PIE: `Gizmo.CancelInteraction()`, clear marquee flags.

`FEditorApp::OnUpdate`: while `PlaySession.IsPlaying()`, draw Outliner/Details/Place/WorldSettings against **EditorWorld** but **read-only** (or skip Place Actors). Never pass `PlaySession.GetPlayWorld()` into those panels.

`OpenProject` must `StopPlayInEditor()` before swapping descriptor / module / map.

### 2.5 Details loads assets

`FDetailsPanel::DrawStaticMeshComponent` / `DrawMaterialComponent`: on path commit or `CONTENT_BROWSER_ASSET` drop, set `AssetPath` **and** `StaticMesh = UAssetManager::GetStaticMesh(...)` (material equivalent). Same path the viewport drop already uses.

### 2.6 Outliner filter enum vs UI

`EOutlinerFilterCategory` is `{ All, StaticMeshes, Lights, Cameras, Characters, Audio, Volumes }`. The segmented control has **six** labels and names index 5 `"Volumes"` → that index is **Audio**, which falls through `PassesCategoryFilter` to `return true`.

**Fix:** drop unused `Audio` **or** add an Audio tab. Keep enum ordinals aligned with the control. Volumes tab must call the Volumes branch.

Hide/Lock: either honor `HiddenActors` / `LockedActors` in `FViewportPanel` pick + `FWorldRenderer` skip, or remove the eye/lock UI. Do not leave cosmetic toggles.

### New / touched types (Phase 2)

| Type | File | Notes |
| :--- | :--- | :--- |
| (members only) | `Editor/Context/FEditorContext.hpp` | `bMapDirty`, `MarkMapDirty` |
| `EOutlinerFilterCategory` | `Editor/Panels/FOutlinerPanel.hpp` | Align with UI |

No new command types required in this phase.

**Verify:** `python Scripts/verify_ue_naming.py` on touched files.

---

## Phase 3 — Play In Editor (FPS / TPS)

**Status: implemented.** Cursor follows PlayerController GameOnly (GLFW disable), any possessed pawn starts GameOnly, ImGui does not steal capture while the cursor is hidden, Esc stays in-game, Stop is Shift+Esc / toolbar, New Editor Window combo is disabled.

**Outcome:** Play is how you test aim, pause, and listen-server. Stop is explicit.

Keep existing types: `FPlaySession`, `FPlaySettings`, `EPlayNetMode`, `EPlayMode`, `FGameModuleLoader`.

### 3.1 Cursor + input (must match packaged game)

Packaged path: `FGameViewportLayer` applies `APlayerController` cursor visibility to GLFW.

Editor path today never does. Look uses absolute mouse delta → cursor hits the screen edge.

- After PIE `Tick`, if the possessed pawn wants `GameOnly`, call `GetWindow().SetCursorVisible(false)` (and clip/disable cursor while captured). Restore on Stop.
- `FGameplaySession::Start` currently forces `SetInputModeGameOnly()` only when the pawn class is `"ADefaultPawn"`. After BeginPlay, apply GameOnly for **any** possessed pawn (Tournament character included).
- While cursor is hidden, do not let ImGui `WantCaptureMouse` / `WantCaptureKeyboard` eat game `FInput`.

### 3.2 Esc vs Stop

| Input | Behavior |
| :--- | :--- |
| Esc | Game pause / UI (Tournament PC). **Not** Stop PIE |
| Shift+Esc or toolbar Stop | `FPlaySession::RequestStop()` |
| Stop | Restore editor camera, cursor, `Viewport.SetPlayingInEditor(false)` |

### 3.3 Honest Play Settings

`EPlayMode::NewEditorWindow` is stored in `PlaySettings.json` but **never read** by `FPlaySession::Start`.

Until a dedicated host exists: hide the combo **or** force `SelectedViewport` and log a warning. Do not ship a second window mode as polish.

Multi-instance clients (`CreateProcess` + `--pie-role=client`) stay Windows-only; log English if spawn fails.

### 3.4 Game module

`LEON_EDITOR_HAS_TOURNAMENT_MODULE` in-process registration is valid for the current product. Other `.lproject` files keep `FGameModuleLoader::TryLoadDynamicLibrary`. PIE already toasts if unloadable — keep that; do not silently start with `AGameModeBase` + `ADefaultPawn` when the project expected Tournament classes.

**No new types** unless cursor routing needs a small helper in `FEditorApp` (prefer methods, not `FPieCursorSync`).

---

## Phase 4 — Level blockout

**Status: implemented.** Surface placement traces collision then mesh AABB then the grid plane through the camera pivot. XZ grid toggle. Gizmo snap is per active axis. Ramp builtin mesh. `ASkyLight` (one per map) and `ATriggerVolume` (overlap, not blocking). `FSpawnActorsCommand` on Place / drop / Outliner. RMB look disables the cursor.

**Outcome:** Whitebox an arena without fighting the grid or Y=0 placement.

### 4.1 Surface placement

Replace `FViewportPanel::GetWorldRayIntersection` (plane Y=0) with a trace against world collision / mesh AABB, fallback to the editor grid plane through the camera pivot.

Keep the function on `FViewportPanel` (no `FSceneRay` type).

### 4.2 Grid + snap

- Draw an XZ grid in the editor debug pass (engine already has `Engine/Resources/Materials/M_WorldGrid.lmat`). Toggle next to existing Show Flags gizmos (`bShowEditorGizmos` or a sibling `bShowGrid`, default true).
- `FTransformGizmo`: snap **only the active axis**; local translate snaps along `DragAxisDir`, not world XYZ independently.
- Implement unused `EGizmoAxis` plane values (`PlaneXY` / `PlaneXZ` / `PlaneYZ`) and `Uniform` scale **or delete the enum entries**. Do not leave dead Unreal-like names.

### 4.3 Ramp primitive

`FProceduralPrimitiveSpawner::SpawnShape` today: `Cube`, `Sphere`, `Cylinder`, `Plane`.

Add **`Ramp`**: engine builtin `.lmesh` (same pipeline as Cube — author FBX → `Engine/Resources/Meshes/Ramp.lmesh` via AssetTool) and a Place Actors Shapes item `"Ramp"`.

Do not invent `FRampActor`. Spawn stays `AActor` + `FStaticMeshComponent` like other shapes.

### 4.4 Skybox and trigger (no empty actors)

| Place label | Spawn |
| :--- | :--- |
| Sky Light / Skybox | Actor with `FSkyboxComponent` (singleton-aware: warn if one already exists). Prefer a thin `ASkyLight` under `Gameplay/` if maps need a stable class name; file `ASkyLight.hpp` / `.cpp`, register in `UClassRegistry`. |
| Trigger Volume | New `ATriggerVolume` (Unreal-style `A*`), sibling of `ABlockingVolume`: box collision, **not** blocking movement. Files: `Gameplay/ATriggerVolume.hpp` / `.cpp`. Place type `"TriggerVolume"` must hit `SpawnActor<ATriggerVolume>`, not the generic `SpawnActor(InType + "Actor")` fallback. |

Do **not** add `ATriggerVolume` as a stub that still has no overlap. Minimum: `UBoxComponent` with `bBlockMovement = false` and a Details checkbox **Enabled**. Overlap gameplay can stay in game modules.

### 4.5 Spawn undo

New command (file = type):

```text
Editor/Source/Public/Editor/Commands/FSpawnActorsCommand.hpp
Editor/Source/Private/Commands/FSpawnActorsCommand.cpp
```

Reuse `FActorEditorSnapshot` from `FDeleteActorsCommand.hpp` (or move the snapshot struct to `FActorEditorSnapshot.hpp` if both commands need it — aggregation header only if it is a true family). Execute = spawn from snapshot; Undo = destroy. Wire Place Actors, viewport drop, Outliner “spawn here”.

Transform / delete / duplicate already have `FTransformActorsCommand`, `FDeleteActorsCommand`, `FDuplicateActorsCommand`. Do **not** wrap every Details float in a command in this phase.

### 4.6 Gizmo RMB navigation

While `bRmbNavigating`, disable/capture cursor (same idea as PIE) so look does not stop at the monitor edge.

---

## Phase 5 — Content honesty — **Done**

**Outcome:** Content Browser talks native formats. Import is AssetTool, not copy.

### 5.1 Import

`FContentBrowserPanel` import must invoke the existing pipeline (`Scripts/import_assets.py` / `AssetTool import`), async like Lightmass bake (`bBakeRunning` pattern): English lines in Output Log, toast on failure.

Accept into Content: `.lmesh`, `.lskeletalmesh`, `.lskeleton`, `.lanim`, `.ltex`, `.lhdr`, `.lmat`, `.lmap`, project audio formats the runtime already loads. **Reject** leaving raw `.fbx` / `.obj` as the playable mesh.

Viewport drop of `CONTENT_BROWSER_ASSET`: spawn mesh only for `.lmesh` (and skeletal later). Do not spawn from `.fbx`.

### 5.2 Rename / delete references

`FindAssetReferences` must not match path **stems** (`Cube` vs `CubeRed`). Use normalized virtual path equality.

Rename: rewrite `FStaticMeshComponent::AssetPath` / materials on the **active EditorWorld**, then dirty the map. If too risky for v1, disable rename when reference count > 0 and log why.

### 5.3 Project Hub

Keep New Project templates. Ensure Blank project gets an empty `.lmap`, default GameMode, and Place Actors primitives without a game DLL. English logs.

---

## 4. Suggested type checklist (do not invent extras)

| Type | Module | Phase |
| :--- | :--- | :--- |
| `FEditorContext` (`bMapDirty`) | Editor/Context | 2 |
| `EOutlinerFilterCategory` | Editor/Panels | 2 |
| `FPlaySession` / `FPlaySettings` / `EPlayNetMode` / `EPlayMode` | Editor/Play | 3 (behavior only) |
| `FSpawnActorsCommand` : `IEditorCommand` | Editor/Commands | 4 |
| `ASkyLight` (optional, if class name needed) | Gameplay | 4 |
| `ATriggerVolume` | Gameplay | 4 |
| `FProceduralPrimitiveSpawner` (+ `Ramp`) | Gameplay | 4 |

CMake: add new `.cpp` to `LeonEditorCore` and/or `Leon::Gameplay` as appropriate. Run `python Scripts/verify_ue_naming.py`.

---

## 5. Implementation order (solo)

Do not parallelize Phase 2. After 2, Phase 3 and 4 can interleave if PIE is blocked on engine input and blockout is blocked on viewport traces.

1. Dirty + Ctrl+S + Untitled save dialog  
2. PIE gate marquee/drop + stop OpenProject during play  
3. Details `GetStaticMesh` / material load  
4. Outliner filter + hide/lock or remove UI  
5. Drop `SelectedActor` dual state  
6. PIE cursor capture + GameOnly for all pawns + Esc/Stop split  
7. Disable New Editor Window combo  
8. Surface trace + grid  
9. `ATriggerVolume` + skybox spawn + Ramp  
10. `FSpawnActorsCommand`  
11. AssetTool import + drop only native meshes  

Each step should leave the editor **bootable**. Prefer deleting a lying control over shipping a stub actor.

---

## 6. Done when

- Title bar shows `Leon Engine Editor - [Project] - MapName *` iff unsaved.  
- Ctrl+S and File → Save work on Untitled maps.  
- Play: look works with captured mouse; Esc does not destroy the session; Stop restores the editor.  
- Place Ramp / Trigger / Skybox produce usable actors; drop onto floors/ramps.  
- Details mesh field changes the rendered mesh.  
- Import produces `.lmesh` / `.ltex` (etc.), not a raw FBX in `/Game`.  
- `verify_ue_naming.py` passes; no Spanish in new comments or `LE_CORE_*` strings.  
