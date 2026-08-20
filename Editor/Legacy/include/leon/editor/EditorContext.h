#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <leon/core/Camera.h>
#include <leon/editor/EditorSelection.h>
#include <leon/editor/PieAspectFit.h>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/World.h>
#include <leon/level/Level.h>
#include <leon/level/LevelCatalog.h>
#include <leon/render/ResourceCache.h>
#include <string>
#include <vector>

namespace leon {
class Engine;
class Renderer;
class Window;
} // namespace leon

namespace leon::editor {

class EditorHistory;

enum class EGizmoOperation : std::uint8_t {
    Translate,
    Rotate,
    Scale,
};

enum class EGizmoSpace : std::uint8_t {
    Local,
    World,
};

/// Unreal-like Play mode: Selected Viewport (PIE) or a separate OS game window.
enum class EEditorPlayMode : std::uint8_t {
    SelectedViewport = 0,
    NewEditorWindow = 1,
};

/// Unreal Play → Net Mode (editor PIE / multi-instance).
enum class EEditorPlayNetMode : std::uint8_t {
    Standalone = 0,
    ListenServer = 1,
    Client = 2,
};

enum class EEditorViewMode : std::uint8_t {
    Perspective = 0,
    OrthoTop,
    OrthoFront,
    OrthoSide,
};

/// Unreal-like viewport View Mode (VMI_Lit / VMI_CollisionPawn / VMI_Wireframe).
enum class EEditorViewportViewMode : std::uint8_t {
    Lit = 0,
    PlayerCollision,
    Wireframe,
};

/// Content Browser Filters chips (Unreal-like).
enum class EContentBrowserKindFilter : std::uint8_t {
    All = 0,
    StaticMeshes,
    Materials,
    Textures,
    Blueprints,
    Characters,
    Misc,
};

/// Asset editor tab pending close (Save / Discard / Cancel).
enum class EAssetEditorTabKind : std::uint8_t {
    None = 0,
    Material,
    Blueprint,
    Widget,
};

/// Active class in Place Actors (Modes) — LMB in Viewport commits a place.
enum class EEditorPlaceActorsKind : std::uint8_t {
    None = 0,
    Cube,
    Sphere,
    Plane,
    Cylinder,
    BlockingVolume,
    PointLight,
    SpotLight,
    DirectionalLight,
    TriggerVolume,
    PainCausingVolume,
    PlayerStart,
    AISpawnPoint,
    TextRenderActor,
    BlueprintClass,
};

struct PlaceActorsRecentEntry {
    EEditorPlaceActorsKind kind = EEditorPlaceActorsKind::None;
    std::string blueprintPath;
    std::string label;
};

/// Shared mutable editor state passed to every panel (Unreal editor "context" lite).
struct EditorContext {
    leon::Engine* engine = nullptr;
    leon::Level* level = nullptr;
    leon::World* world = nullptr;
    leon::Camera* camera = nullptr;
    leon::Renderer* renderer = nullptr;
    leon::Window* window = nullptr;
    leon::ResourceCache* resources = nullptr;
    leon::LevelCatalog* catalog = nullptr;
    EditorHistory* history = nullptr;

    /// Primary selection (Details / gizmo). Also mirrored in `selected`.
    EditorSelection selection{};
    std::vector<EditorSelection> selected;

    /// Next id for Level objects (Actors use `World` spawn ids).
    std::uint64_t nextEditorId = 1;

    EGizmoOperation gizmoOp = EGizmoOperation::Translate;
    EGizmoSpace gizmoSpace = EGizmoSpace::Local;

