#pragma once

#include <leon/content/LeonBlueprint.h>
#include <leon/editor/EditorContext.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Edit `.lbp` composition + Event Graph lite (event → ordered actions).
class BlueprintEditorPanel {
public:
    void Draw(EditorContext& ctx);
    void Open(const std::string& path);
    [[nodiscard]] bool HasDirtyDocs() const;
    [[nodiscard]] bool HasFocusedDirtyDoc() const;
    bool SaveFocused(EditorContext& ctx);
    bool SaveAll(EditorContext& ctx);
    void SyncDirtyPaths(EditorContext& ctx) const;
    void RemapAssetPath(EditorContext& ctx, const std::string& fromAbs, const std::string& toAbs);
    void CloseDoc(const std::string& path);
    [[nodiscard]] bool SavePath(EditorContext& ctx, const std::string& path);

private:
    struct Doc {
        std::string path;
        BlueprintDocument document;
        bool dirty = false;
        bool open = true;
        bool focusTab = true;
    };

    Doc* FindDoc(const std::string& path);
    bool SaveDoc(EditorContext& ctx, Doc& doc);
    void DrawDoc(EditorContext& ctx, Doc& doc);

    std::vector<Doc> docs_;
    std::string focusedPath_;
};

} // namespace leon::editor
