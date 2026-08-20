#include "Editor/UI/FEditorTheme.hpp"
#include "Core/FLog.hpp"

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

    void FEditorTheme::ApplyTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.TabRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;

        style.WindowPadding = ImVec2(8.0f, 8.0f);
        style.FramePadding = ImVec2(6.0f, 4.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 16.0f;
        style.ScrollbarSize = 13.0f;
        style.GrabMinSize = 10.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.15f, 0.96f);
        colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.23f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.26f, 0.32f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.11f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.46f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.69f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.45f, 0.78f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.22f, 0.42f, 0.68f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.13f, 0.13f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
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
            LE_CORE_INFO("FEditorTheme: Loaded Inter fonts from '{0}'", regularTtf);
        } else {
            InIO.Fonts->AddFontDefault();
            LE_CORE_WARN("FEditorTheme: Inter font not found on disk, using ImGui default font");
        }
    }

} // namespace Leon::Editor
