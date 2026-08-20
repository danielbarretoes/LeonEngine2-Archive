#include <glm/vec3.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <iterator>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorBuild.h>
#include <leon/editor/EditorCommands.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorLayout.h>
#include <leon/editor/EditorLevelFactory.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/LevelSaver.h>
#include <leon/level/LightmapBaker.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/Engine.h>
#include <leon/level/BasicShape.h>
#include <leon/level/LevelLoader.h>
#include <leon/level/Light.h>
#include <string>

namespace leon::editor {

std::string EditorLayout::LayoutIniPath() {
    return (ExecutableDirectory() / "editor_layout.ini").lexically_normal().string();
}

bool EditorLayout::HasUsableSavedLayout() {
    const std::filesystem::path path = LayoutIniPath();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return false;
    }
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // A usable layout must include docking tree data, not only floating [Window] blocks.
    return contents.find("[Docking][Data]") != std::string::npos &&
           contents.find("DockSpace") != std::string::npos;
}

void EditorLayout::ApplyDefaultDockLayout(ImGuiID dockspaceId) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 size = (viewport != nullptr && viewport->WorkSize.x > 1.0f)
                            ? viewport->WorkSize
                            : ImVec2(1280.0f, 720.0f);

    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    // Layout: large Viewport | right column (Outliner/World Settings + Details)
    //         bottom: Content Browser / Output Log (+ other utility tabs).
    ImGuiID dockMain = dockspaceId;
    ImGuiID dockRight =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.22f, nullptr, &dockMain);
    ImGuiID dockBottom =
        ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.25f, nullptr, &dockMain);
    ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.07f, nullptr, &dockMain);

    ImGuiID dockRightTop = dockRight;
    ImGuiID dockRightBottom =
        ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.50f, nullptr, &dockRightTop);

    ImGui::DockBuilderDockWindow("Toolbar", dockTop);
    ImGui::DockBuilderDockWindow("Viewport", dockMain);
    ImGui::DockBuilderDockWindow("World Outliner", dockRightTop);
    ImGui::DockBuilderDockWindow("Place Actors", dockRightTop);
    ImGui::DockBuilderDockWindow("World Settings", dockRightTop);
    ImGui::DockBuilderDockWindow("Details", dockRightBottom);
    ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
    ImGui::DockBuilderDockWindow("Output Log", dockBottom);
    ImGui::DockBuilderDockWindow("Console", dockBottom);
    ImGui::DockBuilderDockWindow("Asset Preview", dockBottom);
    ImGui::DockBuilderDockWindow("Static Mesh Editor", dockBottom);
    ImGui::DockBuilderDockWindow("Material Editor", dockBottom);
    ImGui::DockBuilderDockWindow("Blueprint Editor", dockBottom);
    ImGui::DockBuilderDockWindow("Widget Designer", dockBottom);
    ImGui::DockBuilderDockWindow("Project Settings", dockRightTop);
    ImGui::DockBuilderFinish(dockspaceId);
    dockAssetEditors_ = dockBottom;
}

void EditorLayout::PromoteAssetEditorDock(bool promote) {
    if (!promote) {
        return;
    }
    ImGuiID dockTarget = 0;
    if (ImGuiWindow* anchor = ImGui::FindWindowByName("Content Browser")) {
        if (anchor->DockId != 0) {
            dockTarget = anchor->DockId;
        }
    }
    if (dockTarget == 0) {
        dockTarget = dockAssetEditors_;
    }
    if (dockTarget != 0) {
        ImGui::SetNextWindowDockID(dockTarget, ImGuiCond_Always);
    }
}

void EditorLayout::SetupDefaultDocking(EditorContext& ctx) {
    const ImGuiID dockspaceId = ImGui::GetID("LeonEditorDockspace");

    if (ctx.requestResetLayout) {
        ctx.requestResetLayout = false;
        dockingConfigured_ = false;
        forceDefaultLayout_ = true;
        saveDefaultLayoutFrames_ = -1;
        // Asset editors must not reappear as empty floating shells after reset.
        ctx.showMaterialEditor = false;
        ctx.showBlueprintEditor = false;
        ctx.showWidgetDesigner = false;
        ctx.showStaticMeshEditor = false;
        ctx.showProjectSettings = false;
        ctx.showAssetPreview = false;
        ResetAssetEditors(ctx);
    }

    // Wait until the host viewport has a real size — applying DockBuilder on a 0×0
    // first frame (welcome → editor) leaves every panel floating / scrambled.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = viewport != nullptr ? viewport->WorkSize : ImVec2(0, 0);
    const bool sizeReady = workSize.x > 200.0f && workSize.y > 200.0f;

    if (!dockingConfigured_ && sizeReady) {
        const bool useSaved = !forceDefaultLayout_ && HasUsableSavedLayout();
        forceDefaultLayout_ = false;
        if (!useSaved) {
            ApplyDefaultDockLayout(dockspaceId);
            // Persist once windows have been submitted into the dock nodes.
            saveDefaultLayoutFrames_ = 2;
        }
        dockingConfigured_ = true;
    }

    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
}

bool EditorLayout::OpenLevel(EditorContext& ctx, const std::string& path) {
    if (ctx.engine == nullptr || path.empty()) {
        return false;
    }
    if (ctx.piePlaying) {
        ctx.requestPieStop = true;
        ctx.pendingOpenPath = path;
        return false;
    }
    if (HasUnsavedPackages(ctx)) {
        ctx.pendingOpenPath = path;
        return false;
    }
    if (!LoadLevelFile(*ctx.engine, path)) {
        return false;
    }
    ctx.level = &ctx.engine->GetLevel();
    ViewportPanel::FocusLevelBounds(ctx);
    ctx.levelPath = path;
    ctx.dirty = false;
    ctx.lightingOutOfDate = false;
    ctx.ClearSelection();
    ctx.requestContentRefresh = true;
    if (ctx.history != nullptr) {
        ctx.history->Clear();
        ctx.history->Capture(ctx);
    }
    return true;
}

