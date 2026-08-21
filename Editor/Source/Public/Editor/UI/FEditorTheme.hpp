#pragma once

#include "Core/Base.hpp"
#include <imgui.h>
#include <string>

namespace Leon::Editor {

    /**
     * Design tokens (shadcn-like): defaults from the editor theme; override any field
     * via FEditorTheme::SetTokens / per-control FControlStyle.
     */
    struct FUiTokens {
        // Semantic colors
        ImVec4 Background{0.11f, 0.11f, 0.13f, 1.00f};
        ImVec4 Foreground{0.92f, 0.92f, 0.94f, 1.00f};
        ImVec4 Muted{0.16f, 0.16f, 0.19f, 1.00f};
        ImVec4 MutedForeground{0.50f, 0.50f, 0.54f, 1.00f};
        ImVec4 Border{0.20f, 0.20f, 0.23f, 1.00f};
        ImVec4 Accent{0.26f, 0.59f, 0.98f, 1.00f};
        ImVec4 AccentForeground{1.00f, 1.00f, 1.00f, 1.00f};
        ImVec4 Primary{0.18f, 0.20f, 0.24f, 1.00f};
        ImVec4 PrimaryHover{0.26f, 0.30f, 0.38f, 1.00f};
        ImVec4 PrimaryActive{0.16f, 0.45f, 0.78f, 1.00f};
        ImVec4 Destructive{0.75f, 0.20f, 0.20f, 1.00f};
        ImVec4 Success{0.15f, 0.55f, 0.25f, 1.00f};
        ImVec4 Warning{1.00f, 0.75f, 0.25f, 1.00f};
        ImVec4 Frame{0.16f, 0.16f, 0.19f, 1.00f};
        ImVec4 FrameHover{0.22f, 0.22f, 0.26f, 1.00f};
        ImVec4 FrameActive{0.26f, 0.26f, 0.32f, 1.00f};
        ImVec4 Popover{0.13f, 0.13f, 0.15f, 0.96f};
        ImVec4 Card{0.14f, 0.14f, 0.16f, 1.00f};

        // Typography (nullptr = ImGui / loaded theme font)
        ImFont* Font = nullptr;
        ImFont* FontBold = nullptr;
        ImFont* FontSmall = nullptr;

        // Geometry
        float Radius = 3.0f;
        float PropertyLabelWidth = 100.0f;
        float ControlHeight = 26.0f;

        static FUiTokens Default();
    };

    /**
     * Optional per-control override. Unset fields keep FUiTokens defaults.
     */
    struct FControlStyle {
        bool bOverrideAccent = false;
        ImVec4 Accent{};
        bool bOverrideFrame = false;
        ImVec4 Frame{};
        bool bOverrideForeground = false;
        ImVec4 Foreground{};
        ImFont* Font = nullptr; ///< nullptr = tokens / default
        float Width = 0.0f;     ///< 0 = fill available
        float LabelWidth = -1.0f; ///< <0 = tokens.PropertyLabelWidth
        float Height = 0.0f;    ///< 0 = tokens.ControlHeight / frame height
    };

    class FEditorTheme {
    public:
        static void ApplyTheme();
        static void LoadFonts(ImGuiIO& InIO, const std::string& InResourceRoot = "");

        static const FUiTokens& GetTokens();
        static void SetTokens(const FUiTokens& InTokens);
        static void ResetTokens();

        static ImFont* ResolveFont(ImFont* InOverride = nullptr);
        static ImFont* ResolveBoldFont(ImFont* InOverride = nullptr);
        static ImFont* ResolveSmallFont(ImFont* InOverride = nullptr);

        static ImU32 ToU32(const ImVec4& InColor);
        static ImVec4 Lighten(const ImVec4& InColor, float InAmount);
        static ImVec4 WithAlpha(const ImVec4& InColor, float InAlpha);

        static ImFont* FontRegular;
        static ImFont* FontBold;
        static ImFont* FontSmall;

    private:
        static FUiTokens Tokens;
        static void ApplyTokensToImGuiStyle(const FUiTokens& InTokens);
    };

} // namespace Leon::Editor
