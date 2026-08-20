#pragma once

#include <imgui.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/panels/AssetPreviewPanel.h>
#include <leon/editor/panels/BlueprintEditorPanel.h>
#include <leon/editor/panels/ConsolePanel.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <leon/editor/panels/DetailsPanel.h>
#include <leon/editor/panels/ImportDialog.h>
#include <leon/editor/panels/MaterialEditorPanel.h>
#include <leon/editor/panels/OutlinerPanel.h>
#include <leon/editor/panels/OutputLogPanel.h>
#include <leon/editor/panels/PlaceActorsPanel.h>
#include <leon/editor/panels/ProjectSettingsPanel.h>
#include <leon/editor/panels/StaticMeshPreviewPanel.h>
#include <leon/editor/panels/ToolbarPanel.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/panels/WidgetDesignerPanel.h>
#include <leon/editor/panels/WorldSettingsPanel.h>
#include <string>

namespace leon::editor {

/// Full-window docking host (menu bar + default Unreal-like panel layout).
class EditorLayout {
public:
    void Draw(EditorContext& ctx);

    [[nodiscard]] ViewportPanel& Viewport() { return viewport_; }

    /// Path used for ImGui docking / window layout (`editor_layout.ini` beside the exe).
    [[nodiscard]] static std::string LayoutIniPath();

    /// Unreal-like Discard: drop dirty Material / Blueprint / Widget packages without writing.
    void DiscardUnsavedPackages(EditorContext& ctx);

    /// Clear asset editor tabs and session open requests (project switch / layout reset).
    void ResetAssetEditors(EditorContext& ctx);

    /// Persist the open `.llev` (Save As dialog when `saveAs` or `levelPath` empty).
    [[nodiscard]] bool SaveLevel(EditorContext& ctx, bool saveAs);

private:
    void DrawMenuBar(EditorContext& ctx);
    void DrawPlaceActorsMenu(EditorContext& ctx);
    void DrawNewLevelDialog(EditorContext& ctx);
    void SetupDefaultDocking(EditorContext& ctx);
    void ApplyDefaultDockLayout(ImGuiID dockspaceId);
    /// Dock asset editor into the Content Browser tab group and bring its tab forward.
    void PromoteAssetEditorDock(bool promote);
    [[nodiscard]] static bool HasUsableSavedLayout();
    [[nodiscard]] bool OpenLevel(EditorContext& ctx, const std::string& path);
    /// Unreal-like: focused Material Editor if dirty, else current level if dirty.
    [[nodiscard]] bool SaveCurrent(EditorContext& ctx);
    /// Unreal-like Save All: dirty materials + dirty level.
    [[nodiscard]] bool SaveAll(EditorContext& ctx);
    [[nodiscard]] bool CreateNewLevel(EditorContext& ctx, int templateIndex);
    [[nodiscard]] bool HasUnsavedPackages(const EditorContext& ctx) const;

    ViewportPanel viewport_;
    OutlinerPanel outliner_;
    PlaceActorsPanel placeActors_;
    DetailsPanel details_;
    ContentBrowserPanel contentBrowser_;
    AssetPreviewPanel assetPreview_;
    StaticMeshPreviewPanel staticMeshPreview_;
    ImportDialog importDialog_;
    ToolbarPanel toolbar_;
    WorldSettingsPanel worldSettings_;
    ProjectSettingsPanel projectSettings_;
    OutputLogPanel outputLog_;
    ConsolePanel console_;
    MaterialEditorPanel materialEditor_;
    BlueprintEditorPanel blueprintEditor_;
    WidgetDesignerPanel widgetDesigner_;
    bool dockingConfigured_ = false;
    bool forceDefaultLayout_ = false;
    ImGuiID dockAssetEditors_ = 0;
    /// Save ini a few frames after applying the built-in default (windows must dock first).
    int saveDefaultLayoutFrames_ = -1;
};

} // namespace leon::editor
