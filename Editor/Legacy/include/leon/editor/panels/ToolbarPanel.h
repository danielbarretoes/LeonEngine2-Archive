#pragma once

#include <leon/editor/EditorContext.h>

namespace leon::editor {

/// Top toolbar: Unreal-like Play / Pause / Stop and Build Lighting.
class ToolbarPanel {
public:
    void Draw(EditorContext& ctx);
    /// Shared Play/Pause/Stop controls (also used from the main menu bar).
    static void DrawPlayControls(EditorContext& ctx);
};

} // namespace leon::editor
