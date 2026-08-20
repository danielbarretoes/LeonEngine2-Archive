#pragma once

#include <cstdint>
#include <leon/editor/EditorContext.h>
#include <leon/editor/MaterialSpherePreview.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Content Browser: hierarchy tree or flat contents grid with path navigation.
class ContentBrowserPanel {
public:
    enum class EViewMode : std::uint8_t {
        Hierarchy = 0,
        Contents = 1,
    };

    struct Entry {
        std::string name;
        std::string path;
        bool isDirectory = false;
        enum class Kind {
            Folder,
            Level,
            Material,      // `.lmat` MaterialInstance
            MaterialGraph, // `.lmgraph` parent Material
            StaticMesh,
            Texture,
            Blueprint,
            UserWidget,
            FbxSource,
            Character,
            SkelMesh,
            Skeleton,
            Anim,
            AnimMontage,
            AnimBlueprint,
            PhysicsAsset,
            Hdr,
            Lightmap,
            Redirector,
            EnginePrimitive,
            Other
        } kind = Kind::Other;
        std::vector<Entry> children;
        /// Optional albedo swatch for materials (fallback before 3D thumb is ready).
        float swatch[3] = {0.35f, 0.35f, 0.38f};
        bool hasSwatch = false;
    };

    void Draw(EditorContext& ctx);
    void Refresh(EditorContext& ctx);

private:
    struct TileHit {
        std::string path;
        float minX = 0.0f;
        float minY = 0.0f;
        float maxX = 0.0f;
        float maxY = 0.0f;
    };

    void SyncRoot(EditorContext& ctx);
    [[nodiscard]] static Entry::Kind ClassifyFile(const std::string& path);
    [[nodiscard]] Entry BuildTree(const std::string& absoluteDir) const;
    void DrawEntry(EditorContext& ctx, const Entry& entry);
    void DrawEngineLibrary(EditorContext& ctx);
    void DrawBreadcrumbs(EditorContext& ctx);
    void DrawContentsGrid(EditorContext& ctx);
    void DrawStatusFooter(const EditorContext& ctx) const;
    void DrawCreateContextMenu(EditorContext& ctx);
    void DrawNewMaterialModal(EditorContext& ctx);
    void DrawNewMaterialGraphModal(EditorContext& ctx);
    void DrawCreateInstanceFromModal(EditorContext& ctx);
    void DrawNewBlueprintModal(EditorContext& ctx);
    void DrawNewWidgetModal(EditorContext& ctx);
    void DrawNewFolderModal(EditorContext& ctx);
    void DrawRenameAssetModal(EditorContext& ctx);
    void DrawDeleteAssetsModal(EditorContext& ctx);
    void DrawReferenceViewerModal(EditorContext& ctx);
    void DrawAssetItemContextMenu(EditorContext& ctx, const Entry& entry);
    void AcceptFolderDropTarget(EditorContext& ctx, const Entry& folderEntry);
    void ActivateEntry(EditorContext& ctx, const Entry& entry, bool openFolder);
    void BeginRenameAsset(EditorContext& ctx, const std::string& path);
    void BeginDeleteAsset(EditorContext& ctx, const std::string& path);
    void BeginDeletePaths(EditorContext& ctx, const std::vector<std::string>& paths);
    void BeginReferenceViewer(EditorContext& ctx, const std::string& path);
    void BeginDuplicatePaths(EditorContext& ctx, const std::vector<std::string>& paths);
    void ClearBrowserSelection(EditorContext& ctx);
    void SetPrimarySelection(EditorContext& ctx, const std::string& path);
    void SelectOnly(EditorContext& ctx, const std::string& path);
    void ToggleSelect(EditorContext& ctx, const std::string& path);
    void SelectRange(EditorContext& ctx, const std::vector<Entry>& children,
                     const std::string& toPath);
    void SelectAllVisible(EditorContext& ctx, const std::vector<Entry>& children);
    void ApplyMarqueeSelection(EditorContext& ctx, const std::vector<TileHit>& tiles,
                               bool additive);
    void HandleMarquee(EditorContext& ctx, const std::vector<TileHit>& tiles);
    void SyncSelectionFolder(EditorContext& ctx);
    [[nodiscard]] bool IsPathSelected(const std::string& path) const;
    void ApplyTileClick(EditorContext& ctx, const Entry& entry, const std::vector<Entry>& children);
    [[nodiscard]] const Entry* FindEntryByPath(const Entry& node, const std::string& path) const;
    [[nodiscard]] std::vector<Entry> CurrentFolderChildren() const;
    void EnsureMaterialSwatch(Entry& entry) const;
    void InvalidateLiveFolderListing() const;

    std::string contentRoot_;
    std::string lastLevelPath_;
    std::string currentFolder_;
    Entry root_;
    bool scanned_ = false;
    /// Contents-view listing for `currentFolder_` (refreshed on navigate / Refresh).
    mutable std::string liveFolderPath_;
    mutable std::vector<Entry> liveFolderChildren_;
    EViewMode viewMode_ = EViewMode::Contents;
    MaterialThumbnailCache materialThumbs_;
    MeshThumbnailCache meshThumbs_;
    TextureThumbnailCache textureThumbs_;
    SkeletalThumbnailCache skeletalThumbs_;
    bool openNewMaterialModal_ = false;
    bool openNewMaterialGraphModal_ = false;
    bool openCreateInstanceFromModal_ = false;
    bool openNewBlueprintModal_ = false;
    bool openNewWidgetModal_ = false;
    bool openNewFolderModal_ = false;
    char newBlueprintNameBuf_[128]{"BP_New"};
    char newWidgetNameBuf_[128]{"WBP_New"};
    char createInstanceNameBuf_[128]{"MI_New"};
    std::string createInstanceParentPath_;
    bool openRenameModal_ = false;
    bool openDeleteModal_ = false;
    bool openReferenceViewerModal_ = false;
    std::string modalAssetPath_;
    std::vector<std::string> pendingDeletePaths_;
    int deleteRefCount_ = 0;
    std::vector<std::string> deleteReferencers_;
    std::vector<std::string> referenceViewerReferencers_;
    int referenceViewerRefCount_ = 0;
    char renameNameBuf_[128]{};

    /// Multi-select (Contents grid): Shift range, Ctrl toggle, empty-space marquee.
    std::vector<std::string> selectedPaths_;
    std::string selectionAnchorPath_;
    std::string selectionFolder_;
    bool marqueeActive_ = false;
    bool marqueeAdditive_ = false;
    float marqueeStartX_ = 0.0f;
    float marqueeStartY_ = 0.0f;
    float marqueeEndX_ = 0.0f;
    float marqueeEndY_ = 0.0f;
};

} // namespace leon::editor