bool EditorLayout::SaveLevel(EditorContext& ctx, bool saveAs) {
    if (ctx.level == nullptr || ctx.camera == nullptr) {
        return false;
    }
    const std::string previousPath = ctx.levelPath;
    std::string path = ctx.levelPath;
    if (path.empty() || saveAs) {
        const std::string picked =
            EditorPickSaveFile("Leon Level\0*.llev\0All\0*.*\0", "Save Level As",
                               path.empty() ? "level.llev" : path.c_str());
        if (picked.empty()) {
            return false;
        }
        path = picked;
    }
    for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
        if (mesh.tag == "Lava" || mesh.tag.rfind("Door:", 0) == 0) {
            std::cerr << "Editor: legacy mesh tag \"" << mesh.tag
                      << "\" — prefer TriggerVolume / PainCausingVolume PODs\n";
        }
    }
    RemapLevelPathsToContentRelative(ctx);
    if (!saveLevelFile(path, *ctx.level, *ctx.camera)) {
        std::cerr << "Editor: failed to save " << path << '\n';
        return false;
    }
    // Save As: keep original BuiltData and copy sidecar beside the new stem.
    if (!previousPath.empty() && previousPath != path) {
        CopyMapBuiltDataBesideLevel(previousPath, path);
    }
    ctx.levelPath = path;
    ctx.dirty = false;
    if (ctx.catalog != nullptr && !ctx.projectPath.empty()) {
        (void)ctx.catalog->ScanPack(ctx.projectPath);
    }
    std::cout << "Editor: saved " << path << '\n';
    if (ctx.lightingOutOfDate) {
        EditorToast("Lighting out of date — build lighting to refresh baked map data",
                    EEditorToastKind::Warning, 4.0f);
    }
    return true;
}

bool EditorLayout::HasUnsavedPackages(const EditorContext& ctx) const {
    return ctx.dirty || materialEditor_.HasDirtyDocs() || blueprintEditor_.HasDirtyDocs() ||
           widgetDesigner_.HasDirtyDocs();
}

void EditorLayout::ResetAssetEditors(EditorContext& ctx) {
    materialEditor_ = MaterialEditorPanel{};
    blueprintEditor_ = BlueprintEditorPanel{};
    widgetDesigner_ = WidgetDesignerPanel{};
    staticMeshPreview_ = StaticMeshPreviewPanel{};
    ctx.showMaterialEditor = false;
    ctx.showBlueprintEditor = false;
    ctx.showWidgetDesigner = false;
    ctx.showStaticMeshEditor = false;
    ctx.dirtyMaterialPaths.clear();
    ctx.dirtyBlueprintPaths.clear();
    ctx.dirtyWidgetPaths.clear();
    ctx.requestOpenMaterialPath.clear();
    ctx.requestOpenBlueprintPath.clear();
    ctx.requestOpenWidgetPath.clear();
    ctx.requestOpenMeshPreviewPath.clear();
    ctx.previewAssetPath.clear();
    ctx.pendingCloseAssetTabPath.clear();
    ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::None;
}

void EditorLayout::DiscardUnsavedPackages(EditorContext& ctx) {
    // Flow: Unreal Discard packages
    // 1. Clear level / package dirty sets and asset pick mode
    // 2. Drop editor tabs without writing (Material / Blueprint / Widget / Static Mesh)
    ctx.dirty = false;
    ctx.lightingOutOfDate = false;
    ctx.ClearAssetPickMode();
    ResetAssetEditors(ctx);
}

bool EditorLayout::SaveCurrent(EditorContext& ctx) {
    // Flow: Unreal-like Save (Ctrl+S)
    // 1. Focused dirty Material / Blueprint / Widget editor
    // 2. Else dirty current level
    if (materialEditor_.HasFocusedDirtyDoc()) {
        return materialEditor_.SaveFocused(ctx);
    }
    if (blueprintEditor_.HasFocusedDirtyDoc()) {
        return blueprintEditor_.SaveFocused(ctx);
    }
    if (widgetDesigner_.HasFocusedDirtyDoc()) {
        return widgetDesigner_.SaveFocused(ctx);
    }
    if (ctx.dirty) {
        return SaveLevel(ctx, false);
    }
    return true;
}

bool EditorLayout::SaveAll(EditorContext& ctx) {
    // Flow: Unreal-like Save All (Ctrl+Shift+S)
    bool ok = materialEditor_.SaveAll(ctx);
    ok = blueprintEditor_.SaveAll(ctx) && ok;
    ok = widgetDesigner_.SaveAll(ctx) && ok;
    if (ctx.dirty) {
        ok = SaveLevel(ctx, false) && ok;
    }
    return ok;
}

