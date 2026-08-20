#pragma once

#include <leon/core/Camera.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorViewportTarget.h>
#include <leon/level/Level.h>
#include <leon/render/StaticMesh.h>
#include <memory>
#include <string>
#include <vector>

namespace leon::editor {

/// Unreal-like Static Mesh Editor: one dockable window, asset tabs, viewport + Details.
class StaticMeshPreviewPanel {
public:
    void Draw(EditorContext& ctx);
    void Open(const std::string& path);
    void RemapAssetPath(EditorContext& ctx, const std::string& fromAbs, const std::string& toAbs);
    void CloseDoc(const std::string& path);

private:
    struct Doc {
        std::string path;
        bool open = true;
        bool focusTab = true;
        std::string status;
        std::unique_ptr<EditorViewportTarget> target = std::make_unique<EditorViewportTarget>();
        Camera camera{};
        Level previewLevel{};
        std::shared_ptr<StaticMesh> mesh;
        bool orbitDragging = false;
        double lastMouseX = 0.0;
        double lastMouseY = 0.0;
        float sunIntensity = 3.25f;
        float envExposure = 1.15f;
    };

    Doc* FindDoc(const std::string& path);
    [[nodiscard]] bool LoadDoc(EditorContext& ctx, Doc& doc, const std::string& path);
    void SyncPreviewLighting(Doc& doc);
    void ResetCamera(Doc& doc);
    void PlaceInLevel(EditorContext& ctx, Doc& doc);
    void DrawDetails(EditorContext& ctx, Doc& doc);
    void DrawActiveDoc(EditorContext& ctx, Doc& doc);
    void HandleOrbit(EditorContext& ctx, Doc& doc);
    void RenderViewport(EditorContext& ctx, Doc& doc, int vw, int vh);

    std::vector<Doc> docs_;
    std::string activePath_;
};

} // namespace leon::editor
