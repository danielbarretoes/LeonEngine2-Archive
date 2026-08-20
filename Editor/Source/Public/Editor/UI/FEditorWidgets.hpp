#pragma once

#include "Editor/UI/FLucideIcons.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon::Editor {

    class FEditorWidgets {
    public:
        /**
         * @brief Renders a stylized 3-component vector control with RGB colored axis buttons and reset capability.
         */
        static bool DrawVec3Control(const std::string& InLabel, glm::vec3& InValues, float InResetValue = 0.0f,
                                    float InColumnWidth = 80.0f);

        /**
         * @brief Search input with a Lucide search icon on the left and optional clear button.
         */
        static bool DrawSearchInput(const char* InId, char* InBuffer, size_t InBufferSize,
                                    const char* InHint = "Search...");

        /**
         * @brief Toolbar button with Lucide icon + label (shared format for Add / Import / Save All, etc.).
         */
        static bool DrawToolbarButton(ELucideIcon InIcon, const char* InLabel, const char* InId,
                                      float InHeight = 26.0f);

        /**
         * @brief Icon-only toolbar button (Back / Forward / Settings, etc.).
         */
        static bool DrawToolbarIconButton(ELucideIcon InIcon, const char* InId, bool bEnabled = true,
                                          float InSize = 26.0f);

        /**
         * @brief Begin a dockable panel and draw a Lucide icon left of the title / tab label.
         * Prefer titles with a couple of leading spaces so the label does not cover the icon.
         */
        static bool BeginPanelWindow(const char* InTitle, bool* bInOutOpen, ELucideIcon InIcon,
                                     ImGuiWindowFlags InFlags = 0);
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorWidgets;
} // namespace Leon