    std::string projectPath;
    std::string projectName;
    std::string projectDisplayName;
    /// From DefaultEngine.ini `GameDefaultMap`, e.g. `Levels/MainMenu.llev`.
    std::string projectGameDefaultMap;
    /// From DefaultEngine.ini `EditorStartupMap`. Empty → use `gameDefaultMap`.
    std::string projectEditorStartupMap;
    /// From DefaultEngine.ini `GlobalDefaultGameMode`.
    std::string projectGlobalDefaultGameMode;
    /// From DefaultGame.ini `[Leon.Project] TemplateId` (Content library used at New Project).
    std::string projectTemplateId;
    std::string projectDescription;
    /// From DefaultGame.ini `[Leon.Project] BuildDedicatedServer`.
    bool projectBuildDedicatedServer = false;

    std::string levelPath;
    bool dirty = false;
    bool requestCloseProject = false;
    bool pendingCloseProject = false;
    bool pendingExit = false;
    bool viewportHovered = false;
    bool viewportFocused = false;
    /// Content Browser consumed Delete/F2 this frame — skip Viewport/Outliner Delete.
    bool contentBrowserFocused = false;
    bool requestFocusSelected = false;
    /// Outliner: begin inline rename for the primary selection (Viewport RMB / F2).
    bool requestRenameSelected = false;

    int viewportWidth = 1;
    int viewportHeight = 1;
    float deltaTime = 1.0f / 60.0f;

    std::string pendingOpenPath;
    bool requestNewLevelDialog = false;
    int pendingNewLevelTemplate = -1;

    std::string previewAssetPath;
    bool requestPreviewReload = false;
    bool requestContentRefresh = false;
    bool requestOpenImportDialog = false;
    /// When set, Import dialog opens with this source and auto-detects mode (cleared on open).
    std::string pendingImportSourcePath;
    std::string contentFilter;
    /// Content Browser kind chips (All | Static Meshes | …).
    EContentBrowserKindFilter contentKindFilter = EContentBrowserKindFilter::All;
    /// Absolute Content paths starred as Favorites (persisted in editor prefs).
    std::vector<std::string> contentFavoritePaths;
    /// Content Browser: navigate to this asset path (folder + selection). Cleared when handled.
    std::string requestRevealContentPath;
    /// Place / gizmo: prefer mesh AABB hit over ground plane when true (End or modifier).
    bool surfaceSnapEnabled = true;

    EEditorPlayMode playMode = EEditorPlayMode::SelectedViewport;
    /// Unreal Play settings: Number of Players (1..4) + Net Mode.
    int pieNumberOfPlayers = 1;
    EEditorPlayNetMode pieNetMode = EEditorPlayNetMode::Standalone;
    /// Address used when Net Mode is Client (and for extra spawned clients).
    std::string pieClientAddress = "127.0.0.1";
    /// PIE frame aspect inside the viewport / play window (default 16:9 letterbox).
    EEditorPieAspect pieAspect = EEditorPieAspect::Fit16x9;
    bool piePlaying = false;
    bool piePaused = false;
    bool requestPieStart = false;
    bool requestPieStop = false;
    bool requestPiePauseToggle = false;
    bool pieNewWindow = false;
    leon::Camera* editorViewCamera = nullptr;
    /// Pack PIE: paint HUD / LevelDirector UI into the play framebuffer (after DrawScene).
    std::function<void(int fbW, int fbH)> piePaintPlayOverlay;

    bool requestBuildLights = false;
    std::string buildLightsStatus;
    /// True after Static mesh transforms change post-bake; cleared by Build Lighting / level open.
    bool lightingOutOfDate = false;
    /// Build menu: cmake-build the open game pack → `<project>/Shipping/`.
    bool requestBuildProject = false;

    bool showGrid = true;
    bool showStats = false;
    bool snapEnabled = true;
    float gridSize = 1.0f;
    float rotationSnapDegrees = 15.0f;
    EEditorViewMode viewMode = EEditorViewMode::Perspective;
    EEditorViewportViewMode viewportViewMode = EEditorViewportViewMode::Lit;

    /// Window menu: rebuild default docking / flush ImGui layout ini.
    bool requestResetLayout = false;
    bool requestSaveLayout = false;

