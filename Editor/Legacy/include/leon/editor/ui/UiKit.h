#pragma once

#include <imgui.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/ui/UiTokens.h>

namespace leon::editor::ui {

/// Optional Lucide glyph for button / select preview rows.
struct UiIcon {
    ELucideIcon id = ELucideIcon::Circle;
    bool present = false;

    UiIcon() = default;
    explicit UiIcon(ELucideIcon icon) : id(icon), present(true) {}
};

struct UiButtonDesc {
    const char* label = nullptr;
    UiIcon icon{};
    EUiSize size = EUiSize::Md;
    EUiVariant variant = EUiVariant::Default;
    bool disabled = false;
    bool stretch = false; ///< Fill remaining width in the line
    float width = 0.0f;   ///< >0 fixed width; 0 = content-sized
    const char* tooltip = nullptr;
};

struct UiPanelDesc {
    bool* pOpen = nullptr;
    ImGuiWindowFlags extraFlags = 0;
    bool compactPadding = false; ///< Toolbar-like tight padding
    bool zeroPadding = false;    ///< Viewport / render target (no inset)
};

/// Text / icon / icon+label button. `id` must be unique among siblings (##suffix ok).
[[nodiscard]] bool Button(const char* id, const UiButtonDesc& desc);

[[nodiscard]] bool Button(const char* id, const char* label,
                          EUiVariant variant = EUiVariant::Default, EUiSize size = EUiSize::Md);

[[nodiscard]] bool IconButton(const char* id, ELucideIcon icon,
                              EUiVariant variant = EUiVariant::Ghost, EUiSize size = EUiSize::Md,
                              const char* tooltip = nullptr);

/// Toolbar helper: icon (+ optional caption). Returns true if icon or caption was clicked.
[[nodiscard]] bool ToolbarButton(const char* id, ELucideIcon icon, const char* label = nullptr,
                                 const char* tooltip = nullptr,
                                 EUiVariant variant = EUiVariant::Ghost,
                                 EUiSize size = EUiSize::Md);

/// Press-stay toggle (segmented / filter chips). Uses Primary fill when `*value` is true.
[[nodiscard]] bool ToggleButton(const char* id, bool* value, const UiButtonDesc& desc = {});

struct UiSelectDesc {
    const char* preview = "";
    UiIcon previewIcon{};
    EUiSize size = EUiSize::Md;
    float width = 0.0f; ///< 0 = ImGui default item width; <0 stretch
    bool disabled = false;
    const char* tooltip = nullptr;
};

/// Styled combo opener. Pair with SelectItem / EndSelect (ImGui popup pattern).
[[nodiscard]] bool BeginSelect(const char* id, const UiSelectDesc& desc);
[[nodiscard]] bool SelectItem(const char* label, bool selected, UiIcon icon = {});
void EndSelect();

/// Checkbox with size-aware hit target and optional label.
[[nodiscard]] bool Checkbox(const char* id, const char* label, bool* value,
                            EUiSize size = EUiSize::Md);

/// Enum-style select from a parallel string table. Returns true when `*index` changes.
[[nodiscard]] bool SelectFromList(const char* id, int* index, const char* const* items, int count,
                                  EUiSize size = EUiSize::Md, float width = 0.0f);

/// Horizontal exclusive choice (toolbar / mode switcher).
[[nodiscard]] bool SegmentedControl(const char* id, int* index, const char* const* labels,
                                    int count, EUiSize size = EUiSize::Sm);

/// Fixed-width dialog action (Create / Cancel / Discard).
[[nodiscard]] bool DialogButton(const char* id, const char* label,
                                EUiVariant variant = EUiVariant::Default, float width = 120.0f);

/// Docked editor panel with consistent padding / spacing.
[[nodiscard]] bool BeginPanel(const char* title, const UiPanelDesc& desc = {});
void EndPanel();

/// Styled ImGui popup (Play settings, context menus with form layout).
[[nodiscard]] bool BeginPopupPanel(const char* id, float minWidth = 240.0f);
void EndPopupPanel();

/// Property-row label (Details / World Settings two-column forms).
void FieldLabel(const char* label, float width = 0.0f);

/// Label stacked above the control (modals, multiline fields).
void FieldLabelBlock(const char* label);

[[nodiscard]] bool SliderFloat(const char* id, const char* label, float* value, float vMin,
                               float vMax, const char* format = "%.3f", float width = 0.0f);

[[nodiscard]] bool DragFloat(const char* id, const char* label, float* value, float speed = 0.05f,
                             float vMin = 0.0f, float vMax = 0.0f, const char* format = "%.3f",
                             float width = 0.0f);

[[nodiscard]] bool DragFloat3(const char* id, const char* label, float* value, float speed = 0.05f,
                              float width = 0.0f);

[[nodiscard]] bool DragInt(const char* id, const char* label, int* value, float speed = 1.0f,
                           int vMin = 0, int vMax = 0, float width = 0.0f);

/// Push two-column form layout (label | control). Pair with PopFormLayout.
void PushFormLayout(float labelWidth = 0.0f);
void PopFormLayout();

void Spacing(EUiSpacing spacing);

/// Horizontal or vertical hairline separator.
void Separator(bool vertical = false);

/// Muted section caption (Apple Settings / UE details group label feel).
void SectionLabel(const char* label);

/// Disabled / hint text at control scale.
void Hint(const char* text);

/// UE-style foldout section (styled CollapsingHeader).
[[nodiscard]] bool CollapsingSection(const char* title,
                                     ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen);

/// Body / caption / title / error text (token colors + medium font for titles).
void TextBody(const char* text);
void TextMuted(const char* text);
void TextCaption(const char* text);
void TextTitle(const char* text);
void TextError(const char* text);
void TextPath(const char* path);

/// Read-only label | value row (Details metadata).
void LabelValue(const char* label, const char* value);

[[nodiscard]] bool ColorEdit3(const char* id, const char* label, float* rgb, float width = 0.0f);

[[nodiscard]] bool DragFloat2(const char* id, const char* label, float* value, float speed = 0.05f,
                              float width = 0.0f);

[[nodiscard]] bool DragFloat4(const char* id, const char* label, float* value, float speed = 0.05f,
                              float width = 0.0f);

/// Styled modal dialog shell (New Project, save prompts).
[[nodiscard]] bool BeginModal(const char* name, bool* pOpen, float minWidth = 360.0f);
void EndModal();

struct UiInputDesc {
    const char* hint = nullptr;
    EUiSize size = EUiSize::Md;
    float width = 0.0f; ///< <0 stretch; 0 = default control width
    bool disabled = false;
    ImGuiInputTextFlags flags = 0;
    ImGuiInputTextCallback callback = nullptr;
    void* callbackUserData = nullptr;
};

struct UiTextAreaDesc {
    EUiSize size = EUiSize::Md;
    float width = 0.0f;
    float minLines = 3.0f;
    bool disabled = false;
    ImGuiInputTextFlags flags = 0;
};

/// Single-line text field. `label` uses FieldLabel when non-empty; `id` is the control id
/// (##suffix).
[[nodiscard]] bool InputText(const char* id, const char* label, char* buf, size_t bufSize,
                             const UiInputDesc& desc = {});

[[nodiscard]] bool InputTextWithHint(const char* id, const char* label, char* buf, size_t bufSize,
                                     const char* hint, const UiInputDesc& desc = {});

/// Multiline text field (`TextArea`). Height scales with `minLines`.
[[nodiscard]] bool TextArea(const char* id, const char* label, char* buf, size_t bufSize,
                            const UiTextAreaDesc& desc = {});

/// Compact integer field for graph pins / ids.
[[nodiscard]] bool InputInt(const char* id, const char* label, int* value, int step = 1,
                            int stepFast = 100, ImGuiInputTextFlags flags = 0, float width = 0.0f,
                            EUiSize size = EUiSize::Md);

/// Search row with Lucide search icon (Outliner / Content Browser / Place Actors).
[[nodiscard]] bool SearchField(const char* id, char* buf, size_t bufSize,
                               const char* hint = "Search…", float width = -1.0f);

/// Material / blueprint graph canvas state (scrollable child with grid).
struct UiGraphCanvas {
    ImVec2 origin{};
    ImVec2 size{};
    ImVec2 scroll{};
    ImDrawList* draw = nullptr;
    ImGuiID id = 0;
};

struct UiGraphCanvasDesc {
    ImVec2 size = ImVec2(0.0f, 0.0f); ///< 0 = fill avail on that axis
    float gridStep = 24.0f;
    bool showGrid = true;
    const char* emptyHint = nullptr;
};

[[nodiscard]] bool BeginGraphCanvas(const char* id, const UiGraphCanvasDesc& desc,
                                    UiGraphCanvas* out = nullptr);
void EndGraphCanvas();

struct UiGraphNodeDesc {
    bool selected = false;
    ImVec4 accent = ImVec4(0, 0, 0, 0); ///< Zero alpha → material graph accent
    float width = 0.0f;                 ///< 0 = stretch
};

/// Styled node card (expression block, blueprint node lite).
[[nodiscard]] bool BeginGraphNode(const char* id, const char* title,
                                  const UiGraphNodeDesc& desc = {});
void EndGraphNode();

/// Pin dot + label + integer wiring field (material outputs).
[[nodiscard]] bool GraphPinInt(const char* id, const char* label, int* value, float width = 0.0f);

/// Graph panel section title (Outputs / Expressions).
void GraphSectionHeader(const char* title, const char* subtitle = nullptr);

} // namespace leon::editor::ui
