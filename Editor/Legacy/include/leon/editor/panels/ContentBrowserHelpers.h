#pragma once

#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <string>

namespace leon::editor {
namespace content_browser {

using leon::editor::NormalizeAssetPathAbs;
using leon::editor::PathsEqualNormalized;

[[nodiscard]] inline std::string NormalizePath(const std::string& path) {
    return NormalizeAssetPathAbs(path);
}

inline std::string ExtLower(const std::filesystem::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

[[nodiscard]] inline bool IsAssetPathDirty(const EditorContext& ctx,
                                           const ContentBrowserPanel::Entry& entry) {
    if (entry.isDirectory) {
        return false;
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::Level) {
        return ctx.dirty && PathsEqualNormalized(ctx.levelPath, entry.path);
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::Material ||
        entry.kind == ContentBrowserPanel::Entry::Kind::MaterialGraph) {
        for (const std::string& p : ctx.dirtyMaterialPaths) {
            if (PathsEqualNormalized(p, entry.path)) {
                return true;
            }
        }
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::Blueprint) {
        for (const std::string& p : ctx.dirtyBlueprintPaths) {
            if (PathsEqualNormalized(p, entry.path)) {
                return true;
            }
        }
    }
    if (entry.kind == ContentBrowserPanel::Entry::Kind::UserWidget) {
        for (const std::string& p : ctx.dirtyWidgetPaths) {
            if (PathsEqualNormalized(p, entry.path)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace content_browser
} // namespace leon::editor
