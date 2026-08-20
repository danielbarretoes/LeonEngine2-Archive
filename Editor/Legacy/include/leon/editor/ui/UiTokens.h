#pragma once

#include <cstdint>
#include <imgui.h>

struct ImFont;
struct ImGuiStyle;

namespace leon::editor::ui {

/// Control height / padding / icon box (Unreal-like density).
enum class EUiSize : std::uint8_t {
    Sm = 0, ///< Toolbar, search, dense lists (~24 px)
    Md,     ///< Default panels / forms (~28 px)
    Lg,     ///< Primary actions / hero toolbar (~34 px)
};

/// Visual role — black Unreal / Apple-adjacent chrome.
enum class EUiVariant : std::uint8_t {
    Default = 0,
    Primary,
    Secondary,
    Ghost,
    Outline,
    Destructive,
    Warning,
};

enum class EUiSpacing : std::uint8_t {
    Xs = 0,
    Sm,
    Md,
    Lg,
};

struct UiSizeMetrics {
    float height = 28.0f;
    float padX = 10.0f;
    float padY = 5.0f;
    float icon = 14.0f;
    float iconGap = 6.0f;
    float rounding = 4.0f;
};

/// Editor typography — single body size; hierarchy via font weight + color.
struct UiTypography {
    float bodySize = 14.0f;
    float titleSize = 14.0f;
    float captionSize = 13.0f;
};

struct UiFonts {
    ImFont* body = nullptr;
    ImFont* medium = nullptr; ///< Section headers / emphasis
};

/// Single source of truth for editor chrome colors (UiKit + ImGui theme).
struct UiPalette {
    ImVec4 bgWindow{};
    ImVec4 bgPanel{};
    ImVec4 bgDeep{};
    ImVec4 bg{};
    ImVec4 bgElevated{};
    ImVec4 bgHover{};
    ImVec4 bgActive{};
    ImVec4 border{};
    ImVec4 borderStrong{};
    ImVec4 text{};
    ImVec4 textMuted{};
    ImVec4 textOnAccent{};
    ImVec4 accent{};
    ImVec4 accentHover{};
    ImVec4 accentActive{};
    ImVec4 accentMuted{};
    ImVec4 danger{};
    ImVec4 dangerHover{};
    ImVec4 warning{};
    ImVec4 warningHover{};
    ImVec4 success{};
    ImVec4 frame{};
    ImVec4 frameHover{};
    ImVec4 frameActive{};
    ImVec4 tabIdle{};
    ImVec4 tabActive{};
    ImVec4 popupBg{};
    ImVec4 bannerBg{};
    ImVec4 graphBg{};
    ImVec4 graphGrid{};
    ImVec4 graphNodeBg{};
    ImVec4 graphNodeHeader{};
    ImVec4 graphPin{};
    ImVec4 graphAccent{};
};

struct UiLayoutMetrics {
    ImVec2 panelPadding{12.0f, 10.0f};
    ImVec2 compactPanelPadding{8.0f, 6.0f};
    ImVec2 zeroPanelPadding{0.0f, 0.0f};
    ImVec2 popupPadding{12.0f, 10.0f};
    ImVec2 modalPadding{16.0f, 14.0f};
    ImVec2 welcomePadding{24.0f, 20.0f};
    ImVec2 toastPadding{12.0f, 10.0f};
    /// Dock tab strip + menu bar frame padding (ImGui chrome — not UiKit control rows).
    ImVec2 chromeFramePadding{8.0f, 3.0f};
    ImVec2 itemSpacing{8.0f, 6.0f};
    ImVec2 formRowSpacing{10.0f, 8.0f};
    ImVec2 itemInnerSpacing{6.0f, 4.0f};
    ImVec2 graphNodePadding{10.0f, 8.0f};
    ImVec2 graphNodeItemSpacing{8.0f, 6.0f};
    float labelWidth = 132.0f;
    float controlMinWidth = 160.0f;
    float sectionGap = 10.0f;
    float sectionLabelGap = 4.0f;
    float sectionIndent = 0.0f;
    float hintGap = 4.0f;
    float separatorMargin = 4.0f;
    float segmentedGap = 2.0f;
    float popupOffset = 4.0f;
    float tabBarOverline = 1.5f;
    float spacingXs = 4.0f;
    float spacingSm = 8.0f;
    float spacingMd = 12.0f;
    float spacingLg = 16.0f;
    float popupRounding = 6.0f;
    float panelRounding = 0.0f;
    float modalRounding = 8.0f;
};

[[nodiscard]] const UiPalette& Tokens();
[[nodiscard]] const UiLayoutMetrics& Layout();
[[nodiscard]] const UiTypography& Typography();
[[nodiscard]] const UiFonts& Fonts();

/// Register ImGui fonts after LoadEditorFonts (body = default UI, medium = section titles).
void SetEditorFonts(ImFont* body, ImFont* medium);

void ApplyImGuiStyle();

[[nodiscard]] UiSizeMetrics SizeMetrics(EUiSize size);

[[nodiscard]] ImU32 ToU32(const ImVec4& c);

[[nodiscard]] ImVec4 Rgb8(int r, int g, int b, float a = 1.0f);

} // namespace leon::editor::ui
