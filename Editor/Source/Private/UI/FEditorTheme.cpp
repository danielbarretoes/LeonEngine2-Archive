#include "Editor/UI/FEditorTheme.hpp"
#include "Core/FLog.hpp"

#include <algorithm>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Leon::Editor {

    ImFont* FEditorTheme::FontRegular = nullptr;
    ImFont* FEditorTheme::FontBold = nullptr;
    ImFont* FEditorTheme::FontSmall = nullptr;
    FUiTokens FEditorTheme::Tokens = FUiTokens::Default();

    FUiTokens FUiTokens::Default() {
        return FUiTokens{};
    }

    const FUiTokens& FEditorTheme::GetTokens() {
        return Tokens;
    }

    void FEditorTheme::SetTokens(const FUiTokens& InTokens) {
        Tokens = InTokens;
        if (!Tokens.Font)
            Tokens.Font = FontRegular;
        if (!Tokens.FontBold)
            Tokens.FontBold = FontBold ? FontBold : FontRegular;
        if (!Tokens.FontSmall)
            Tokens.FontSmall = FontSmall ? FontSmall : FontRegular;
        ApplyTokensToImGuiStyle(Tokens);
    }

    void FEditorTheme::ResetTokens() {
        Tokens = FUiTokens::Default();
        Tokens.Font = FontRegular;
        Tokens.FontBold = FontBold ? FontBold : FontRegular;
        Tokens.FontSmall = FontSmall ? FontSmall : FontRegular;
        ApplyTokensToImGuiStyle(Tokens);
    }

    ImFont* FEditorTheme::ResolveFont(ImFont* InOverride) {
        if (InOverride)
            return InOverride;
        if (Tokens.Font)
            return Tokens.Font;
        return FontRegular ? FontRegular : ImGui::GetFont();
    }

    ImFont* FEditorTheme::ResolveBoldFont(ImFont* InOverride) {
        if (InOverride)
            return InOverride;
        if (Tokens.FontBold)
            return Tokens.FontBold;
        return FontBold ? FontBold : ResolveFont();
    }

    ImFont* FEditorTheme::ResolveSmallFont(ImFont* InOverride) {
        if (InOverride)
            return InOverride;
        if (Tokens.FontSmall)
            return Tokens.FontSmall;
        return FontSmall ? FontSmall : ResolveFont();
    }

    ImU32 FEditorTheme::ToU32(const ImVec4& InColor) {
        return ImGui::ColorConvertFloat4ToU32(InColor);
    }

    ImVec4 FEditorTheme::Lighten(const ImVec4& InColor, float InAmount) {
        return ImVec4(std::min(1.0f, InColor.x + InAmount), std::min(1.0f, InColor.y + InAmount),
                      std::min(1.0f, InColor.z + InAmount), InColor.w);
    }

    ImVec4 FEditorTheme::WithAlpha(const ImVec4& InColor, float InAlpha) {
        return ImVec4(InColor.x, InColor.y, InColor.z, InAlpha);
    }

    void FEditorTheme::ApplyTokensToImGuiStyle(const FUiTokens& InTokens) {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = InTokens.Radius + 1.0f;
        style.ChildRounding = InTokens.Radius;
        style.FrameRounding = InTokens.Radius;
        style.GrabRounding = InTokens.Radius;
        style.PopupRounding = InTokens.Radius + 1.0f;
        style.ScrollbarRounding = InTokens.Radius + 1.0f;
        style.TabRounding = InTokens.Radius;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.WindowMenuButtonPosition = ImGuiDir_None;

        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 16.0f;
        style.ScrollbarSize = 13.0f;
        style.GrabMinSize = 10.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = InTokens.Foreground;
        colors[ImGuiCol_TextDisabled] = InTokens.MutedForeground;
        colors[ImGuiCol_WindowBg] = InTokens.Background;
        colors[ImGuiCol_ChildBg] = InTokens.Card;
        colors[ImGuiCol_PopupBg] = InTokens.Popover;
        colors[ImGuiCol_Border] = InTokens.Border;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = InTokens.Frame;
        colors[ImGuiCol_FrameBgHovered] = InTokens.FrameHover;
        colors[ImGuiCol_FrameBgActive] = InTokens.FrameActive;
        colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = InTokens.Background;
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.46f, 1.00f);
        colors[ImGuiCol_CheckMark] = InTokens.Accent;
        colors[ImGuiCol_SliderGrab] = InTokens.Accent;
        colors[ImGuiCol_SliderGrabActive] = Lighten(InTokens.Accent, 0.12f);
        colors[ImGuiCol_Button] = InTokens.Primary;
        colors[ImGuiCol_ButtonHovered] = InTokens.PrimaryHover;
        colors[ImGuiCol_ButtonActive] = InTokens.PrimaryActive;
        colors[ImGuiCol_Header] = InTokens.Primary;
        colors[ImGuiCol_HeaderHovered] = InTokens.PrimaryHover;
        colors[ImGuiCol_HeaderActive] = InTokens.PrimaryActive;
        colors[ImGuiCol_Separator] = InTokens.Border;
        colors[ImGuiCol_SeparatorHovered] = WithAlpha(InTokens.Accent, 0.78f);
        colors[ImGuiCol_SeparatorActive] = InTokens.Accent;
        colors[ImGuiCol_ResizeGrip] = WithAlpha(InTokens.Accent, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = WithAlpha(InTokens.Accent, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = WithAlpha(InTokens.Accent, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered] = InTokens.PrimaryHover;
        colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
        colors[ImGuiCol_DockingPreview] = WithAlpha(InTokens.Accent, 0.70f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    }

    void FEditorTheme::ApplyTheme() {
        if (!Tokens.Font)
            Tokens.Font = FontRegular;
        if (!Tokens.FontBold)
            Tokens.FontBold = FontBold ? FontBold : FontRegular;
        if (!Tokens.FontSmall)
            Tokens.FontSmall = FontSmall ? FontSmall : FontRegular;
        ApplyTokensToImGuiStyle(Tokens);
    }

    void FEditorTheme::LoadFonts(ImGuiIO& InIO, const std::string& InResourceRoot) {
        std::vector<std::string> searchRoots = {
            InResourceRoot,    "Editor/Resources/Fonts", "../Editor/Resources/Fonts", "../../Editor/Resources/Fonts",
            "Resources/Fonts",
        };

#ifdef _WIN32
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
            fs::path exeDir = fs::path(exePath).parent_path();
            searchRoots.push_back((exeDir / "Resources" / "Fonts").string());
            searchRoots.push_back((exeDir / "Editor" / "Resources" / "Fonts").string());
            searchRoots.push_back((exeDir / ".." / ".." / "Editor" / "Resources" / "Fonts").string());
        }
#endif

        const char* envRoot = std::getenv("LEON_ENGINE_ROOT");
        if (envRoot && envRoot[0] != '\0') {
            searchRoots.push_back((fs::path(envRoot) / "Editor" / "Resources" / "Fonts").string());
        }

        std::string regularTtf;
        std::string boldTtf;

        for (const auto& root : searchRoots) {
            if (root.empty())
                continue;
            try {
                fs::path reg = fs::path(root) / "Inter-Regular.ttf";
                fs::path bld = fs::path(root) / "Inter-Bold.ttf";
                if (fs::exists(reg)) {
                    regularTtf = fs::canonical(reg).string();
                }
                if (fs::exists(bld)) {
                    boldTtf = fs::canonical(bld).string();
                }
                if (!regularTtf.empty())
                    break;
            } catch (...) {
            }
        }

        if (!regularTtf.empty()) {
            ImFontConfig config;
            config.OversampleH = 3;
            config.OversampleV = 3;
            config.PixelSnapH = true;

            FontRegular = InIO.Fonts->AddFontFromFileTTF(regularTtf.c_str(), 15.0f, &config);
            if (!boldTtf.empty()) {
                FontBold = InIO.Fonts->AddFontFromFileTTF(boldTtf.c_str(), 15.0f, &config);
            } else {
                FontBold = FontRegular;
            }
            FontSmall = InIO.Fonts->AddFontFromFileTTF(regularTtf.c_str(), 13.0f, &config);

            InIO.FontDefault = FontRegular;
            Tokens.Font = FontRegular;
            Tokens.FontBold = FontBold;
            Tokens.FontSmall = FontSmall;
            LE_CORE_INFO("FEditorTheme: Loaded Inter fonts from '{}'", regularTtf);
        } else {
            InIO.Fonts->AddFontDefault();
            LE_CORE_WARN("FEditorTheme: Inter font not found on disk, using ImGui default font");
        }
    }

} // namespace Leon::Editor
