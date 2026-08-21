#include "Editor/Panels/FToolbarPanel.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FToolbarPanel::Draw(const std::string& InProjectName, const std::string& InMapName,
                             const std::string& InStatusMessage) {
        // Contents only — host (dockspace) owns the fixed strip; do not create a dockable window.
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Folder, "Project Browser", "TB_Hub")) {
            if (OnOpenHub)
                OnOpenHub();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Save, "Save Map", "TB_Save")) {
            if (OnSaveMap)
                OnSaveMap();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Sun, "Bake Draft", "TB_BakeDraft")) {
            if (OnBakeDraft)
                OnBakeDraft();
        }
        ImGui::SameLine();
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Flame, "Bake Production", "TB_BakeProd")) {
            if (OnBakeProduction)
                OnBakeProduction();
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        {
            FControlStyle playStyle;
            playStyle.bOverrideAccent = true;
            playStyle.Accent = FEditorTheme::GetTokens().Success;
            playStyle.bOverrideFrame = true;
            playStyle.Frame = FEditorTheme::GetTokens().Success;
            if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Play, "Play Game", "TB_Play", 26.0f, &playStyle)) {
                if (OnRunGame)
                    OnRunGame();
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::LayoutGrid, "Reset Layout", "TB_Reset")) {
            if (OnResetLayout)
                OnResetLayout();
        }

        if (!InStatusMessage.empty() && InStatusMessage != "Ready") {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::TextColored(FEditorTheme::GetTokens().Warning, "%s", InStatusMessage.c_str());
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

            const FUiTokens& t = FEditorTheme::GetTokens();
            ImGui::TextDisabled("Project:");
            ImGui::SameLine();
            ImGui::TextColored(t.Accent, "%s", InProjectName.empty() ? "None" : InProjectName.c_str());

            ImGui::SameLine();
            ImGui::TextDisabled("Map:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%s",
                               InMapName.empty() ? "Untitled" : InMapName.c_str());
        }
    }

} // namespace Leon::Editor
