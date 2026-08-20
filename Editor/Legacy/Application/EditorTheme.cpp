#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorTheme.h>
#include <leon/editor/ui/UiTokens.h>
#include <string>
#include <system_error>

namespace leon::editor {
namespace {

[[nodiscard]] std::string ResolveEditorFontPath(const char* fileName) {
    const std::filesystem::path exeDir = ExecutableDirectory();
    const std::string candidates[] = {
        (exeDir / "Engine" / "Assets" / "Fonts" / fileName).generic_string(),
        ResolveAssetPath(std::string("assets/Fonts/") + fileName),
        (exeDir / ".." / ".." / "Engine" / "Assets" / "Fonts" / fileName).generic_string(),
        (std::filesystem::path("Engine") / "Assets" / "Fonts" / fileName).generic_string(),
    };
    for (const std::string& path : candidates) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && !ec) {
            return path;
        }
    }
    return {};
}

} // namespace

ImFont* gTextRenderLabelFont = nullptr;

void ApplyEditorTheme() {
    ui::ApplyImGuiStyle();
}

bool LoadEditorFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const std::string regularPath = ResolveEditorFontPath("Inter-Regular.ttf");
    const std::string mediumPath = ResolveEditorFontPath("Inter-Medium.ttf");
    const float sizePx = ui::Typography().bodySize;

    static const ImWchar kGlyphRanges[] = {
        0x0020, 0x00FF, 0x2010, 0x2027, 0x2190, 0x2193, 0,
    };

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    config.PixelSnapH = true;
    config.GlyphRanges = kGlyphRanges;

    ImFont* regular = nullptr;
    if (!regularPath.empty()) {
        regular = io.Fonts->AddFontFromFileTTF(regularPath.c_str(), sizePx, &config);
    }
    if (regular == nullptr) {
        regular = io.Fonts->AddFontDefault();
        std::cerr << "Editor: Inter-Regular.ttf not found; using ImGui default font\n";
        return false;
    }

    ImFont* medium = regular;
    if (!mediumPath.empty()) {
        ImFontConfig mediumCfg = config;
        mediumCfg.MergeMode = false;
        ImFont* loadedMedium = io.Fonts->AddFontFromFileTTF(mediumPath.c_str(), sizePx, &mediumCfg);
        if (loadedMedium != nullptr) {
            medium = loadedMedium;
        }
    }

    const std::string boldPath = ResolveEditorFontPath("Inter-Bold.ttf");
    if (!boldPath.empty()) {
        ImFontConfig boldCfg = config;
        boldCfg.MergeMode = false;
        gTextRenderLabelFont = io.Fonts->AddFontFromFileTTF(
            boldPath.c_str(), ui::Typography().titleSize + 1.0f, &boldCfg);
    }
    if (gTextRenderLabelFont == nullptr) {
        gTextRenderLabelFont = regular;
    }

    io.FontDefault = regular;
    ui::SetEditorFonts(regular, medium);
    return true;
}

ImFont* GetEditorTextRenderLabelFont() {
    if (gTextRenderLabelFont != nullptr) {
        return gTextRenderLabelFont;
    }
    return ImGui::GetFont();
}

} // namespace leon::editor