    std::string pendingMaterialPickPath;
    std::string pendingMeshPickPath;
    std::string pendingHdrPickPath;
    bool requestPickMaterial = false;
    bool requestPickMesh = false;
    bool requestPickHdr = false;

    [[nodiscard]] bool IsAssetPickActive() const {
        return requestPickMaterial || requestPickMesh || requestPickHdr;
    }

    void ClearAssetPickMode() {
        requestPickMaterial = false;
        requestPickMesh = false;
        requestPickHdr = false;
        pendingMaterialPickPath.clear();
        pendingMeshPickPath.clear();
        pendingHdrPickPath.clear();
    }

    void BeginPickMaterial() {
        ClearAssetPickMode();
        requestPickMaterial = true;
    }

    void BeginPickMesh() {
        ClearAssetPickMode();
        requestPickMesh = true;
    }

    void BeginPickHdr() {
        ClearAssetPickMode();
        requestPickHdr = true;
    }

    /// Dockable panel visibility (Window menu). Closed with the window X; reopen from Window.
    bool showConsole = true;
    bool showOutputLog = true;
    /// Asset editors stay closed until opened from Content Browser / Window menu.
    bool showMaterialEditor = false;
    bool showBlueprintEditor = false;
    bool showWidgetDesigner = false;
    bool showStaticMeshEditor = false;
    bool showContentBrowser = true;
    bool showAssetPreview = false;
    bool showWorldSettings = true;
    bool showProjectSettings = false;
    bool showOutliner = true;
    bool showDetails = true;
    bool showPlaceActors = true;
    bool requestFocusConsole = false;
    bool requestFocusOutputLog = false;
    bool requestFocusPlaceActors = false;
    bool requestFocusMaterialEditor = false;
    bool requestFocusBlueprintEditor = false;
    bool requestFocusWidgetDesigner = false;
    bool requestFocusStaticMeshEditor = false;

    /// Place Actors (Modes): non-None arms LMB place in the Viewport; Esc clears.
    EEditorPlaceActorsKind placeActorsKind = EEditorPlaceActorsKind::None;
    std::string placeActorsBlueprintPath;
    std::vector<PlaceActorsRecentEntry> placeActorsRecent;
    /// Double-click `.lmat` / Console `openmat` → Material Editor tab.
    std::string requestOpenMaterialPath;
    /// Double-click `.lmesh` → Static Mesh Editor (dockable).
    std::string requestOpenMeshPreviewPath;
    /// Double-click `.lbp` → Blueprint Editor tab.
    std::string requestOpenBlueprintPath;
    /// Double-click `.luw` → Widget Designer tab.
    std::string requestOpenWidgetPath;

    /// Unreal-like Save / Save All (File menu, Content Browser, Ctrl+S / Ctrl+Shift+S).
    bool requestSaveCurrent = false;
    bool requestSaveAll = false;
    /// Content Browser: save this asset path (`.llev` or `.lmat`) if dirty.
    std::string requestSaveAssetPath;
    /// Filled each frame by MaterialEditorPanel for Content Browser dirty markers (*).
    std::vector<std::string> dirtyMaterialPaths;
    std::vector<std::string> dirtyBlueprintPaths;
    std::vector<std::string> dirtyWidgetPaths;

    /// Close Asset Tab modal (Unreal Save / Discard / Cancel).
    std::string pendingCloseAssetTabPath;
    EAssetEditorTabKind pendingCloseAssetTabKind = EAssetEditorTabKind::None;

