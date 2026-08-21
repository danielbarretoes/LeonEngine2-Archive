#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>

namespace Leon::Editor {

    namespace {

        struct FPushedControlStyle {
            int ColorCount = 0;
            bool bPushedFont = false;
            float Width = 0.0f;
        };

        ImVec4 ResolveAccent(const FControlStyle* InStyle) {
            const FUiTokens& t = FEditorTheme::GetTokens();
            if (InStyle && InStyle->bOverrideAccent)
                return InStyle->Accent;
            return t.Accent;
        }

        ImVec4 ResolveFrame(const FControlStyle* InStyle) {
            const FUiTokens& t = FEditorTheme::GetTokens();
            if (InStyle && InStyle->bOverrideFrame)
                return InStyle->Frame;
            return t.Frame;
        }

        ImVec4 ResolveForeground(const FControlStyle* InStyle) {
            const FUiTokens& t = FEditorTheme::GetTokens();
            if (InStyle && InStyle->bOverrideForeground)
                return InStyle->Foreground;
            return t.Foreground;
        }

        float ResolveLabelWidth(const FControlStyle* InStyle) {
            const FUiTokens& t = FEditorTheme::GetTokens();
            if (InStyle && InStyle->LabelWidth >= 0.0f)
                return InStyle->LabelWidth;
            return t.PropertyLabelWidth;
        }

