#pragma once

#include <leon/editor/EditorContext.h>
#include <string>
#include <vector>

namespace leon::editor {

/// User editor prefs (Play settings, grid, etc.) beside the executable — not in project Config/.
struct EditorUserPreferences {
    bool showGrid = true;
    bool showStats = false;
    bool snapEnabled = true;
    bool surfaceSnapEnabled = true;
    float gridSize = 1.0f;
    float rotationSnapDegrees = 15.0f;
    EEditorPlayMode playMode = EEditorPlayMode::SelectedViewport;
    int pieNumberOfPlayers = 1;
    EEditorPlayNetMode pieNetMode = EEditorPlayNetMode::Standalone;
    std::string pieClientAddress = "127.0.0.1";
    EEditorPieAspect pieAspect = EEditorPieAspect::Fit16x9;
    EEditorViewportViewMode viewportViewMode = EEditorViewportViewMode::Lit;
    EGizmoOperation gizmoOp = EGizmoOperation::Translate;
    EGizmoSpace gizmoSpace = EGizmoSpace::Local;
    std::vector<std::string> contentFavoritePaths;

    [[nodiscard]] bool Equals(const EditorUserPreferences& other) const;
};

namespace EditorPreferences {

[[nodiscard]] std::string FilePath();
[[nodiscard]] EditorUserPreferences Capture(const EditorContext& ctx);
void Apply(EditorContext& ctx, const EditorUserPreferences& prefs);
[[nodiscard]] EditorUserPreferences Load();
void Save(const EditorUserPreferences& prefs);

/// Load → apply once; call Save when Capture(ctx) differs from the last snapshot.
void LoadInto(EditorContext& ctx);
/// Persist only if ctx prefs changed since `ioLastSaved` (updates snapshot on write).
void SaveIfChanged(const EditorContext& ctx, EditorUserPreferences& ioLastSaved);

} // namespace EditorPreferences

} // namespace leon::editor