    /// Content Browser selection (F2 Rename / Delete key). Absolute pack Content path.
    std::string contentBrowserSelectedPath;
    /// Absolute folder currently open in the Content Browser (import destination).
    std::string contentBrowserFolder;
    /// Asset Tools requests (processed by EditorLayout).
    std::string requestRenameAssetPath;
    std::string pendingRenameAssetName; // base name without extension
    std::string requestMoveAssetPath;
    std::string requestMoveAssetDestFolder;
    std::string requestDeleteAssetPath;
    /// Batch delete from Content Browser multi-select (processed after single-path delete).
    std::vector<std::string> requestDeleteAssetPaths;
    /// Batch duplicate from Content Browser (processed by EditorLayout).
    std::vector<std::string> requestDuplicateAssetPaths;
    /// File → Fix Up Redirectors (rewrite referencers, delete `.lredirect` stubs).
    bool requestFixUpRedirectors = false;
    /// Material Editor: remap open doc path after Rename/Move/Delete (empty To = close).
    std::string requestMaterialEditorRemapFrom;
    std::string requestMaterialEditorRemapTo;

    void MarkDirty() { dirty = true; }

    /// Mark BuiltData stale after Static mesh transform edits (toast on Play / Save).
    void MarkLightingOutOfDate() { lightingOutOfDate = true; }

    void BeginPlaceActors(EEditorPlaceActorsKind kind, std::string blueprintPath = {}) {
        placeActorsKind = kind;
        placeActorsBlueprintPath = std::move(blueprintPath);
        showPlaceActors = true;
    }

    void ClearPlaceActors() {
        placeActorsKind = EEditorPlaceActorsKind::None;
        placeActorsBlueprintPath.clear();
    }

    [[nodiscard]] bool IsPlaceActorsActive() const {
        return placeActorsKind != EEditorPlaceActorsKind::None;
    }

    /// Unreal Browse to Asset: focus Content Browser on `path` (abs or pack-relative).
    void RevealInContentBrowser(std::string path) {
        if (path.empty()) {
            return;
        }
        requestRevealContentPath = std::move(path);
        showContentBrowser = true;
    }

    [[nodiscard]] bool IsContentFavorite(const std::string& path) const {
        return std::find(contentFavoritePaths.begin(), contentFavoritePaths.end(), path) !=
               contentFavoritePaths.end();
    }

    void ToggleContentFavorite(const std::string& path) {
        if (path.empty()) {
            return;
        }
        const auto it = std::find(contentFavoritePaths.begin(), contentFavoritePaths.end(), path);
        if (it != contentFavoritePaths.end()) {
            contentFavoritePaths.erase(it);
        } else {
            contentFavoritePaths.push_back(path);
        }
    }

    [[nodiscard]] std::uint64_t AllocateEditorId() { return nextEditorId++; }

    [[nodiscard]] leon::Actor* FindActorByIndex(std::size_t index) const {
        if (world == nullptr) {
            return nullptr;
        }
        std::size_t i = 0;
        leon::Actor* found = nullptr;
        world->ForEachActor([&](leon::Actor& actor) {
            if (i == index) {
                found = &actor;
            }
            ++i;
        });
        return found;
    }

    /// Assign a session id on the Level/Actor object at `index` if missing; return it.
    [[nodiscard]] std::uint64_t EnsureEditorId(EEditorSelectionKind kind, std::size_t index) {
        if (kind == EEditorSelectionKind::Actor) {
            leon::Actor* actor = FindActorByIndex(index);
            if (actor == nullptr) {
                return 0;
            }
            if (actor->GetEditorId() == 0) {
                actor->SetEditorId(AllocateEditorId());
            }
            return actor->GetEditorId();
        }
        std::uint64_t* idPtr = LevelEditorIdPtr(kind, index);
        if (idPtr == nullptr) {
            return 0;
        }
        if (*idPtr == 0) {
            *idPtr = AllocateEditorId();
        }
        return *idPtr;
    }

    void ClearSelection() {
        selection.Clear();
        selected.clear();
    }

    [[nodiscard]] bool IsSelected(EEditorSelectionKind kind, std::size_t index) const {
        const std::uint64_t id = PeekEditorId(kind, index);
        for (const EditorSelection& s : selected) {
            if (id != 0 && s.id != 0) {
                if (s.EqualsId(kind, id)) {
                    return true;
                }
            } else if (s.Equals(kind, index)) {
                return true;
            }
        }
        if (id != 0 && selection.id != 0) {
            return selection.EqualsId(kind, id);
        }
        return selection.Equals(kind, index);
    }