void EditorLayout::DrawPlaceActorsMenu(EditorContext& ctx) {
    if (!ImGui::BeginMenu("Place Actors")) {
        return;
    }
    if (ImGui::MenuItem("Place Actors Panel")) {
        ctx.showPlaceActors = true;
        ctx.requestFocusPlaceActors = true;
    }
    ImGui::Separator();
    ImGui::TextDisabled("Arm Modes (click Viewport to place)");
    auto arm = [&](EEditorPlaceActorsKind kind, const char* label) {
        if (ImGui::MenuItem(label)) {
            ctx.BeginPlaceActors(kind);
            ctx.requestFocusPlaceActors = true;
        }
    };
    arm(EEditorPlaceActorsKind::Cube, PlaceActorsKindLabel(EEditorPlaceActorsKind::Cube));
    arm(EEditorPlaceActorsKind::Sphere, PlaceActorsKindLabel(EEditorPlaceActorsKind::Sphere));
    arm(EEditorPlaceActorsKind::Plane, PlaceActorsKindLabel(EEditorPlaceActorsKind::Plane));
    arm(EEditorPlaceActorsKind::Cylinder, PlaceActorsKindLabel(EEditorPlaceActorsKind::Cylinder));
    arm(EEditorPlaceActorsKind::BlockingVolume,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::BlockingVolume));
    arm(EEditorPlaceActorsKind::PlayerStart,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::PlayerStart));
    arm(EEditorPlaceActorsKind::TextRenderActor,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::TextRenderActor));
    arm(EEditorPlaceActorsKind::TriggerVolume,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::TriggerVolume));
    arm(EEditorPlaceActorsKind::PainCausingVolume,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::PainCausingVolume));
    arm(EEditorPlaceActorsKind::AISpawnPoint,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::AISpawnPoint));
    arm(EEditorPlaceActorsKind::PointLight,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::PointLight));
    arm(EEditorPlaceActorsKind::SpotLight, PlaceActorsKindLabel(EEditorPlaceActorsKind::SpotLight));
    arm(EEditorPlaceActorsKind::DirectionalLight,
        PlaceActorsKindLabel(EEditorPlaceActorsKind::DirectionalLight));
    if (ImGui::MenuItem("Blueprint class…")) {
        const std::string picked =
            EditorPickOpenFile("Leon Blueprint\0*.lbp\0All\0*.*\0", "Place Blueprint Class");
        if (!picked.empty()) {
            const std::string rel = MakePackRelativeAssetPath(ctx, picked);
            ctx.BeginPlaceActors(EEditorPlaceActorsKind::BlueprintClass,
                                 rel.empty() ? picked : rel);
            ctx.requestFocusPlaceActors = true;
        }
    }
    if (ImGui::MenuItem("StaticMesh…")) {
        if (ctx.level != nullptr && ctx.resources != nullptr) {
            const std::string picked =
                EditorPickOpenFile("Leon Static Mesh\0*.lmesh\0All\0*.*\0", "Place StaticMesh");
            if (!picked.empty()) {
                if (ctx.history != nullptr) {
                    ctx.history->Capture(ctx);
                }
                const std::string meshPath = MakePackRelativeAssetPath(ctx, picked);
                auto loaded = ctx.resources->LoadStaticMesh(ResolveAssetPath(meshPath));
                if (loaded == nullptr || !loaded->Valid()) {
                    loaded = ctx.resources->LoadStaticMesh(picked);
                }
                if (loaded != nullptr && loaded->Valid()) {
                    glm::vec3 pos =
                        ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0.5f, 0};
                    if (ctx.snapEnabled) {
                        Transform t{};
                        t.position = pos;
                        EditorCommands::SnapTransform(t, ctx.gridSize, ctx.rotationSnapDegrees,
                                                      false);
                        pos = t.position;
                    }
                    StaticMeshComponent actor;
                    actor.mesh = std::move(loaded);
                    actor.transform.position = pos;
                    actor.editorClass = "StaticMesh";
                    actor.meshPath = meshPath;
                    actor.collisionEnabled = true;
                    actor.material = ctx.resources->DefaultMaterial();
                    ctx.level->AddStaticMesh(std::move(actor));
                    ctx.Select(EEditorSelectionKind::StaticMesh,
                               ctx.level->StaticMeshes().size() - 1);
                    ctx.MarkDirty();
                }
            }
        }
    }
    ImGui::EndMenu();
}

