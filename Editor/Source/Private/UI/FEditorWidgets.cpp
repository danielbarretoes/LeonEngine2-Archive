#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

namespace Leon::Editor {

    namespace {

        void DrawPanelTitleIcon(ELucideIcon InIcon) {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (!window)
                return;

            const float iconSz = ImGui::GetFontSize() * 0.92f;
            const ImU32 col = IM_COL32(175, 180, 190, 255);
            ImDrawList* draw = ImGui::GetForegroundDrawList();

            if (window->DockNode && window->DockNode->TabBar) {
                ImGuiTabBar* tabBar = window->DockNode->TabBar;
                for (int n = 0; n < tabBar->Tabs.Size; ++n) {
                    const ImGuiTabItem& tab = tabBar->Tabs[n];
                    if (tab.Window != window)
                        continue;

                    const float x = tabBar->BarRect.Min.x + tab.Offset + 5.0f - tabBar->ScrollingAnim;
                    const float y = tabBar->BarRect.Min.y + (tabBar->BarRect.GetHeight() - iconSz) * 0.5f;
                    FLucideIcons::DrawIcon(draw, ImVec2(x, y), ImVec2(x + iconSz, y + iconSz), InIcon, col);
                    return;
                }
            }

            if (!(window->Flags & ImGuiWindowFlags_NoTitleBar)) {
                const ImRect title = window->TitleBarRect();
                const float padX = ImGui::GetStyle().FramePadding.x + 2.0f;
                const float x = title.Min.x + padX;
                const float y = title.Min.y + (title.GetHeight() - iconSz) * 0.5f;
                FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(x, y), ImVec2(x + iconSz, y + iconSz),
                                       InIcon, col);
            }
        }

    } // namespace

    bool FEditorWidgets::BeginPanelWindow(const char* InTitle, bool* bInOutOpen, ELucideIcon InIcon,
                                          ImGuiWindowFlags InFlags) {
        const bool open = ImGui::Begin(InTitle, bInOutOpen, InFlags);
        // Always decorate: docked tabs stay visible even when Begin returns false (collapsed/hidden host).
        DrawPanelTitleIcon(InIcon);
        return open;
    }

    bool FEditorWidgets::DrawVec3Control(const std::string& InLabel, glm::vec3& InValues, float InResetValue,
                                         float InColumnWidth) {
        bool bChanged = false;
        ImGui::PushID(InLabel.c_str());

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, InColumnWidth);
        ImGui::TextUnformatted(InLabel.c_str());
        ImGui::NextColumn();

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
                               IM_COL32(180, 185, 195, 255));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset all components to default");
        }

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
        if (ImGui::DragFloat("##X", &InValues.x, 0.1f, 0.0f, 0.0f, "%.2f")) {
            bChanged = true;
        }
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
        if (ImGui::DragFloat("##Y", &InValues.y, 0.1f, 0.0f, 0.0f, "%.2f")) {
            bChanged = true;
        }
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
        if (ImGui::DragFloat("##Z", &InValues.z, 0.1f, 0.0f, 0.0f, "%.2f")) {
            bChanged = true;
        }

        ImGui::Columns(1);
        ImGui::PopID();
        return bChanged;
    }

    bool FEditorWidgets::DrawSearchInput(const char* InId, char* InBuffer, size_t InBufferSize, const char* InHint) {
        bool bChanged = false;
        ImGui::PushID(InId);

        const float height = ImGui::GetFrameHeight();
        const float iconPad = 6.0f;
        const float leftPad = height; // room for search icon
        const bool bHasText = (InBuffer && InBuffer[0] != '\0');
        const float clearW = bHasText ? (height + 4.0f) : 0.0f;
        const float availWidth = ImGui::GetContentRegionAvail().x;
        const float inputWidth = std::max(40.0f, availWidth - clearW);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                            ImVec2(leftPad, ImGui::GetStyle().FramePadding.y));
        ImGui::SetNextItemWidth(inputWidth);
        if (ImGui::InputTextWithHint("##SearchInput", InHint, InBuffer, InBufferSize)) {
            bChanged = true;
        }
        ImGui::PopStyleVar();

        const ImVec2 fieldMin = ImGui::GetItemRectMin();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const float iconSize = height - iconPad * 2.0f;
        FLucideIcons::DrawIcon(draw, ImVec2(fieldMin.x + iconPad, fieldMin.y + iconPad),
                               ImVec2(fieldMin.x + iconPad + iconSize, fieldMin.y + iconPad + iconSize),
                               ELucideIcon::Search, IM_COL32(150, 155, 165, 255));

        if (bHasText) {
            ImGui::SameLine(0.0f, 4.0f);
            if (ImGui::Button("x##ClearSearch", ImVec2(height, height))) {
                InBuffer[0] = '\0';
                bChanged = true;
            }
        }

        ImGui::PopID();
        return bChanged;
    }

    bool FEditorWidgets::DrawToolbarButton(ELucideIcon InIcon, const char* InLabel, const char* InId,
                                           float InHeight) {
        ImGui::PushID(InId);

        const float iconSize = InHeight * 0.55f;
        const float padX = 8.0f;
        const float gap = 6.0f;
        const ImVec2 textSize = ImGui::CalcTextSize(InLabel);
        const float width = padX + iconSize + gap + textSize.x + padX;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.28f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.40f, 0.75f, 1.0f));
        const bool clicked = ImGui::Button("##tb", ImVec2(width, InHeight));
        ImGui::PopStyleColor(3);

        const ImVec2 min = ImGui::GetItemRectMin();
        ImDrawList* draw = ImGui::GetWindowDrawList();

        const float iconY = min.y + (InHeight - iconSize) * 0.5f;
        const float iconX = min.x + padX;
        FLucideIcons::DrawIcon(draw, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize), InIcon,
                               IM_COL32(220, 225, 235, 255));

        const float textY = min.y + (InHeight - textSize.y) * 0.5f;
        draw->AddText(ImVec2(iconX + iconSize + gap, textY), IM_COL32(220, 225, 235, 255), InLabel);

        ImGui::PopID();
        return clicked;
    }

    bool FEditorWidgets::DrawToolbarIconButton(ELucideIcon InIcon, const char* InId, bool bEnabled, float InSize) {
        ImGui::PushID(InId);

        if (!bEnabled)
            ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.19f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.28f, 0.32f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.40f, 0.75f, 1.0f));
        const bool clicked = ImGui::Button("##icon", ImVec2(InSize, InSize));
        ImGui::PopStyleColor(3);

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float pad = InSize * 0.22f;
        FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(min.x + pad, min.y + pad),
                               ImVec2(max.x - pad, max.y - pad), InIcon, IM_COL32(220, 225, 235, 255));

        if (!bEnabled)
            ImGui::EndDisabled();

        ImGui::PopID();
        return clicked && bEnabled;
    }

} // namespace Leon::Editor
