#pragma once

#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorContext.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Unreal-like Place Actors (Modes): pick a class, then LMB in the Viewport to place.
class PlaceActorsPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void RefreshBlueprints(const EditorContext& ctx);
    void DrawCategoryBasic(EditorContext& ctx, const char* filter);
    void DrawCategoryLights(EditorContext& ctx, const char* filter);
    void DrawCategoryVolumes(EditorContext& ctx, const char* filter);
    void DrawCategoryBlueprints(EditorContext& ctx, const char* filter);
    void DrawCategoryRecent(EditorContext& ctx, const char* filter);

    std::string blueprintsProjectKey_;
    std::vector<EditorBlueprintEntry> blueprints_;
    char searchBuf_[128]{};
};

} // namespace leon::editor
