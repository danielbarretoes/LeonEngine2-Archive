#pragma once

namespace leon {

class Level;

namespace editor {

struct EditorContext;

/// Output Log manifest when the Visual Showcase project / level is opened or played in-editor.
void LogShowcaseEditorManifest(const EditorContext& ctx, const Level& level);

[[nodiscard]] bool IsShowcaseProject(std::string_view projectName);

} // namespace editor
} // namespace leon
