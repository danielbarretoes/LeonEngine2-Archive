#include "Editor/Panels/FToolbarPanel.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FToolbarPanel::Draw(const std::string& InProjectName, const std::string& InMapName) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("##EditorToolbar", nullptr, flags)) {
            // Project Hub button
            if (ImGui::Button("Project Browser", ImVec2(120.0f, 26.0f))) {
                if (OnOpenHub)
                    OnOpenHub();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Save Map
            if (ImGui::Button("Save Map", ImVec2(90.0f, 26.0f))) {
                if (OnSaveMap)
                    OnSaveMap();
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Bake Lighting dropdown / buttons
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

            // Run / PIE
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
            if (ImGui::Button("Play Game", ImVec2(90.0f, 26.0f))) {
                if (OnRunGame)
                    OnRunGame();
            }
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Reset Layout button
            if (ImGui::Button("Reset Layout", ImVec2(100.0f, 26.0f))) {
                if (OnResetLayout)
                    OnResetLayout();
            }

            // Right side: Active Project and Map badges
            float rightOffset = 380.0f;
            if (ImGui::GetWindowWidth() > rightOffset + 100.0f) {
                ImGui::SameLine(ImGui::GetWindowWidth() - rightOffset);
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
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

} // namespace Leon::Editor
