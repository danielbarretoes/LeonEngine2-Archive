#include <imgui.h>
#include <leon/editor/ui/UiTokens.h>

namespace leon::editor::ui {
namespace {

UiPalette BuildDefaultPalette() {
    UiPalette p;
    p.bgWindow = Rgb8(24, 24, 24);
    p.bgPanel = Rgb8(32, 32, 32);
    p.bgDeep = Rgb8(18, 18, 18);
    p.bg = Rgb8(18, 18, 18);
    p.bgElevated = Rgb8(40, 40, 40);
    p.bgHover = Rgb8(56, 56, 56);
    p.bgActive = Rgb8(70, 70, 70);
    p.border = Rgb8(10, 10, 10);
    p.borderStrong = Rgb8(72, 72, 72);
    p.text = Rgb8(230, 230, 230);
    p.textMuted = Rgb8(140, 140, 140);
    p.textOnAccent = Rgb8(255, 255, 255);
    p.accent = Rgb8(0, 112, 224);
    p.accentHover = Rgb8(26, 140, 255);
    p.accentActive = Rgb8(0, 90, 180);
    p.accentMuted = Rgb8(0, 112, 224, 0.35f);
    p.danger = Rgb8(160, 40, 40);
    p.dangerHover = Rgb8(190, 55, 55);
    p.warning = Rgb8(180, 120, 30);
    p.warningHover = Rgb8(210, 145, 40);
    p.success = Rgb8(55, 160, 90);
    p.frame = Rgb8(48, 48, 48);
    p.frameHover = Rgb8(64, 64, 64);
    p.frameActive = Rgb8(80, 80, 80);
    p.tabIdle = Rgb8(26, 26, 26);
    p.tabActive = p.bgPanel;
    p.popupBg = Rgb8(28, 28, 28, 0.98f);
    p.bannerBg = Rgb8(20, 48, 78, 0.95f);
    p.graphBg = Rgb8(22, 22, 26);
    p.graphGrid = Rgb8(48, 48, 56, 0.45f);
    p.graphNodeBg = Rgb8(36, 36, 42);
    p.graphNodeHeader = Rgb8(44, 40, 56);
    p.graphPin = Rgb8(180, 140, 255);
    p.graphAccent = Rgb8(215, 190, 255);
    return p;
}

ImFont* g_fontBody = nullptr;
ImFont* g_fontMedium = nullptr;

} // namespace

ImVec4 Rgb8(int r, int g, int b, float a) {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

const UiPalette& Tokens() {
    static const UiPalette kPalette = BuildDefaultPalette();
    return kPalette;
}

const UiLayoutMetrics& Layout() {
    static const UiLayoutMetrics kLayout{};
    return kLayout;
}

const UiTypography& Typography() {
    static const UiTypography kType{};
    return kType;
}

const UiFonts& Fonts() {
    static UiFonts kFonts{};
    kFonts.body = g_fontBody;
    kFonts.medium = g_fontMedium != nullptr ? g_fontMedium : g_fontBody;
    return kFonts;
}

void SetEditorFonts(ImFont* body, ImFont* medium) {
    g_fontBody = body;
    g_fontMedium = medium;
}

void ApplyImGuiStyle() {
    const UiPalette& t = Tokens();
    const UiLayoutMetrics& L = Layout();
    const UiSizeMetrics md = SizeMetrics(EUiSize::Md);
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = L.panelPadding;
    style.FramePadding = L.chromeFramePadding;
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.ItemSpacing = L.itemSpacing;
    style.ItemInnerSpacing = L.itemInnerSpacing;
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = L.panelRounding;
    style.ChildRounding = 2.0f;
    style.FrameRounding = md.rounding;
    style.PopupRounding = L.popupRounding;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 2.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(10.0f, 2.0f);
    style.TabBarOverlineSize = L.tabBarOverline;
    style.DockingSeparatorSize = 2.0f;
    style.HoverStationaryDelay = 0.12f;
    style.HoverDelayShort = 0.12f;
    style.HoverDelayNormal = 0.35f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = t.text;
    c[ImGuiCol_TextDisabled] = t.textMuted;
    c[ImGuiCol_WindowBg] = t.bgWindow;
    c[ImGuiCol_ChildBg] = t.bgPanel;
    c[ImGuiCol_PopupBg] = t.popupBg;
    c[ImGuiCol_Border] = t.border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = t.frame;
    c[ImGuiCol_FrameBgHovered] = t.frameHover;
    c[ImGuiCol_FrameBgActive] = t.frameActive;
    c[ImGuiCol_TitleBg] = t.bgDeep;
    c[ImGuiCol_TitleBgActive] = t.bgDeep;
    c[ImGuiCol_TitleBgCollapsed] = t.bgDeep;
    c[ImGuiCol_MenuBarBg] = Rgb8(32, 32, 32);
    c[ImGuiCol_ScrollbarBg] = t.bgDeep;
    c[ImGuiCol_ScrollbarGrab] = Rgb8(72, 72, 72);
    c[ImGuiCol_ScrollbarGrabHovered] = Rgb8(96, 96, 96);
    c[ImGuiCol_ScrollbarGrabActive] = Rgb8(120, 120, 120);
    c[ImGuiCol_CheckMark] = t.accent;
    c[ImGuiCol_SliderGrab] = Rgb8(140, 140, 140);
    c[ImGuiCol_SliderGrabActive] = t.accent;
    c[ImGuiCol_Button] = t.frame;
    c[ImGuiCol_ButtonHovered] = t.bgHover;
    c[ImGuiCol_ButtonActive] = t.accentActive;
    c[ImGuiCol_Header] = t.frame;
    c[ImGuiCol_HeaderHovered] = t.bgHover;
    c[ImGuiCol_HeaderActive] = t.accentActive;
    c[ImGuiCol_Separator] = t.borderStrong;
    c[ImGuiCol_SeparatorHovered] = t.accentHover;
    c[ImGuiCol_SeparatorActive] = t.accent;
    c[ImGuiCol_ResizeGrip] = Rgb8(60, 60, 60, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = t.accentHover;
    c[ImGuiCol_ResizeGripActive] = t.accent;
    c[ImGuiCol_Tab] = t.tabIdle;
    c[ImGuiCol_TabHovered] = t.bgHover;
    c[ImGuiCol_TabSelected] = t.tabActive;
    c[ImGuiCol_TabSelectedOverline] = t.accent;
    c[ImGuiCol_TabDimmed] = t.bgDeep;
    c[ImGuiCol_TabDimmedSelected] = t.bgPanel;
    c[ImGuiCol_TabDimmedSelectedOverline] = t.accentActive;
    c[ImGuiCol_DockingPreview] = Rgb8(0, 112, 224, 0.45f);
    c[ImGuiCol_DockingEmptyBg] = t.bgDeep;
    c[ImGuiCol_PlotLines] = Rgb8(180, 180, 180);
    c[ImGuiCol_PlotLinesHovered] = t.accentHover;
    c[ImGuiCol_PlotHistogram] = t.accentActive;
    c[ImGuiCol_PlotHistogramHovered] = t.accent;
    c[ImGuiCol_TableHeaderBg] = Rgb8(40, 40, 40);
    c[ImGuiCol_TableBorderStrong] = t.border;
    c[ImGuiCol_TableBorderLight] = Rgb8(40, 40, 40);
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = Rgb8(255, 255, 255, 0.02f);
    c[ImGuiCol_TextSelectedBg] = t.accentMuted;
    c[ImGuiCol_DragDropTarget] = t.accentHover;
    c[ImGuiCol_NavHighlight] = t.accent;
    c[ImGuiCol_NavWindowingHighlight] = Rgb8(255, 255, 255, 0.5f);
    c[ImGuiCol_NavWindowingDimBg] = Rgb8(0, 0, 0, 0.55f);
    c[ImGuiCol_ModalWindowDimBg] = Rgb8(0, 0, 0, 0.55f);
}

UiSizeMetrics SizeMetrics(EUiSize size) {
    switch (size) {
    case EUiSize::Sm:
        return UiSizeMetrics{24.0f, 8.0f, 4.0f, 12.0f, 6.0f, 3.0f};
    case EUiSize::Lg:
        return UiSizeMetrics{34.0f, 14.0f, 6.0f, 16.0f, 8.0f, 5.0f};
    case EUiSize::Md:
    default:
        return UiSizeMetrics{28.0f, 10.0f, 5.0f, 14.0f, 6.0f, 4.0f};
    }
}

ImU32 ToU32(const ImVec4& c) {
    return ImGui::ColorConvertFloat4ToU32(c);
}

} // namespace leon::editor::ui
