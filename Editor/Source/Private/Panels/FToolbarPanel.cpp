#include "Editor/Panels/FToolbarPanel.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FToolbarPanel::Draw(const std::string& InProjectName, const std::string& InMapName,
                             const std::string& InStatusMessage) {
        // Contents only — host (dockspace) owns the fixed strip; do not create a dockable window.
        if (ImGui::Button("Project Browser", ImVec2(120.0f, 26.0f))) {
            if (OnOpenHub)
                OnOpenHub();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::Button("Save Map", ImVec2(90.0f, 26.0f))) {
            if (OnSaveMap)
                OnSaveMap();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::Button("Bake (Draft)", ImVec2(100.0f, 26.0f))) {
            if (OnBakeDraft)
                OnBakeDraft();
        }
        ImGui::SameLine();
        if (ImGui::Button("Bake (Production)", ImVec2(130.0f, 26.0f))) {
            if (OnBakeProduction)
                OnBakeProduction();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
        if (ImGui::Button("Play Game", ImVec2(90.0f, 26.0f))) {
            if (OnRunGame)
                OnRunGame();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (ImGui::Button("Reset Layout", ImVec2(100.0f, 26.0f))) {
            if (OnResetLayout)
                OnResetLayout();
        }

        if (!InStatusMessage.empty() && InStatusMessage != "Ready") {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", InStatusMessage.c_str());
        }

        float rightOffset = 380.0f;
        if (ImGui::GetContentRegionAvail().x > rightOffset + 100.0f ||
            ImGui::GetWindowWidth() > rightOffset + 100.0f) {
            const float lineStart = ImGui::GetCursorPosX();
            const float targetX = ImGui::GetWindowContentRegionMax().x - rightOffset;
            if (targetX > lineStart)
                ImGui::SameLine(targetX);
            else
                ImGui::SameLine();

            ImGui::TextDisabled("Project:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s",
                               InProjectName.empty() ? "None" : InProjectName.c_str());

            ImGui::SameLine();
            ImGui::TextDisabled("Map:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%s",
                               InMapName.empty() ? "Untitled" : InMapName.c_str());
        }
    }

} // namespace Leon::Editor