void EditorLayout::DrawMenuBar(EditorContext& ctx) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Level…", "Ctrl+N")) {
            ctx.requestNewLevelDialog = true;
        }
        if (ImGui::MenuItem("Import Asset…", "Ctrl+I")) {
            ctx.requestOpenImportDialog = true;
        }
        if (ImGui::BeginMenu("Open Level")) {
            if (ctx.catalog != nullptr) {
                for (const LevelEntry& entry : ctx.catalog->Entries()) {
                    const std::string label = entry.pack + " / " + entry.name;
                    if (ImGui::MenuItem(label.c_str())) {
                        (void)OpenLevel(ctx, entry.path);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            (void)SaveCurrent(ctx);
        }
        if (ImGui::MenuItem("Save All", "Ctrl+Shift+S")) {
            (void)SaveAll(ctx);
        }
        if (ImGui::MenuItem("Fix Up Redirectors", nullptr, false, !ctx.projectPath.empty())) {
            ctx.requestFixUpRedirectors = true;
        }
        if (ImGui::MenuItem("Save Current Level As…")) {
            (void)SaveLevel(ctx, true);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Build Game…", "Ctrl+B", false,
                            !ctx.projectPath.empty() && !EditorBuild::IsBusy())) {
            ctx.requestBuildProject = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close Project")) {
            if (HasUnsavedPackages(ctx)) {
                ctx.pendingCloseProject = true;
                ctx.pendingExit = false;
            } else {
                ctx.requestCloseProject = true;
            }
        }
        if (ImGui::MenuItem("Exit")) {
            if (HasUnsavedPackages(ctx)) {
                ctx.pendingExit = true;
                ctx.pendingCloseProject = false;
            } else if (ctx.engine != nullptr) {
                ctx.engine->RequestQuit();
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = ctx.history != nullptr && ctx.history->CanUndo();
        const bool canRedo = ctx.history != nullptr && ctx.history->CanRedo();
        char undoLabel[96] = "Undo";
        char redoLabel[96] = "Redo";
        if (canUndo && !ctx.history->UndoTransactionName().empty()) {
            (void)std::snprintf(undoLabel, sizeof(undoLabel), "Undo %.*s",
                                static_cast<int>(ctx.history->UndoTransactionName().size()),
                                ctx.history->UndoTransactionName().data());
        }
        if (canRedo && !ctx.history->RedoTransactionName().empty()) {
            (void)std::snprintf(redoLabel, sizeof(redoLabel), "Redo %.*s",
                                static_cast<int>(ctx.history->RedoTransactionName().size()),
                                ctx.history->RedoTransactionName().data());
        }
        if (ImGui::MenuItem(undoLabel, "Ctrl+Z", false, canUndo && ctx.engine != nullptr)) {
            (void)ctx.history->Undo(*ctx.engine, ctx);
        }
        if (ImGui::MenuItem(redoLabel, "Ctrl+Y", false, canRedo && ctx.engine != nullptr)) {
            (void)ctx.history->Redo(*ctx.engine, ctx);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D / Ctrl+W", false, ctx.selection.IsValid())) {
            EditorCommands::DuplicateSelection(ctx, ctx.history);
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, ctx.selection.IsValid())) {
            EditorCommands::CopySelection(ctx);
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, EditorCommands::HasClipboard())) {
            EditorCommands::PasteClipboard(ctx, ctx.history);
        }
        if (ImGui::MenuItem("Delete", "Del", false, ctx.selection.IsValid())) {
            EditorCommands::DeleteSelection(ctx, ctx.history);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Deselect", "Esc")) {
            ctx.ClearSelection();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Project Settings…", nullptr, false, !ctx.projectPath.empty())) {
            ctx.showProjectSettings = true;
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Focus Selected", "F", false, ctx.selection.IsValid())) {
            ctx.RequestFocusSelected();
        }
        ImGui::MenuItem("Show Grid", nullptr, &ctx.showGrid);
        ImGui::MenuItem("Show Stats", "F3", &ctx.showStats);
        ImGui::MenuItem("Surface Snapping", nullptr, &ctx.surfaceSnapEnabled);
        ImGui::Separator();
        if (ImGui::MenuItem("Perspective", "Alt+1", ctx.viewMode == EEditorViewMode::Perspective)) {
            ctx.viewMode = EEditorViewMode::Perspective;
        }
        if (ImGui::MenuItem("Top (Ortho)", "Alt+2", ctx.viewMode == EEditorViewMode::OrthoTop)) {
            ctx.viewMode = EEditorViewMode::OrthoTop;
        }
        if (ImGui::MenuItem("Front (Ortho)", "Alt+3",
                            ctx.viewMode == EEditorViewMode::OrthoFront)) {
            ctx.viewMode = EEditorViewMode::OrthoFront;
        }
        if (ImGui::MenuItem("Side (Ortho)", "Alt+4", ctx.viewMode == EEditorViewMode::OrthoSide)) {
            ctx.viewMode = EEditorViewMode::OrthoSide;
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("View Mode")) {
            if (ImGui::MenuItem("Lit", "Alt+5",
                                ctx.viewportViewMode == EEditorViewportViewMode::Lit)) {
                ctx.viewportViewMode = EEditorViewportViewMode::Lit;
            }
            if (ImGui::MenuItem("Player Collision", "Alt+6",
                                ctx.viewportViewMode == EEditorViewportViewMode::PlayerCollision)) {
                ctx.viewportViewMode = EEditorViewportViewMode::PlayerCollision;
            }
            if (ImGui::MenuItem("Wireframe", "Alt+7",
                                ctx.viewportViewMode == EEditorViewportViewMode::Wireframe)) {
                ctx.viewportViewMode = EEditorViewportViewMode::Wireframe;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }
    DrawPlaceActorsMenu(ctx);
    if (ImGui::BeginMenu("Build")) {
        const bool canBuild = !ctx.projectPath.empty() && !EditorBuild::IsBusy();
        if (ImGui::MenuItem("Build Game", "Ctrl+B", false, canBuild)) {
            ctx.requestBuildProject = true;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Compile the open game.\n"
                              "Output: <YourProject>/Shipping/<Name>.exe\n"
                              "(intermediate files go to <YourProject>/build/)");
        }
        if (EditorBuild::IsBusy()) {
            ImGui::TextDisabled("Building… (see Output Log)");
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        if (ImGui::BeginMenu("Level Editor")) {
            ImGui::MenuItem("World Outliner", nullptr, &ctx.showOutliner);
            ImGui::MenuItem("Place Actors", nullptr, &ctx.showPlaceActors);
            ImGui::MenuItem("Details", nullptr, &ctx.showDetails);
            ImGui::MenuItem("World Settings", nullptr, &ctx.showWorldSettings);
            ImGui::MenuItem("Content Browser", nullptr, &ctx.showContentBrowser);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Asset Editors")) {
            ImGui::MenuItem("Material Editor", nullptr, &ctx.showMaterialEditor);
            ImGui::MenuItem("Static Mesh Editor", nullptr, &ctx.showStaticMeshEditor);
            ImGui::MenuItem("Blueprint Editor", nullptr, &ctx.showBlueprintEditor);
            ImGui::MenuItem("Widget Designer", nullptr, &ctx.showWidgetDesigner);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Developer Tools")) {
            ImGui::MenuItem("Output Log", nullptr, &ctx.showOutputLog);
            ImGui::MenuItem("Console", "`", &ctx.showConsole);
            ImGui::MenuItem("Asset Preview", nullptr, &ctx.showAssetPreview);
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Project Settings", nullptr, &ctx.showProjectSettings);
        ImGui::Separator();
        if (ImGui::MenuItem("Save Layout")) {
            ctx.requestSaveLayout = true;
        }
        if (ImGui::MenuItem("Reset Layout")) {
            ctx.requestResetLayout = true;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Drag window tabs to dock like Unreal");
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();

    // Global shortcuts (skip when typing in an input field)
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
        ctx.requestNewLevelDialog = true;
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) &&
        ctx.history != nullptr && ctx.engine != nullptr) {
        (void)ctx.history->Undo(*ctx.engine, ctx);
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) &&
        ctx.history != nullptr && ctx.engine != nullptr) {
        (void)ctx.history->Redo(*ctx.engine, ctx);
    }
    if (!io.WantTextInput && io.KeyCtrl &&
        (ImGui::IsKeyPressed(ImGuiKey_D) || ImGui::IsKeyPressed(ImGuiKey_W))) {
        EditorCommands::DuplicateSelection(ctx, ctx.history);
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        EditorCommands::CopySelection(ctx);
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        EditorCommands::PasteClipboard(ctx, ctx.history);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        if (io.KeyShift) {
            (void)SaveAll(ctx);
        } else {
            (void)SaveCurrent(ctx);
        }
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B) &&
        !ctx.projectPath.empty() && !EditorBuild::IsBusy()) {
        ctx.requestBuildProject = true;
    }
    if (!io.WantTextInput && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I)) {
        ctx.requestOpenImportDialog = true;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape) && !ctx.piePlaying) {
        if (ctx.IsAssetPickActive()) {
            ctx.ClearAssetPickMode();
        } else if (ctx.IsPlaceActorsActive()) {
            ctx.ClearPlaceActors();
        } else {
            ctx.ClearSelection();
        }
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_1)) {
        ctx.viewMode = EEditorViewMode::Perspective;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_2)) {
        ctx.viewMode = EEditorViewMode::OrthoTop;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_3)) {
        ctx.viewMode = EEditorViewMode::OrthoFront;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_4)) {
        ctx.viewMode = EEditorViewMode::OrthoSide;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_5)) {
        ctx.viewportViewMode = EEditorViewportViewMode::Lit;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_6)) {
        ctx.viewportViewMode = EEditorViewportViewMode::PlayerCollision;
    }
    if (!io.WantTextInput && io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_7)) {
        ctx.viewportViewMode = EEditorViewportViewMode::Wireframe;
    }
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_GraveAccent)) {
        ctx.showConsole = true;
        ctx.requestFocusConsole = true;
    }
}