    /// Select without moving the camera (use View → Focus / F for framing).
    void Select(EEditorSelectionKind kind, std::size_t index, bool additive = false) {
        const std::uint64_t id = EnsureEditorId(kind, index);
        if (!additive) {
            selection = EditorSelection{kind, index, id};
            selected.clear();
            if (selection.IsValid()) {
                selected.push_back(selection);
            }
            return;
        }
        if (IsSelected(kind, index)) {
            selected.erase(std::remove_if(selected.begin(), selected.end(),
                                          [&](const EditorSelection& s) {
                                              if (id != 0 && s.id != 0) {
                                                  return s.EqualsId(kind, id);
                                              }
                                              return s.Equals(kind, index);
                                          }),
                           selected.end());
            if ((id != 0 && selection.id == id) || selection.Equals(kind, index)) {
                selection = selected.empty() ? EditorSelection{} : selected.back();
            }
        } else {
            selection = EditorSelection{kind, index, id};
            selected.push_back(selection);
        }
    }

    /// Refresh `index` from stable `id` after spawn/delete/reorder. Drops stale entries.
    void ResolveSelectionIndices() {
        auto resolveOne = [this](EditorSelection& s) -> bool {
            if (!s.IsValid()) {
                return false;
            }
            if (s.id == 0) {
                s.id = EnsureEditorId(s.kind, s.index);
                if (s.kind == EEditorSelectionKind::Actor) {
                    return FindActorByIndex(s.index) != nullptr;
                }
                return LevelEditorIdPtr(s.kind, s.index) != nullptr;
            }
            const std::size_t found = FindIndexByEditorId(s.kind, s.id);
            if (found == static_cast<std::size_t>(-1)) {
                s.Clear();
                return false;
            }
            s.index = found;
            return true;
        };

        if (!resolveOne(selection)) {
            selection.Clear();
        }
        std::vector<EditorSelection> kept;
        kept.reserve(selected.size());
        for (EditorSelection s : selected) {
            if (resolveOne(s)) {
                kept.push_back(s);
            }
        }
        selected = std::move(kept);
        if (selection.IsValid()) {
            bool primaryInList = false;
            for (const EditorSelection& s : selected) {
                if (s == selection) {
                    primaryInList = true;
                    break;
                }
            }
            if (!primaryInList) {
                selected.push_back(selection);
            }
        } else if (!selected.empty()) {
            selection = selected.back();
        }
    }

    void RequestFocusSelected() { requestFocusSelected = true; }

private:
    [[nodiscard]] std::uint64_t* LevelEditorIdPtr(EEditorSelectionKind kind, std::size_t index) {
        if (level == nullptr) {
            return nullptr;
        }
        switch (kind) {
        case EEditorSelectionKind::StaticMesh:
            if (index < level->StaticMeshes().size()) {
                return &level->StaticMeshes()[index].editorId;
            }
            break;
        case EEditorSelectionKind::DirectionalLight:
            if (index < level->DirectionalLights().size()) {
                return &level->DirectionalLights()[index].editorId;
            }
            break;
        case EEditorSelectionKind::PointLight:
            if (index < level->PointLights().size()) {
                return &level->PointLights()[index].editorId;
            }
            break;
        case EEditorSelectionKind::SpotLight:
            if (index < level->SpotLights().size()) {
                return &level->SpotLights()[index].editorId;
            }
            break;
        case EEditorSelectionKind::PlayerStart:
            if (index < level->PlayerStarts().size()) {
                return &level->PlayerStarts()[index].editorId;
            }
            break;
        case EEditorSelectionKind::TriggerVolume:
            if (index < level->TriggerVolumes().size()) {
                return &level->TriggerVolumes()[index].editorId;
            }
            break;
        case EEditorSelectionKind::PainCausingVolume:
            if (index < level->PainCausingVolumes().size()) {
                return &level->PainCausingVolumes()[index].editorId;
            }
            break;
        case EEditorSelectionKind::AISpawnPoint:
            if (index < level->AISpawnPoints().size()) {
                return &level->AISpawnPoints()[index].editorId;
            }
            break;
        case EEditorSelectionKind::TextRenderActor:
            if (index < level->TextRenderActors().size()) {
                return &level->TextRenderActors()[index].editorId;
            }
            break;
        case EEditorSelectionKind::BlueprintInstance:
            if (index < level->BlueprintInstances().size()) {
                return &level->BlueprintInstances()[index].editorId;
            }
            break;
        default:
            break;
        }
        return nullptr;
    }

