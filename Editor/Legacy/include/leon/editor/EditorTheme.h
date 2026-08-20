#pragma once

struct ImFont;

namespace leon::editor {

/// Apply Unreal Editor 5–inspired Dear ImGui colors and sizing (call once after CreateContext).
void ApplyEditorTheme();

/// Load Inter from `Engine/Assets/Fonts/` (call after CreateContext, before first NewFrame).
/// Returns false if Inter was missing and the default font was used instead.
[[nodiscard]] bool LoadEditorFonts();

/// Inter-Bold for world-fixed TextRender labels in the viewport (may equal default if missing).
[[nodiscard]] ImFont* GetEditorTextRenderLabelFont();

} // namespace leon::editor
