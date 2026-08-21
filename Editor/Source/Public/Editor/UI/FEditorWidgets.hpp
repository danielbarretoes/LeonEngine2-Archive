#pragma once

#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Leon::Editor {

    /**
     * Dock / Begin() window names. Leading spaces reserve room for the Lucide
     * tab icon drawn by BeginPanelWindow (must stay identical for DockBuilder).
     */
    struct FPanelWindowTitles {
        static constexpr const char* PlaceActors = "     Place Actors";
        static constexpr const char* Viewport = "     Viewport";
        static constexpr const char* WorldOutliner = "     World Outliner";
        static constexpr const char* WorldSettings = "     World Settings";
        static constexpr const char* ProjectSettings = "     Project Settings";
        static constexpr const char* Details = "     Details";
        static constexpr const char* ContentBrowser = "     Content Browser";
        static constexpr const char* OutputLog = "     Output Log";
    };

    /**
     * Shared editor UI kit (shadcn-style): themed controls with token defaults
     * and optional FControlStyle overrides (color, font, width).
     */
    class FEditorWidgets {
    public:
        // --- Layout ---
        static bool BeginPanelWindow(const char* InTitle, bool* bInOutOpen, ELucideIcon InIcon,
                                     ImGuiWindowFlags InFlags = 0);

        static void BeginPropertyGrid(float InLabelWidth = -1.0f);
        static void EndPropertyGrid();
        static void BeginProperty(const char* InLabel);
        static void EndProperty();

        // --- Inputs ---
        static bool DrawVec3Control(const std::string& InLabel, glm::vec3& InValues, float InResetValue = 0.0f,
                                    float InColumnWidth = 80.0f, const FControlStyle* InStyle = nullptr);

        static bool DrawSearchInput(const char* InId, char* InBuffer, size_t InBufferSize,
                                    const char* InHint = "Search...", const FControlStyle* InStyle = nullptr);

        static bool DrawInputText(const char* InLabel, const char* InId, char* InBuffer, size_t InBufferSize,
                                  const FControlStyle* InStyle = nullptr);

        // --- Reset ---
        /** Square refresh button (Unreal-style reset-to-default). */
        static bool DrawResetToDefaultButton(const char* InId, bool bEnabled = true,
                                             const char* InTooltip = "Reset to Default");

        // --- Select / Dropdown ---
        /** Combobox (select). Returns true when selection changes. */
        static bool DrawSelect(const char* InId, int* InOutIndex, const char* const* InItems, int InCount,
                               const FControlStyle* InStyle = nullptr);

        /** Label + select inside an open property grid (Columns). */
        static bool DrawPropertySelect(const char* InLabel, const char* InId, int* InOutIndex,
                                       const char* const* InItems, int InCount,
                                       const FControlStyle* InStyle = nullptr, const int* InDefaultIndex = nullptr);

        /**
         * Class-name combobox. If InNoneLabel != nullptr, index 0 is None (empty string).
         * When InDefaultClassName != nullptr, shows reset (typically empty for map overrides).
         */
        static bool DrawPropertyClassSelect(const char* InLabel, const char* InId, std::string& InOutClassName,
                                            const std::vector<std::string>& InClassNames,
                                            const char* InNoneLabel = nullptr,
                                            const std::string* InDefaultClassName = nullptr,
                                            const FControlStyle* InStyle = nullptr);

        /** Dropdown menu triggered by a button; returns selected index or -1. */
        static int DrawDropdown(const char* InId, const char* InPreviewLabel, const char* const* InItems,
                                int InCount, int InCurrentIndex = -1, const FControlStyle* InStyle = nullptr);

        // --- Slider / Drag ---
        static bool DrawSliderFloat(const char* InId, float* InOutValue, float InMin, float InMax,
                                    const char* InFormat = "%.2f", const FControlStyle* InStyle = nullptr);
        static bool DrawPropertySliderFloat(const char* InLabel, const char* InId, float* InOutValue, float InMin,
                                            float InMax, const char* InFormat = "%.2f",
                                            const FControlStyle* InStyle = nullptr,
                                            const float* InDefaultValue = nullptr);

        static bool DrawDragFloat(const char* InId, float* InOutValue, float InSpeed = 0.1f, float InMin = 0.0f,
                                  float InMax = 0.0f, const char* InFormat = "%.2f",
                                  const FControlStyle* InStyle = nullptr);
        static bool DrawPropertyDragFloat(const char* InLabel, const char* InId, float* InOutValue,
                                          float InSpeed = 0.1f, float InMin = 0.0f, float InMax = 0.0f,
                                          const char* InFormat = "%.2f", const FControlStyle* InStyle = nullptr,
                                          const float* InDefaultValue = nullptr);

        static bool DrawDragInt(const char* InId, int* InOutValue, float InSpeed = 1.0f, int InMin = 0, int InMax = 0,
                                const FControlStyle* InStyle = nullptr);
        static bool DrawPropertyDragInt(const char* InLabel, const char* InId, int* InOutValue, float InSpeed = 1.0f,
                                        int InMin = 0, int InMax = 0, const FControlStyle* InStyle = nullptr,
                                        const int* InDefaultValue = nullptr);

        // --- Checkbox / Color / Button ---
        static bool DrawCheckbox(const char* InId, bool* InOutValue, const char* InLabel = nullptr,
                                 const FControlStyle* InStyle = nullptr);
        static bool DrawPropertyCheckbox(const char* InLabel, const char* InId, bool* InOutValue,
                                         const FControlStyle* InStyle = nullptr, const bool* InDefaultValue = nullptr);

        static bool DrawColorEdit3(const char* InId, float* InOutRgb, const FControlStyle* InStyle = nullptr);
        static bool DrawPropertyColorEdit3(const char* InLabel, const char* InId, float* InOutRgb,
                                           const FControlStyle* InStyle = nullptr,
                                           const float* InDefaultRgb = nullptr);

        /**
         * Button with required Lucide icon. Pass nullptr/empty InLabel for icon-only.
         */
        static bool DrawButton(ELucideIcon InIcon, const char* InId, const char* InLabel = nullptr,
                               const ImVec2& InSize = ImVec2(0, 0), const FControlStyle* InStyle = nullptr);
        static bool DrawPrimaryButton(ELucideIcon InIcon, const char* InId, const char* InLabel = nullptr,
                                      const ImVec2& InSize = ImVec2(0, 0), const FControlStyle* InStyle = nullptr);
        static bool DrawSuccessButton(ELucideIcon InIcon, const char* InId, const char* InLabel = nullptr,
                                      const ImVec2& InSize = ImVec2(0, 0), const FControlStyle* InStyle = nullptr);
        static bool DrawToggleButton(ELucideIcon InIcon, const char* InId, bool bActive,
                                     const char* InLabel = nullptr, const ImVec2& InSize = ImVec2(0, 0),
                                     const FControlStyle* InStyle = nullptr);

        /**
         * Segmented control. InIcons required. InLabels may be null (icon-only segments)
         * or an array of labels (icon + label).
         */
        static bool DrawSegmentedControl(const char* InId, int* InOutIndex, const ELucideIcon* InIcons,
                                         const char* const* InLabels, int InCount,
                                         const FControlStyle* InStyle = nullptr);

        // --- Toolbar ---
        static bool DrawToolbarButton(ELucideIcon InIcon, const char* InLabel, const char* InId,
                                      float InHeight = 26.0f, const FControlStyle* InStyle = nullptr);
        static bool DrawToolbarIconButton(ELucideIcon InIcon, const char* InId, bool bEnabled = true,
                                          float InSize = 26.0f, const FControlStyle* InStyle = nullptr);
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorWidgets;
    using Editor::FControlStyle;
    using Editor::FPanelWindowTitles;
    using Editor::FUiTokens;
} // namespace Leon