    [[nodiscard]] const std::uint64_t* LevelEditorIdPtr(EEditorSelectionKind kind,
                                                        std::size_t index) const {
        return const_cast<EditorContext*>(this)->LevelEditorIdPtr(kind, index);
    }

    [[nodiscard]] std::uint64_t PeekEditorId(EEditorSelectionKind kind, std::size_t index) const {
        if (kind == EEditorSelectionKind::Actor) {
            const leon::Actor* actor = FindActorByIndex(index);
            return actor != nullptr ? actor->GetEditorId() : 0;
        }
        const std::uint64_t* idPtr = LevelEditorIdPtr(kind, index);
        return idPtr != nullptr ? *idPtr : 0;
    }

    [[nodiscard]] std::size_t FindIndexByEditorId(EEditorSelectionKind kind,
                                                  std::uint64_t id) const {
        if (id == 0) {
            return static_cast<std::size_t>(-1);
        }
        if (kind == EEditorSelectionKind::Actor && world != nullptr) {
            if (leon::Actor* actor = world->FindActorByEditorId(id)) {
                std::size_t i = 0;
                std::size_t found = static_cast<std::size_t>(-1);
                world->ForEachActor([&](leon::Actor& a) {
                    if (&a == actor) {
                        found = i;
                    }
                    ++i;
                });
                return found;
            }
            return static_cast<std::size_t>(-1);
        }
        if (level == nullptr) {
            return static_cast<std::size_t>(-1);
        }
        switch (kind) {
        case EEditorSelectionKind::StaticMesh:
            for (std::size_t i = 0; i < level->StaticMeshes().size(); ++i) {
                if (level->StaticMeshes()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::DirectionalLight:
            for (std::size_t i = 0; i < level->DirectionalLights().size(); ++i) {
                if (level->DirectionalLights()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::PointLight:
            for (std::size_t i = 0; i < level->PointLights().size(); ++i) {
                if (level->PointLights()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::SpotLight:
            for (std::size_t i = 0; i < level->SpotLights().size(); ++i) {
                if (level->SpotLights()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::PlayerStart:
            for (std::size_t i = 0; i < level->PlayerStarts().size(); ++i) {
                if (level->PlayerStarts()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::TriggerVolume:
            for (std::size_t i = 0; i < level->TriggerVolumes().size(); ++i) {
                if (level->TriggerVolumes()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::PainCausingVolume:
            for (std::size_t i = 0; i < level->PainCausingVolumes().size(); ++i) {
                if (level->PainCausingVolumes()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::AISpawnPoint:
            for (std::size_t i = 0; i < level->AISpawnPoints().size(); ++i) {
                if (level->AISpawnPoints()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::TextRenderActor:
            for (std::size_t i = 0; i < level->TextRenderActors().size(); ++i) {
                if (level->TextRenderActors()[i].editorId == id) {
                    return i;
                }
            }
            break;
        case EEditorSelectionKind::BlueprintInstance:
            for (std::size_t i = 0; i < level->BlueprintInstances().size(); ++i) {
                if (level->BlueprintInstances()[i].editorId == id) {
                    return i;
                }
            }
            break;
        default:
            break;
        }
        return static_cast<std::size_t>(-1);
    }
};

} // namespace leon::editor
