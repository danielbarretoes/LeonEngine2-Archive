#pragma once

#include "Core/Base.hpp"
#include <imgui.h>
#include <string>

namespace Leon::Editor {

    /**
     * @brief Unreal Engine-inspired editor theme and typography configuration.
     */
    class FEditorTheme {
    public:
        static void ApplyTheme();
        static void LoadFonts(ImGuiIO& InIO, const std::string& InResourceRoot = "");

        static ImFont* FontRegular;
        static ImFont* FontBold;
        static ImFont* FontSmall;
    };

} // namespace Leon::Editor
