#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <imgui.h>
#include <leon/core/Camera.h>
#include <leon/core/Transform.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/editor/TransformGizmo.h>
#include <leon/gameplay/GameMode.h>
#include <leon/gameplay/NavigationSystem.h>
#include <leon/physics/PhysScene.h>
#include <leon/render/Material.h>
#include <optional>
#include <string>
#include <vector>

namespace leon::editor {

enum class EViewportDrag : std::uint8_t {
    None,
    Pan,
    Orbit,
    Look,
    Dolly,
};

/// Central Viewport: FBO scene + ImGuizmo + Unreal-like camera + picking + selection AABB.
class ViewportPanel {
public:
    void Draw(EditorContext& ctx);
    /// Renders the Level into the FBO (also called from `Draw` before `ImGui::Image`).
    void RenderScene(EditorContext& ctx);
    /// Frame orbit camera on level static mesh bounds (after open / reload).
    static void FocusLevelBounds(EditorContext& ctx);

    [[nodiscard]] EditorViewportTarget& Target() { return target_; }

private:
    void HandleCameraInput(EditorContext& ctx, float deltaTime);
    void HandleViewportPicking(EditorContext& ctx);
    void DrawEditorHelpers(EditorContext& ctx);
    void DrawSelectionOverlay(EditorContext& ctx);
    void FocusSelection(EditorContext& ctx);
    void ApplyViewMode(EditorContext& ctx, leon::Camera& camera);
    /// Sync PhysScene from the level and queue Player Collision wireframes.
    void AppendPlayerCollisionOverlay(EditorContext& ctx);
    /// F4 collision + F1 NavMesh preview (edit mode, outside PIE).
    void AppendEngineDebugOverlay(EditorContext& ctx);
    void RebuildNavPreview(EditorContext& ctx);
    void DrawDebugHintsOverlay(EditorContext& ctx, const ImVec2& imageMin);
    void DrawViewModeOverlay(EditorContext& ctx, const ImVec2& imageMin);
    void DrawTextRenderOverlay(const leon::Level& level, const ImVec2& imageMin,
                               const ImVec2& imageSize);
    /// Unreal-like viewport menu bar: Perspective / View Mode / Show.
    void DrawViewportMenuBar(EditorContext& ctx);
    void DrawSelectionContextMenu(EditorContext& ctx);
    [[nodiscard]] leon::Transform* SelectedTransform(EditorContext& ctx);
    [[nodiscard]] leon::Transform* SelectedTransformFor(EditorContext& ctx,
                                                        const EditorSelection& sel);
    [[nodiscard]] bool SelectedFocusPoint(EditorContext& ctx, glm::vec3& outPoint);

    void DrawStatsOverlay(EditorContext& ctx, const ImVec2& imageMin);
    [[nodiscard]] std::optional<std::size_t> PickStaticMeshUnderMouse(EditorContext& ctx) const;
    void ClearMaterialDropPreview(EditorContext& ctx);
    void ApplyMaterialDropPreview(EditorContext& ctx, std::size_t meshIndex,
                                  const std::string& materialPath, const Material& material);
    [[nodiscard]] bool HandleMaterialDrag(EditorContext& ctx, const std::string& path,
                                          bool isDelivery);

    EditorViewportTarget target_;
    TransformGizmo gizmo_;
    leon::Transform actorGizmoScratch_{};
    leon::Transform multiGizmoScratch_{};
    bool multiGizmoSeeded_ = false;
    EViewportDrag dragMode_ = EViewportDrag::None;
    bool flyActive_ = false;
    bool suppressDragDelta_ = false;
    bool gizmoWasUsing_ = false;
    /// RMB click (no drag) opens selection context menu; hold+drag keeps fly look.
    bool rmbContextPending_ = false;
    ImVec2 rmbPressPos_{};
    /// Defer Blueprint expand until gizmo release (avoid per-frame rebuild).
    bool blueprintGizmoNeedsExpand_ = false;
    std::uint64_t blueprintGizmoExpandInstanceId_ = 0;
    EEditorViewMode lastAppliedViewMode_ = EEditorViewMode::Perspective;
    /// Edit-mode collision + NavMesh preview when `ctx.world` is null (outside PIE).
    leon::PhysScene collisionPreviewScene_;
    leon::NavigationSystem navPreview_;
    std::uint64_t navPreviewLevelToken_ = 0;
    float statsAccumTime_ = 0.0f;
    int statsAccumFrames_ = 0;
    float displayFps_ = 0.0f;
    float displayMs_ = 0.0f;

    /// Temporary material swap while dragging a .lmat onto a mesh under the cursor.
    bool materialPreviewActive_ = false;
    std::size_t materialPreviewMeshIndex_ = 0;
    Material materialPreviewSaved_{};
    std::vector<Material> materialPreviewSavedSlots_{};
    bool materialPreviewSavedOverride_ = false;
    std::string materialPreviewSavedPath_;
    std::string materialPreviewPath_;

    glm::mat4 overlayView_{1.0f};
    glm::mat4 overlayProjection_{1.0f};
    glm::vec3 overlayEye_{0.0f};
    float overlayFov_ = 60.0f;
    bool overlayOrtho_ = false;
    int overlayFbW_ = 0;
    int overlayFbH_ = 0;
    bool overlayMatricesValid_ = false;

    static void EnsureEditorFreeLook(leon::Camera& camera);
    void HandleAssetDrop(EditorContext& ctx);
    void DrawGrid(EditorContext& ctx);
    /// Corner XYZ triad (ImGui overlay) aligned to the active view camera.
    void DrawAxisIndicator(const leon::Camera& camera, const ImVec2& imageMin,
                           const ImVec2& imageSize) const;
    [[nodiscard]] bool WorldPointUnderMouse(EditorContext& ctx, glm::vec3& outPoint) const;
};

} // namespace leon::editor
