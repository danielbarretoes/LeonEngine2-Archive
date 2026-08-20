#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"

#include <imgui.h>

namespace Leon::Editor {

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

        // Reset all button
        if (ImGui::Button("##ResetAll", buttonSize)) {
            InValues = glm::vec3(InResetValue);
            bChanged = true;
        }
        ImVec2 resetBtnMin = ImGui::GetItemRectMin();
        ImVec2 resetBtnMax = ImGui::GetItemRectMax();
        FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(resetBtnMin.x + 2.0f, resetBtnMin.y + 2.0f),
                               ImVec2(resetBtnMax.x - 2.0f, resetBtnMax.y - 2.0f),
                               ELucideIcon::RefreshCw, IM_COL32(180, 185, 195, 255));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Reset all components to default");
        }

        ImGui::SameLine();

        // X
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

        // Y
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

        // Z
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

        float availWidth = ImGui::GetContentRegionAvail().x;
        bool bHasText = (InBuffer && InBuffer[0] != '\0');

        if (bHasText) {
            float btnWidth = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(availWidth - btnWidth - 4.0f);
        } else {
            ImGui::SetNextItemWidth(availWidth);
        }

        if (ImGui::InputTextWithHint("##SearchInput", InHint, InBuffer, InBufferSize)) {
            bChanged = true;
        }

        if (bHasText) {
            ImGui::SameLine();
            if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
                InBuffer[0] = '\0';
                bChanged = true;
            }
        }

        ImGui::PopID();
        return bChanged;
    }

} // namespace Leon::Editor