bool EditorLayout::CreateNewLevel(EditorContext& ctx, int templateIndex) {
    if (ctx.engine == nullptr) {
        return false;
    }
    if (ctx.piePlaying) {
        ctx.requestPieStop = true;
        ctx.pendingNewLevelTemplate = templateIndex;
        return false;
    }
    std::string err;
    const ENewLevelTemplate tmpl = templateIndex == static_cast<int>(ENewLevelTemplate::Starter)
                                       ? ENewLevelTemplate::Starter
                                       : ENewLevelTemplate::Blank;
    if (!CreateLevelFromTemplate(tmpl, *ctx.engine, err)) {
        std::cerr << "Editor: New Level failed: " << err << '\n';
        return false;
    }
    if (!err.empty()) {
        std::cerr << "Editor: New Level warning: " << err << '\n';
    }
    ctx.levelPath.clear();
    ctx.dirty = true;
    ctx.lightingOutOfDate = false;
    ctx.ClearSelection();
    ctx.requestContentRefresh = true;
    ctx.pendingNewLevelTemplate = -1;
    if (ctx.history != nullptr) {
        ctx.history->Clear();
        ctx.history->Capture(ctx);
    }
    return true;
}

void EditorLayout::DrawNewLevelDialog(EditorContext& ctx) {
    if (ctx.requestNewLevelDialog) {
        ctx.requestNewLevelDialog = false;
        ImGui::OpenPopup("New Level");
    }

    if (!ui::BeginModal("New Level", nullptr)) {
        return;
    }

    ImGui::TextUnformatted("Choose a starting template");
    ImGui::Spacing();

    static int selected = 0;
    if (ImGui::RadioButton("Blank — empty level with sunlight", selected == 0)) {
        selected = 0;
    }
    if (ImGui::RadioButton("Starter — floor, player start, light, skybox", selected == 1)) {
        selected = 1;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ui::DialogButton("##nl_create", "Create", ui::EUiVariant::Primary)) {
        if (HasUnsavedPackages(ctx)) {
            ctx.pendingNewLevelTemplate = selected;
            ImGui::CloseCurrentPopup();
        } else {
            (void)CreateNewLevel(ctx, selected);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##nl_cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void EditorLayout::Draw(EditorContext& ctx) {
    ctx.ResolveSelectionIndices();

    if (!ctx.showContentBrowser) {
        ctx.contentBrowserFocused = false;
    }

    // Content Browser / menus may request Save Current / Save All / Save asset.
    if (!ctx.requestSaveAssetPath.empty()) {
        const std::string path = ctx.requestSaveAssetPath;
        ctx.requestSaveAssetPath.clear();
        namespace fs = std::filesystem;
        std::string ext = fs::path(path).extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext == ".lmat" || ext == ".lmgraph") {
            materialEditor_.OpenMaterial(path);
            (void)materialEditor_.SavePath(ctx, path);
        } else if (ext == ".lbp") {
            blueprintEditor_.Open(path);
            (void)blueprintEditor_.SavePath(ctx, path);
        } else if (ext == ".luw") {
            widgetDesigner_.Open(path);
            (void)widgetDesigner_.SavePath(ctx, path);
        } else if (ext == ".llev") {
            if (!ctx.levelPath.empty() &&
                fs::path(ctx.levelPath).lexically_normal() == fs::path(path).lexically_normal() &&
                ctx.dirty) {
                (void)SaveLevel(ctx, false);
            } else if (ctx.dirty && ctx.levelPath.empty()) {
                (void)SaveLevel(ctx, false);
            }
        }
    }
    if (ctx.requestSaveAll) {
        ctx.requestSaveAll = false;
        (void)SaveAll(ctx);
    }
    if (ctx.requestSaveCurrent) {
        ctx.requestSaveCurrent = false;
        (void)SaveCurrent(ctx);
    }

    auto remapMaterialEditor = [&]() {
        if (!ctx.requestMaterialEditorRemapFrom.empty()) {
            const std::string from = ctx.requestMaterialEditorRemapFrom;
            const std::string to = ctx.requestMaterialEditorRemapTo;
            materialEditor_.RemapAssetPath(ctx, from, to);
            blueprintEditor_.RemapAssetPath(ctx, from, to);
            widgetDesigner_.RemapAssetPath(ctx, from, to);
            staticMeshPreview_.RemapAssetPath(ctx, from, to);
            ctx.requestMaterialEditorRemapFrom.clear();
            ctx.requestMaterialEditorRemapTo.clear();
        }
    };

    // Flow: Content Browser Asset Tools (Rename / Move / Delete)
    remapMaterialEditor();
    if (!ctx.requestRenameAssetPath.empty()) {
        const std::string path = ctx.requestRenameAssetPath;
        const std::string name = ctx.pendingRenameAssetName;
        ctx.requestRenameAssetPath.clear();
        ctx.pendingRenameAssetName.clear();
        const AssetToolsResult result = RenameAsset(ctx, path, name);
        if (result.ok) {
            EditorToast("Renamed " + name, EEditorToastKind::Success, 2.5f);
            remapMaterialEditor();
        } else {
            EditorToast(result.error.empty() ? "Rename failed" : result.error,
                        EEditorToastKind::Error, 4.0f);
        }
    }
    if (!ctx.requestMoveAssetPath.empty() && !ctx.requestMoveAssetDestFolder.empty()) {
        const std::string from = ctx.requestMoveAssetPath;
        const std::string dest = ctx.requestMoveAssetDestFolder;
        ctx.requestMoveAssetPath.clear();
        ctx.requestMoveAssetDestFolder.clear();
        const AssetToolsResult result = MoveAsset(ctx, from, dest);
        if (result.ok) {
            EditorToast("Moved asset", EEditorToastKind::Success, 2.5f);
            remapMaterialEditor();
        } else {
            EditorToast(result.error.empty() ? "Move failed" : result.error,
                        EEditorToastKind::Error, 4.0f);
        }
    }
    if (!ctx.requestDuplicateAssetPaths.empty()) {
        const std::vector<std::string> paths = std::move(ctx.requestDuplicateAssetPaths);
        ctx.requestDuplicateAssetPaths.clear();
        int duplicated = 0;
        std::string lastError;
        for (const std::string& path : paths) {
            const AssetToolsResult result = DuplicateAsset(ctx, path);
            if (result.ok) {
                ++duplicated;
            } else if (lastError.empty()) {
                lastError = result.error;
            }
        }
        if (duplicated > 0) {
            EditorToast(duplicated == 1 ? "Duplicated asset"
                                        : ("Duplicated " + std::to_string(duplicated) + " assets"),
                        EEditorToastKind::Success, 2.5f);
        }
        if (duplicated < static_cast<int>(paths.size())) {
            EditorToast(lastError.empty() ? "Duplicate failed" : lastError, EEditorToastKind::Error,
                        4.0f);
        }
    }
    if (ctx.requestFixUpRedirectors) {
        ctx.requestFixUpRedirectors = false;
        const int fixed = FixUpRedirectors(ctx);
        if (fixed > 0) {
            EditorToast("Fixed " + std::to_string(fixed) + " redirector reference(s)",
                        EEditorToastKind::Success, 3.0f);
        } else {
            EditorToast("No redirectors to fix up", EEditorToastKind::Info, 2.5f);
        }
    }
    if (!ctx.requestDeleteAssetPath.empty()) {
        ctx.requestDeleteAssetPaths.insert(ctx.requestDeleteAssetPaths.begin(),
                                           ctx.requestDeleteAssetPath);
        ctx.requestDeleteAssetPath.clear();
    }
    if (!ctx.requestDeleteAssetPaths.empty()) {
        const std::vector<std::string> paths = std::move(ctx.requestDeleteAssetPaths);
        ctx.requestDeleteAssetPaths.clear();
        int deleted = 0;
        std::string lastError;
        for (const std::string& path : paths) {
            const AssetToolsResult result = DeleteAssets(ctx, path);
            if (result.ok) {
                ++deleted;
                remapMaterialEditor();
            } else if (lastError.empty()) {
                lastError = result.error;
            }
        }
        if (deleted > 0) {
            EditorToast(deleted == 1 ? "Deleted asset"
                                     : ("Deleted " + std::to_string(deleted) + " assets"),
                        EEditorToastKind::Success, 2.5f);
        }
        if (deleted < static_cast<int>(paths.size())) {
            EditorToast(lastError.empty() ? "Delete failed" : lastError, EEditorToastKind::Error,
                        4.0f);
        }
    }

    if (ctx.requestBuildProject) {
        ctx.requestBuildProject = false;
        if (HasUnsavedPackages(ctx)) {
            (void)SaveAll(ctx);
        }
        ctx.showOutputLog = true;
        ctx.requestFocusOutputLog = true;
        if (EditorBuild::StartProjectBuild(ctx.projectPath, ctx.projectName,
                                           ctx.projectBuildDedicatedServer)) {
            EditorToast("Building game…", EEditorToastKind::Info, 4.0f);
        } else {
            EditorToast("Build failed to start", EEditorToastKind::Error, 4.5f);
        }
    }
    if (!ctx.piePlaying && !ctx.pendingOpenPath.empty() && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        const std::string path = ctx.pendingOpenPath;
        ctx.pendingOpenPath.clear();
        (void)OpenLevel(ctx, path);
    }
    if (!ctx.piePlaying && ctx.pendingNewLevelTemplate >= 0 && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes") && !ImGui::IsPopupOpen("New Level")) {
        const int tmpl = ctx.pendingNewLevelTemplate;
        ctx.pendingNewLevelTemplate = -1;
        (void)CreateNewLevel(ctx, tmpl);
    }
    if (!ctx.piePlaying && ctx.pendingCloseProject && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        ctx.pendingCloseProject = false;
        ctx.requestCloseProject = true;
    }
    if (!ctx.piePlaying && ctx.pendingExit && !HasUnsavedPackages(ctx) &&
        !ImGui::IsPopupOpen("Unsaved Changes")) {
        ctx.pendingExit = false;
        if (ctx.engine != nullptr) {
            ctx.engine->RequestQuit();
        }
    }

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("LeonEditorHost", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    DrawMenuBar(ctx);

    SetupDefaultDocking(ctx);

    // Unreal-like asset pick chrome (Details / World Settings → Content Browser).
    if (ctx.IsAssetPickActive()) {
        const ImGuiViewport* hostVp = ImGui::GetMainViewport();
        if (hostVp != nullptr) {
            ImGui::SetNextWindowPos(
                ImVec2(hostVp->WorkPos.x + hostVp->WorkSize.x * 0.5f, hostVp->WorkPos.y + 48.0f),
                ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        }
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ui::Tokens().bannerBg);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::Layout().popupRounding);
        if (ImGui::Begin("##AssetPickBanner", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking)) {
            const char* kind = ctx.requestPickMaterial ? "Material (.lmat)"
                               : ctx.requestPickMesh   ? "Static Mesh"
                               : ctx.requestPickHdr    ? "HDR skybox"
                                                       : "Asset";
            ImGui::Text("Pick a %s in the Content Browser  —  Esc to cancel", kind);
            ImGui::SameLine();
            if (ui::Button("##pick_cancel", "Cancel", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
                ctx.ClearAssetPickMode();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ctx.showContentBrowser = true;
    }

    toolbar_.Draw(ctx);
    viewport_.Draw(ctx);
    outliner_.Draw(ctx);
    placeActors_.Draw(ctx);
    details_.Draw(ctx);
    contentBrowser_.Draw(ctx);
    assetPreview_.Draw(ctx);
    PromoteAssetEditorDock(ctx.requestFocusStaticMeshEditor);
    staticMeshPreview_.Draw(ctx);
    worldSettings_.Draw(ctx);
    projectSettings_.Draw(ctx);
    outputLog_.Draw(ctx);
    console_.Draw(ctx);
    PromoteAssetEditorDock(ctx.requestFocusMaterialEditor);
    materialEditor_.Draw(ctx);
    PromoteAssetEditorDock(ctx.requestFocusBlueprintEditor);
    blueprintEditor_.Draw(ctx);
    PromoteAssetEditorDock(ctx.requestFocusWidgetDesigner);
    widgetDesigner_.Draw(ctx);

    if (ctx.requestOpenImportDialog) {
        ctx.requestOpenImportDialog = false;
        if (!ctx.pendingImportSourcePath.empty()) {
            importDialog_.OpenWithSource(ctx.pendingImportSourcePath);
            ctx.pendingImportSourcePath.clear();
        } else {
            importDialog_.Open();
        }
    }
    importDialog_.Draw(ctx);
    DrawNewLevelDialog(ctx);

    // Dirty confirmation (open level / new level / close project / exit).
    if ((!ctx.pendingOpenPath.empty() || ctx.pendingNewLevelTemplate >= 0 ||
         ctx.pendingCloseProject || ctx.pendingExit) &&
        HasUnsavedPackages(ctx) && !ImGui::IsPopupOpen("Unsaved Changes") &&
        !ImGui::IsPopupOpen("New Level")) {
        ImGui::OpenPopup("Unsaved Changes");
    }
    if (ui::BeginModal("Unsaved Changes", nullptr)) {
        if (ctx.pendingExit) {
            ImGui::TextUnformatted("You have unsaved packages. Save All before exiting?");
        } else if (ctx.pendingCloseProject) {
            ImGui::TextUnformatted("You have unsaved packages. Save All before closing?");
        } else if (ctx.pendingNewLevelTemplate >= 0) {
            ImGui::TextUnformatted("You have unsaved packages. Discard and create a new level?");
        } else {
            ImGui::TextUnformatted("You have unsaved packages. Discard and open the new level?");
        }
        if (ui::DialogButton("##discard", "Discard", ui::EUiVariant::Destructive)) {
            DiscardUnsavedPackages(ctx);
            ImGui::CloseCurrentPopup();
            if (ctx.pendingExit) {
                ctx.pendingExit = false;
                if (ctx.engine != nullptr) {
                    ctx.engine->RequestQuit();
                }
            } else if (ctx.pendingCloseProject) {
                ctx.pendingCloseProject = false;
                ctx.requestCloseProject = true;
            } else if (ctx.pendingNewLevelTemplate >= 0) {
                const int tmpl = ctx.pendingNewLevelTemplate;
                ctx.pendingNewLevelTemplate = -1;
                (void)CreateNewLevel(ctx, tmpl);
            } else {
                const std::string path = ctx.pendingOpenPath;
                ctx.pendingOpenPath.clear();
                (void)OpenLevel(ctx, path);
            }
        }
        ImGui::SameLine();
        if (ui::DialogButton("##save_all", "Save All", ui::EUiVariant::Primary)) {
            if (SaveAll(ctx)) {
                ImGui::CloseCurrentPopup();
                if (ctx.pendingExit) {
                    ctx.pendingExit = false;
                    if (ctx.engine != nullptr) {
                        ctx.engine->RequestQuit();
                    }
                } else if (ctx.pendingCloseProject) {
                    ctx.pendingCloseProject = false;
                    ctx.requestCloseProject = true;
                } else if (ctx.pendingNewLevelTemplate >= 0) {
                    const int tmpl = ctx.pendingNewLevelTemplate;
                    ctx.pendingNewLevelTemplate = -1;
                    (void)CreateNewLevel(ctx, tmpl);
                } else {
                    const std::string path = ctx.pendingOpenPath;
                    ctx.pendingOpenPath.clear();
                    (void)OpenLevel(ctx, path);
                }
            }
        }
        ImGui::SameLine();
        if (ui::DialogButton("##unsaved_cancel", "Cancel", ui::EUiVariant::Ghost)) {
            ctx.pendingOpenPath.clear();
            ctx.pendingNewLevelTemplate = -1;
            ctx.pendingCloseProject = false;
            ctx.pendingExit = false;
            ImGui::CloseCurrentPopup();
        }
        ui::EndModal();
    }

    if (ui::BeginModal("Close Asset Tab", nullptr)) {
        ImGui::TextUnformatted("Save changes before closing this asset tab?");
        ImGui::TextWrapped("%s", ctx.pendingCloseAssetTabPath.c_str());
        if (ui::DialogButton("##close_tab_save", "Save", ui::EUiVariant::Primary)) {
            bool saved = true;
            switch (ctx.pendingCloseAssetTabKind) {
            case EAssetEditorTabKind::Material:
                saved = materialEditor_.SavePath(ctx, ctx.pendingCloseAssetTabPath);
                if (saved) {
                    materialEditor_.CloseDoc(ctx.pendingCloseAssetTabPath);
                }
                break;
            case EAssetEditorTabKind::Blueprint:
                saved = blueprintEditor_.SavePath(ctx, ctx.pendingCloseAssetTabPath);
                if (saved) {
                    blueprintEditor_.CloseDoc(ctx.pendingCloseAssetTabPath);
                }
                break;
            case EAssetEditorTabKind::Widget:
                saved = widgetDesigner_.SavePath(ctx, ctx.pendingCloseAssetTabPath);
                if (saved) {
                    widgetDesigner_.CloseDoc(ctx.pendingCloseAssetTabPath);
                }
                break;
            default:
                break;
            }
            if (saved) {
                ctx.pendingCloseAssetTabPath.clear();
                ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::None;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ui::DialogButton("##close_tab_discard", "Discard", ui::EUiVariant::Destructive)) {
            switch (ctx.pendingCloseAssetTabKind) {
            case EAssetEditorTabKind::Material:
                materialEditor_.CloseDoc(ctx.pendingCloseAssetTabPath);
                break;
            case EAssetEditorTabKind::Blueprint:
                blueprintEditor_.CloseDoc(ctx.pendingCloseAssetTabPath);
                break;
            case EAssetEditorTabKind::Widget:
                widgetDesigner_.CloseDoc(ctx.pendingCloseAssetTabPath);
                break;
            default:
                break;
            }
            ctx.pendingCloseAssetTabPath.clear();
            ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::None;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ui::DialogButton("##close_tab_cancel", "Cancel", ui::EUiVariant::Ghost)) {
            ctx.pendingCloseAssetTabPath.clear();
            ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::None;
            ImGui::CloseCurrentPopup();
        }
        ui::EndModal();
    } else if (!ctx.pendingCloseAssetTabPath.empty() &&
               ctx.pendingCloseAssetTabKind != EAssetEditorTabKind::None) {
        ImGui::OpenPopup("Close Asset Tab");
    }

    ImGui::End();

    DrawEditorToasts();

    if (saveDefaultLayoutFrames_ >= 0) {
        --saveDefaultLayoutFrames_;
        if (saveDefaultLayoutFrames_ < 0) {
            ctx.requestSaveLayout = true;
        }
    }

    if (ctx.requestSaveLayout) {
        ctx.requestSaveLayout = false;
        if (const char* ini = ImGui::GetIO().IniFilename; ini != nullptr && ini[0] != '\0') {
            ImGui::SaveIniSettingsToDisk(ini);
            std::cout << "Editor: saved layout to " << ini << '\n';
        }
    }
}

} // namespace leon::editor