        FPushedControlStyle PushControlColors(const FControlStyle* InStyle) {
            FPushedControlStyle pushed;
            const FUiTokens& t = FEditorTheme::GetTokens();
            const ImVec4 accent = ResolveAccent(InStyle);
            const ImVec4 frame = ResolveFrame(InStyle);
            const ImVec4 fg = ResolveForeground(InStyle);

            ImGui::PushStyleColor(ImGuiCol_Text, fg);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, frame);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, FEditorTheme::Lighten(frame, 0.06f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, FEditorTheme::Lighten(frame, 0.10f));
            ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, accent);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, FEditorTheme::Lighten(accent, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_Header, t.Primary);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, t.PrimaryHover);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, accent);
            ImGui::PushStyleColor(ImGuiCol_Button, t.Primary);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, t.PrimaryHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, t.PrimaryActive);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, t.Radius);
            pushed.ColorCount = 13;

            ImFont* font = FEditorTheme::ResolveFont(InStyle ? InStyle->Font : nullptr);
            if (font && font != ImGui::GetFont()) {
                ImGui::PushFont(font);
                pushed.bPushedFont = true;
            }

            if (InStyle && InStyle->Width > 0.0f)
                pushed.Width = InStyle->Width;
            return pushed;
        }

        void PopControlColors(const FPushedControlStyle& InPushed) {
            if (InPushed.bPushedFont)
                ImGui::PopFont();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(InPushed.ColorCount);
        }

        void ApplyItemWidth(const FPushedControlStyle& InPushed) {
            if (InPushed.Width > 0.0f)
                ImGui::SetNextItemWidth(InPushed.Width);
            else
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        }

        void DrawPanelTitleIcon(ELucideIcon InIcon) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (!window)
                return;

            // Slightly smaller than font so it sits inside the tab padding band.
            const float iconSz = ImGui::GetFontSize() * 0.78f;
            const ImU32 col = FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground);

            if (window->DockNode && window->DockNode->TabBar) {
                ImGuiTabBar* tabBar = window->DockNode->TabBar;
                for (int n = 0; n < tabBar->Tabs.Size; ++n) {
                    const ImGuiTabItem& tab = tabBar->Tabs[n];
                    if (tab.Window != window)
                        continue;

                    // Draw on the dock host (same layer as the tab bar). ForegroundDrawList
                    // would float above every window/popup.
                    ImDrawList* draw = (window->DockNode->HostWindow) ? window->DockNode->HostWindow->DrawList
                                                                     : ImGui::GetWindowDrawList();
                    if (!draw)
                        return;

                    const float scroll = tabBar->ScrollingAnim;
                    const ImRect tabRect(tabBar->BarRect.Min.x + tab.Offset - scroll, tabBar->BarRect.Min.y,
                                         tabBar->BarRect.Min.x + tab.Offset + tab.Width - scroll,
                                         tabBar->BarRect.Max.y);
                    if (tabRect.Max.x <= tabBar->BarRect.Min.x || tabRect.Min.x >= tabBar->BarRect.Max.x)
                        return;

                    const float padL = ImGui::GetStyle().FramePadding.x;
                    const float x = tabRect.Min.x + padL;
                    const float y = tabRect.Min.y + (tabRect.GetHeight() - iconSz) * 0.5f;

                    draw->PushClipRect(tabBar->BarRect.Min, tabBar->BarRect.Max, true);
                    draw->PushClipRect(tabRect.Min, tabRect.Max, true);
                    FLucideIcons::DrawIcon(draw, ImVec2(x, y), ImVec2(x + iconSz, y + iconSz), InIcon, col);
                    draw->PopClipRect();
                    draw->PopClipRect();
                    return;
                }
            }

            if (!(window->Flags & ImGuiWindowFlags_NoTitleBar)) {
                const ImRect title = window->TitleBarRect();
                const float padX = ImGui::GetStyle().FramePadding.x;
                const float x = title.Min.x + padX;
                const float y = title.Min.y + (title.GetHeight() - iconSz) * 0.5f;
                FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(x, y), ImVec2(x + iconSz, y + iconSz),
                                       InIcon, col);
            }
        }

        void DrawSelectChevron() {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float sz = ImGui::GetFontSize() * 0.75f;
            const float pad = 6.0f;
            const float x = max.x - pad - sz;
            const float y = min.y + (max.y - min.y - sz) * 0.5f;
            FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(x, y), ImVec2(x + sz, y + sz),
                                   ELucideIcon::ChevronDown, FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground));
        }

        const FControlStyle* StyleWithResetWidth(const FControlStyle* InStyle, FControlStyle& OutLocal) {
            const float btn = ImGui::GetFrameHeight();
            const float gap = 4.0f;
            OutLocal = InStyle ? *InStyle : FControlStyle{};
            OutLocal.Width = std::max(40.0f, ImGui::GetContentRegionAvail().x - btn - gap);
            return &OutLocal;
        }

        bool NearlyEqualFloat(float A, float B, float Eps = 1e-4f) {
            return std::fabs(A - B) <= Eps;
        }

    } // namespace

    bool FEditorWidgets::DrawResetToDefaultButton(const char* InId, bool bEnabled, const char* InTooltip) {
        ImGui::PushID(InId);
        const float size = ImGui::GetFrameHeight();
        if (!bEnabled)
            ImGui::BeginDisabled();
        const bool clicked = ImGui::Button("##ResetDefault", ImVec2(size, size));
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(min.x + 2.0f, min.y + 2.0f),
                               ImVec2(max.x - 2.0f, max.y - 2.0f), ELucideIcon::RefreshCw,
                               FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground));
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && InTooltip && InTooltip[0] != '\0')
            ImGui::SetTooltip("%s", InTooltip);
        if (!bEnabled)
            ImGui::EndDisabled();
        ImGui::PopID();
        return clicked && bEnabled;
    }

    bool FEditorWidgets::BeginPanelWindow(const char* InTitle, bool* bInOutOpen, ELucideIcon InIcon,
                                          ImGuiWindowFlags InFlags) {
        const bool open = ImGui::Begin(InTitle, bInOutOpen, InFlags);
        DrawPanelTitleIcon(InIcon);
        return open;
    }

    void FEditorWidgets::BeginPropertyGrid(float InLabelWidth) {
        const float w = InLabelWidth >= 0.0f ? InLabelWidth : FEditorTheme::GetTokens().PropertyLabelWidth;
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, w);
    }

    void FEditorWidgets::EndPropertyGrid() {
        ImGui::Columns(1);
    }

    void FEditorWidgets::BeginProperty(const char* InLabel) {
        ImGui::TextUnformatted(InLabel ? InLabel : "");
        ImGui::NextColumn();
    }

    void FEditorWidgets::EndProperty() {
        ImGui::NextColumn();
    }

    bool FEditorWidgets::DrawVec3Control(const std::string& InLabel, glm::vec3& InValues, float InResetValue,
                                         float InColumnWidth, const FControlStyle* InStyle) {
        bool bChanged = false;
        ImGui::PushID(InLabel.c_str());

        const float labelW = InStyle && InStyle->LabelWidth >= 0.0f ? InStyle->LabelWidth : InColumnWidth;
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, labelW);
        ImGui::TextUnformatted(InLabel.c_str());
        ImGui::NextColumn();

        auto pushed = PushControlColors(InStyle);

        float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        ImVec2 buttonSize = {lineHeight, lineHeight};
        float availableWidth = ImGui::GetContentRegionAvail().x - buttonSize.x;
        float itemWidth = (availableWidth - 16.0f) / 3.0f;

        if (ImGui::Button("##ResetAll", buttonSize)) {
            InValues = glm::vec3(InResetValue);
            bChanged = true;
        }
        ImVec2 resetBtnMin = ImGui::GetItemRectMin();
        ImVec2 resetBtnMax = ImGui::GetItemRectMax();
        FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(resetBtnMin.x + 2.0f, resetBtnMin.y + 2.0f),
                               ImVec2(resetBtnMax.x - 2.0f, resetBtnMax.y - 2.0f), ELucideIcon::RefreshCw,
                               FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Reset all components to default");

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.75f, 0.20f, 0.20f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.90f, 0.30f, 0.30f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.65f, 0.15f, 0.15f, 1.0f});
        if (ImGui::Button("X", buttonSize)) {
            InValues.x = InResetValue;
            bChanged = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##X", &InValues.x, 0.1f, 0.0f, 0.0f, "%.2f"))
            bChanged = true;
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.20f, 0.65f, 0.20f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.30f, 0.80f, 0.30f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.55f, 0.15f, 1.0f});
        if (ImGui::Button("Y", buttonSize)) {
            InValues.y = InResetValue;
            bChanged = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##Y", &InValues.y, 0.1f, 0.0f, 0.0f, "%.2f"))
            bChanged = true;
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.20f, 0.35f, 0.85f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.30f, 0.45f, 0.95f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.15f, 0.25f, 0.75f, 1.0f});
        if (ImGui::Button("Z", buttonSize)) {
            InValues.z = InResetValue;
            bChanged = true;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::DragFloat("##Z", &InValues.z, 0.1f, 0.0f, 0.0f, "%.2f"))
            bChanged = true;

        PopControlColors(pushed);
        ImGui::Columns(1);
        ImGui::PopID();
        return bChanged;
    }

    bool FEditorWidgets::DrawSearchInput(const char* InId, char* InBuffer, size_t InBufferSize, const char* InHint,
                                         const FControlStyle* InStyle) {
        bool bChanged = false;
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);

        const float height = ImGui::GetFrameHeight();
        const float iconPad = 6.0f;
        const float leftPad = height;
        const bool bHasText = (InBuffer && InBuffer[0] != '\0');
        const float clearW = bHasText ? (height + 4.0f) : 0.0f;
        const float availWidth = pushed.Width > 0.0f ? pushed.Width : ImGui::GetContentRegionAvail().x;
        const float inputWidth = std::max(40.0f, availWidth - clearW);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(leftPad, ImGui::GetStyle().FramePadding.y));
        ImGui::SetNextItemWidth(inputWidth);
        if (ImGui::InputTextWithHint("##SearchInput", InHint, InBuffer, InBufferSize))
            bChanged = true;
        ImGui::PopStyleVar();

        const ImVec2 fieldMin = ImGui::GetItemRectMin();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float iconSize = height - iconPad * 2.0f;
        FLucideIcons::DrawIcon(draw, ImVec2(fieldMin.x + iconPad, fieldMin.y + iconPad),
                               ImVec2(fieldMin.x + iconPad + iconSize, fieldMin.y + iconPad + iconSize),
                               ELucideIcon::Search, FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground));

        if (bHasText) {
            ImGui::SameLine(0.0f, 4.0f);
            if (DrawToolbarIconButton(ELucideIcon::X, "ClearSearch", true, height)) {
                InBuffer[0] = '\0';
                bChanged = true;
            }
        }

        PopControlColors(pushed);
        ImGui::PopID();
        return bChanged;
    }

    bool FEditorWidgets::DrawInputText(const char* InLabel, const char* InId, char* InBuffer, size_t InBufferSize,
                                       const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        bool changed = false;
        if (InLabel && InLabel[0] != '\0') {
            BeginPropertyGrid(ResolveLabelWidth(InStyle));
            BeginProperty(InLabel);
            ApplyItemWidth(pushed);
            changed = ImGui::InputText("##input", InBuffer, InBufferSize);
            EndProperty();
            EndPropertyGrid();
        } else {
            ApplyItemWidth(pushed);
            changed = ImGui::InputText("##input", InBuffer, InBufferSize);
        }
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawSelect(const char* InId, int* InOutIndex, const char* const* InItems, int InCount,
                                    const FControlStyle* InStyle) {
        if (!InOutIndex || !InItems || InCount <= 0)
            return false;

        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        ApplyItemWidth(pushed);

        const char* preview =
            (*InOutIndex >= 0 && *InOutIndex < InCount) ? InItems[*InOutIndex] : "";
        bool changed = false;
        const bool open = ImGui::BeginCombo("##select", preview, ImGuiComboFlags_None);
        const ImVec2 comboMin = ImGui::GetItemRectMin();
        const ImVec2 comboMax = ImGui::GetItemRectMax();
        if (open) {
            for (int i = 0; i < InCount; ++i) {
                const bool selected = (i == *InOutIndex);
                if (ImGui::Selectable(InItems[i], selected)) {
                    *InOutIndex = i;
                    changed = true;
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        {
            const float sz = ImGui::GetFontSize() * 0.75f;
            const float pad = 6.0f;
            const float x = comboMax.x - pad - sz;
            const float y = comboMin.y + (comboMax.y - comboMin.y - sz) * 0.5f;
            FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(x, y), ImVec2(x + sz, y + sz),
                                   ELucideIcon::ChevronDown,
                                   FEditorTheme::ToU32(FEditorTheme::GetTokens().MutedForeground));
        }

        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertySelect(const char* InLabel, const char* InId, int* InOutIndex,
                                            const char* const* InItems, int InCount, const FControlStyle* InStyle,
                                            const int* InDefaultIndex) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultIndex ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawSelect(InId, InOutIndex, InItems, InCount, style);
        if (InDefaultIndex && InOutIndex) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = (*InOutIndex != *InDefaultIndex);
            if (DrawResetToDefaultButton("##ResetSelect", canReset)) {
                *InOutIndex = *InDefaultIndex;
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    bool FEditorWidgets::DrawPropertyClassSelect(const char* InLabel, const char* InId, std::string& InOutClassName,
                                                 const std::vector<std::string>& InClassNames, const char* InNoneLabel,
                                                 const std::string* InDefaultClassName, const FControlStyle* InStyle) {
        BeginProperty(InLabel);

        std::vector<std::string> ownedLabels;
        ownedLabels.reserve(InClassNames.size() + 2);
        if (InNoneLabel)
            ownedLabels.emplace_back(InNoneLabel);

        bool bFoundCurrent = InOutClassName.empty();
        for (const std::string& name : InClassNames) {
            ownedLabels.push_back(name);
            if (name == InOutClassName)
                bFoundCurrent = true;
        }
        if (!InOutClassName.empty() && !bFoundCurrent)
            ownedLabels.insert(ownedLabels.begin() + (InNoneLabel ? 1 : 0), InOutClassName);

        std::vector<const char*> items;
        items.reserve(ownedLabels.size());
        for (const std::string& label : ownedLabels)
            items.push_back(label.c_str());

        int index = 0;
        if (InOutClassName.empty() && InNoneLabel) {
            index = 0;
        } else {
            for (size_t i = 0; i < ownedLabels.size(); ++i) {
                if (InNoneLabel && i == 0)
                    continue;
                if (ownedLabels[i] == InOutClassName) {
                    index = static_cast<int>(i);
                    break;
                }
            }
        }

        FControlStyle localStyle;
        const FControlStyle* style = InDefaultClassName ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawSelect(InId, &index, items.data(), static_cast<int>(items.size()), style);
        if (changed) {
            if (InNoneLabel && index == 0)
                InOutClassName.clear();
            else if (index >= 0 && index < static_cast<int>(ownedLabels.size()))
                InOutClassName = ownedLabels[static_cast<size_t>(index)];
        }

        if (InDefaultClassName) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = (InOutClassName != *InDefaultClassName);
            if (DrawResetToDefaultButton("##ResetClass", canReset)) {
                InOutClassName = *InDefaultClassName;
                changed = true;
            }
        }

        EndProperty();
        return changed;
    }

    int FEditorWidgets::DrawDropdown(const char* InId, const char* InPreviewLabel, const char* const* InItems,
                                     int InCount, int InCurrentIndex, const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        const FUiTokens& t = FEditorTheme::GetTokens();
        const float height = (InStyle && InStyle->Height > 0.0f) ? InStyle->Height : t.ControlHeight;

        int result = -1;
        const ImVec2 size = pushed.Width > 0.0f ? ImVec2(pushed.Width, height) : ImVec2(0.0f, height);
        if (ImGui::Button(InPreviewLabel ? InPreviewLabel : "##dd", size))
            ImGui::OpenPopup("##dropdown");
        DrawSelectChevron();

        if (ImGui::BeginPopup("##dropdown")) {
            for (int i = 0; i < InCount; ++i) {
                const bool selected = (i == InCurrentIndex);
                if (ImGui::MenuItem(InItems[i], nullptr, selected))
                    result = i;
            }
            ImGui::EndPopup();
        }

        PopControlColors(pushed);
        ImGui::PopID();
        return result;
    }

    bool FEditorWidgets::DrawSliderFloat(const char* InId, float* InOutValue, float InMin, float InMax,
                                         const char* InFormat, const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        ApplyItemWidth(pushed);
        const bool changed = ImGui::SliderFloat("##slider", InOutValue, InMin, InMax, InFormat);
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertySliderFloat(const char* InLabel, const char* InId, float* InOutValue, float InMin,
                                                 float InMax, const char* InFormat, const FControlStyle* InStyle,
                                                 const float* InDefaultValue) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultValue ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawSliderFloat(InId, InOutValue, InMin, InMax, InFormat, style);
        if (InDefaultValue && InOutValue) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = !NearlyEqualFloat(*InOutValue, *InDefaultValue);
            if (DrawResetToDefaultButton("##ResetSlider", canReset)) {
                *InOutValue = *InDefaultValue;
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    bool FEditorWidgets::DrawDragFloat(const char* InId, float* InOutValue, float InSpeed, float InMin, float InMax,
                                       const char* InFormat, const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        ApplyItemWidth(pushed);
        const bool changed = ImGui::DragFloat("##drag", InOutValue, InSpeed, InMin, InMax, InFormat);
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertyDragFloat(const char* InLabel, const char* InId, float* InOutValue, float InSpeed,
                                               float InMin, float InMax, const char* InFormat,
                                               const FControlStyle* InStyle, const float* InDefaultValue) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultValue ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawDragFloat(InId, InOutValue, InSpeed, InMin, InMax, InFormat, style);
        if (InDefaultValue && InOutValue) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = !NearlyEqualFloat(*InOutValue, *InDefaultValue);
            if (DrawResetToDefaultButton("##ResetDragF", canReset)) {
                *InOutValue = *InDefaultValue;
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    bool FEditorWidgets::DrawDragInt(const char* InId, int* InOutValue, float InSpeed, int InMin, int InMax,
                                     const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        ApplyItemWidth(pushed);
        const bool changed = ImGui::DragInt("##drag", InOutValue, InSpeed, InMin, InMax);
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertyDragInt(const char* InLabel, const char* InId, int* InOutValue, float InSpeed,
                                             int InMin, int InMax, const FControlStyle* InStyle,
                                             const int* InDefaultValue) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultValue ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawDragInt(InId, InOutValue, InSpeed, InMin, InMax, style);
        if (InDefaultValue && InOutValue) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = (*InOutValue != *InDefaultValue);
            if (DrawResetToDefaultButton("##ResetDragI", canReset)) {
                *InOutValue = *InDefaultValue;
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    bool FEditorWidgets::DrawCheckbox(const char* InId, bool* InOutValue, const char* InLabel,
                                      const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        const bool changed = InLabel && InLabel[0] != '\0' ? ImGui::Checkbox(InLabel, InOutValue)
                                                           : ImGui::Checkbox("##cb", InOutValue);
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertyCheckbox(const char* InLabel, const char* InId, bool* InOutValue,
                                              const FControlStyle* InStyle, const bool* InDefaultValue) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultValue ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawCheckbox(InId, InOutValue, nullptr, style);
        if (InDefaultValue && InOutValue) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = (*InOutValue != *InDefaultValue);
            if (DrawResetToDefaultButton("##ResetCb", canReset)) {
                *InOutValue = *InDefaultValue;
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    bool FEditorWidgets::DrawColorEdit3(const char* InId, float* InOutRgb, const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        ApplyItemWidth(pushed);
        const bool changed = ImGui::ColorEdit3("##color", InOutRgb, ImGuiColorEditFlags_Float);
        PopControlColors(pushed);
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawPropertyColorEdit3(const char* InLabel, const char* InId, float* InOutRgb,
                                                const FControlStyle* InStyle, const float* InDefaultRgb) {
        BeginProperty(InLabel);
        FControlStyle localStyle;
        const FControlStyle* style = InDefaultRgb ? StyleWithResetWidth(InStyle, localStyle) : InStyle;
        bool changed = DrawColorEdit3(InId, InOutRgb, style);
        if (InDefaultRgb && InOutRgb) {
            ImGui::SameLine(0.0f, 4.0f);
            const bool canReset = !NearlyEqualFloat(InOutRgb[0], InDefaultRgb[0]) ||
                                  !NearlyEqualFloat(InOutRgb[1], InDefaultRgb[1]) ||
                                  !NearlyEqualFloat(InOutRgb[2], InDefaultRgb[2]);
            if (DrawResetToDefaultButton("##ResetColor", canReset)) {
                InOutRgb[0] = InDefaultRgb[0];
                InOutRgb[1] = InDefaultRgb[1];
                InOutRgb[2] = InDefaultRgb[2];
                changed = true;
            }
        }
        EndProperty();
        return changed;
    }

    namespace {

        bool DrawIconLabelButton(ELucideIcon InIcon, const char* InLabel, const ImVec2& InSize, const ImVec4& InBg,
                                 const ImVec4& InBgHover, const ImVec4& InBgActive, const ImVec4& InFg) {
            const FUiTokens& t = FEditorTheme::GetTokens();
            const bool bHasLabel = InLabel && InLabel[0] != '\0';
            const float height = InSize.y > 0.0f ? InSize.y : t.ControlHeight;
            const float iconSize = height * 0.55f;
            const float padX = bHasLabel ? 8.0f : (height - iconSize) * 0.5f;
            const float gap = bHasLabel ? 6.0f : 0.0f;
            const ImVec2 textSize = bHasLabel ? ImGui::CalcTextSize(InLabel) : ImVec2(0, 0);
            float width = InSize.x;
            if (width < 0.0f)
                width = ImGui::GetContentRegionAvail().x;
            else if (width <= 0.0f)
                width = bHasLabel ? (padX + iconSize + gap + textSize.x + padX) : height;

            ImGui::PushStyleColor(ImGuiCol_Button, InBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, InBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, InBgActive);
            const bool clicked = ImGui::Button("##btn", ImVec2(width, height));
            ImGui::PopStyleColor(3);

            const ImVec2 min = ImGui::GetItemRectMin();
            ImDrawList* draw = ImGui::GetWindowDrawList();
            const ImU32 iconCol = FEditorTheme::ToU32(InFg);
            const float iconY = min.y + (height - iconSize) * 0.5f;
            const float iconX = min.x + padX;
            FLucideIcons::DrawIcon(draw, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize), InIcon,
                                   iconCol);
            if (bHasLabel) {
                const float textY = min.y + (height - textSize.y) * 0.5f;
                draw->AddText(ImVec2(iconX + iconSize + gap, textY), iconCol, InLabel);
            }
            return clicked;
        }

    } // namespace

    bool FEditorWidgets::DrawButton(ELucideIcon InIcon, const char* InId, const char* InLabel, const ImVec2& InSize,
                                    const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        const FUiTokens& t = FEditorTheme::GetTokens();
        const ImVec4 bg = InStyle && InStyle->bOverrideFrame ? InStyle->Frame : t.Primary;
        const ImVec4 accent = ResolveAccent(InStyle);
        const ImVec4 fg = ResolveForeground(InStyle);
        ImVec2 size = InSize;
        if (size.x <= 0.0f && pushed.Width > 0.0f)
            size.x = pushed.Width;
        const bool clicked =
            DrawIconLabelButton(InIcon, InLabel, size, bg, FEditorTheme::Lighten(bg, 0.08f), accent, fg);
        PopControlColors(pushed);
        ImGui::PopID();
        return clicked;
    }

    bool FEditorWidgets::DrawPrimaryButton(ELucideIcon InIcon, const char* InId, const char* InLabel,
                                           const ImVec2& InSize, const FControlStyle* InStyle) {
        FControlStyle style = InStyle ? *InStyle : FControlStyle{};
        if (!style.bOverrideAccent) {
            style.bOverrideAccent = true;
            style.Accent = FEditorTheme::GetTokens().Accent;
        }
        ImGui::PushID(InId);
        auto pushed = PushControlColors(&style);
        const ImVec4 accent = ResolveAccent(&style);
        const ImVec4 fg = FEditorTheme::GetTokens().AccentForeground;
        ImVec2 size = InSize;
        if (size.x <= 0.0f && pushed.Width > 0.0f)
            size.x = pushed.Width;
        const bool clicked = DrawIconLabelButton(InIcon, InLabel, size, accent, FEditorTheme::Lighten(accent, 0.08f),
                                                 FEditorTheme::Lighten(accent, -0.08f), fg);
        PopControlColors(pushed);
        ImGui::PopID();
        return clicked;
    }

    bool FEditorWidgets::DrawSuccessButton(ELucideIcon InIcon, const char* InId, const char* InLabel,
                                           const ImVec2& InSize, const FControlStyle* InStyle) {
        FControlStyle style = InStyle ? *InStyle : FControlStyle{};
        style.bOverrideAccent = true;
        style.Accent = FEditorTheme::GetTokens().Success;
        return DrawPrimaryButton(InIcon, InId, InLabel, InSize, &style);
    }

    bool FEditorWidgets::DrawToggleButton(ELucideIcon InIcon, const char* InId, bool bActive, const char* InLabel,
                                          const ImVec2& InSize, const FControlStyle* InStyle) {
        FControlStyle style = InStyle ? *InStyle : FControlStyle{};
        if (bActive) {
            style.bOverrideAccent = true;
            style.Accent = ResolveAccent(InStyle);
            return DrawPrimaryButton(InIcon, InId, InLabel, InSize, &style);
        }
        return DrawButton(InIcon, InId, InLabel, InSize, &style);
    }

    bool FEditorWidgets::DrawSegmentedControl(const char* InId, int* InOutIndex, const ELucideIcon* InIcons,
                                              const char* const* InLabels, int InCount, const FControlStyle* InStyle) {
        if (!InOutIndex || !InIcons || InCount <= 0)
            return false;

        ImGui::PushID(InId);
        bool changed = false;
        for (int i = 0; i < InCount; ++i) {
            if (i > 0)
                ImGui::SameLine(0.0f, 4.0f);
            const bool active = (*InOutIndex == i);
            const char* label = InLabels ? InLabels[i] : nullptr;
            char itemId[32];
            std::snprintf(itemId, sizeof(itemId), "seg%d", i);
            if (DrawToggleButton(InIcons[i], itemId, active, label, ImVec2(0, 0), InStyle)) {
                if (*InOutIndex != i) {
                    *InOutIndex = i;
                    changed = true;
                }
            }
        }
        ImGui::PopID();
        return changed;
    }

    bool FEditorWidgets::DrawToolbarButton(ELucideIcon InIcon, const char* InLabel, const char* InId, float InHeight,
                                           const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        auto pushed = PushControlColors(InStyle);
        const FUiTokens& t = FEditorTheme::GetTokens();
        const ImVec4 btn = InStyle && InStyle->bOverrideFrame ? InStyle->Frame : t.Primary;
        const ImVec4 accent = ResolveAccent(InStyle);
        const ImVec4 fg = ResolveForeground(InStyle);

        const float iconSize = InHeight * 0.55f;
        const float padX = 8.0f;
        const float gap = 6.0f;
        const ImVec2 textSize = ImGui::CalcTextSize(InLabel);
        const float width = padX + iconSize + gap + textSize.x + padX;

        ImGui::PushStyleColor(ImGuiCol_Button, btn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FEditorTheme::Lighten(btn, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
        const bool clicked = ImGui::Button("##tb", ImVec2(width, InHeight));
        ImGui::PopStyleColor(3);

        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float iconY = min.y + (InHeight - iconSize) * 0.5f;
        const float iconX = min.x + padX;
        const ImU32 iconCol = FEditorTheme::ToU32(fg);
        FLucideIcons::DrawIcon(draw, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize), InIcon,
                               iconCol);
        const float textY = min.y + (InHeight - textSize.y) * 0.5f;
        draw->AddText(ImVec2(iconX + iconSize + gap, textY), iconCol, InLabel);

        PopControlColors(pushed);
        ImGui::PopID();
        return clicked;
    }

    bool FEditorWidgets::DrawToolbarIconButton(ELucideIcon InIcon, const char* InId, bool bEnabled, float InSize,
                                               const FControlStyle* InStyle) {
        ImGui::PushID(InId);
        if (!bEnabled)
            ImGui::BeginDisabled();

        auto pushed = PushControlColors(InStyle);
        const FUiTokens& t = FEditorTheme::GetTokens();
        const ImVec4 btn = InStyle && InStyle->bOverrideFrame ? InStyle->Frame : t.Primary;
        const ImVec4 accent = ResolveAccent(InStyle);
        const ImVec4 fg = ResolveForeground(InStyle);

        ImGui::PushStyleColor(ImGuiCol_Button, btn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, FEditorTheme::Lighten(btn, 0.08f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
        const bool clicked = ImGui::Button("##icon", ImVec2(InSize, InSize));
        ImGui::PopStyleColor(3);

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float pad = InSize * 0.22f;
        FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(min.x + pad, min.y + pad),
                               ImVec2(max.x - pad, max.y - pad), InIcon, FEditorTheme::ToU32(fg));

        PopControlColors(pushed);
        if (!bEnabled)
            ImGui::EndDisabled();
        ImGui::PopID();
        return clicked && bEnabled;
    }

} // namespace Leon::Editor
