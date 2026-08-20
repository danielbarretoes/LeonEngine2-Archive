#pragma once

#include <cstdint>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/MaterialSpherePreview.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/LeonMaterialGraph.h>
#include <memory>
#include <string>
#include <vector>

namespace leon::editor {

/// Dockable Material Editor — `.lmgraph` (Material) or `.lmat` (Material Instance).
class MaterialEditorPanel {
public:
    void Draw(EditorContext& ctx);
    void OpenMaterial(const std::string& path);

    [[nodiscard]] bool HasDirtyDocs() const;
    [[nodiscard]] bool IsPathDirty(const std::string& path) const;
    [[nodiscard]] bool HasFocusedDirtyDoc() const;
    [[nodiscard]] bool SaveFocused(EditorContext& ctx);
    [[nodiscard]] bool SavePath(EditorContext& ctx, const std::string& path);
    [[nodiscard]] bool SaveAll(EditorContext& ctx);
    void SyncDirtyPaths(EditorContext& ctx) const;
    void RemapAssetPath(EditorContext& ctx, const std::string& fromAbs, const std::string& toAbs);
    void CloseDoc(const std::string& path);

private:
    enum class EMode : std::uint8_t {
        Instance = 0,
        Graph = 1,
    };

    struct Doc {
        std::string path;
        EMode mode = EMode::Instance;
        leon::LeonMaterialDocument instance{}; // authoring / sparse overrides
        leon::LeonMaterialDocument resolved{}; // inherited display values
        leon::LeonMaterialGraphDocument graph{};
        bool dirty = false;
        bool open = true;
        bool focusOnce = false;
        bool resolvedDirty = true;
        /// Graph sphere: recompile while dirty; skip when saved and already valid.
        bool graphPreviewReady = false;
        char nameBuf[128]{};
        char parentBuf[260]{};
        char baseMapBuf[260]{};
        char normalMapBuf[260]{};
        char emissiveMapBuf[260]{};
        char ormMapBuf[260]{};
        char opacityMaskBuf[260]{};
        std::unique_ptr<MaterialSpherePreview> preview;
    };

    [[nodiscard]] Doc* FindDoc(const std::string& path);
    [[nodiscard]] const Doc* FindDoc(const std::string& path) const;
    void DrawInstanceDoc(EditorContext& ctx, Doc& doc);
    void DrawGraphDoc(EditorContext& ctx, Doc& doc);
    bool SaveDoc(EditorContext& ctx, Doc& doc);
    void ApplyToSelection(EditorContext& ctx, Doc& doc);
    void SyncBuffersFromDoc(Doc& doc);
    void SyncDocFromBuffers(Doc& doc);
    void RefreshResolved(Doc& doc);
    void CreateInstanceFromDoc(EditorContext& ctx, Doc& doc);
    void RefreshTextureList(EditorContext& ctx);

    std::vector<Doc> docs_;
    std::string focusedPath_;
    std::string texturesProjectKey_;
    std::vector<EditorTextureEntry> textures_;
};

} // namespace leon::editor
