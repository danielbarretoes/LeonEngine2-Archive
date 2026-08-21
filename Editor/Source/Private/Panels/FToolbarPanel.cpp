#include "Editor/Panels/FToolbarPanel.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <cstdio>
#include <imgui.h>

namespace Leon::Editor {

    namespace {
        constexpr float kToolbarBtnH = 26.0f;
        constexpr float kGroupGap = 10.0f;
        constexpr float kItemGap = 4.0f;

        void ToolbarSameLine(float gap = kItemGap) { ImGui::SameLine(0.0f, gap); }

        void DrawToolbarDivider() {
            ToolbarSameLine(kGroupGap);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float h = kToolbarBtnH * 0.55f;
            const float y0 = pos.y + (kToolbarBtnH - h) * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(ImVec2(pos.x, y0), ImVec2(pos.x, y0 + h),
                                                ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.0f);
            ImGui::Dummy(ImVec2(1.0f, kToolbarBtnH));
            ToolbarSameLine(kGroupGap);
        }
    } // namespace

    void FToolbarPanel::DrawPlaySettingsPopup() {
        if (!PlaySettings)
            return;

        ImGui::TextUnformatted("Play Settings");
        ImGui::Separator();

        int Players = PlaySettings->NumberOfPlayers;
        if (ImGui::SliderInt("Number of Players", &Players, 1, 4))
            PlaySettings->NumberOfPlayers = Players;

        const char* NetModes[] = {"Standalone", "Listen Server", "Client", "Dedicated Server"};
        int NetIdx = static_cast<int>(PlaySettings->NetMode);
        if (ImGui::Combo("Net Mode", &NetIdx, NetModes, 4))
            PlaySettings->NetMode = static_cast<EPlayNetMode>(NetIdx);

        const char* PlayModes[] = {"Selected Viewport", "New Editor Window"};
        int PlayIdx = static_cast<int>(PlaySettings->PlayMode);
        if (ImGui::Combo("Play Mode", &PlayIdx, PlayModes, 2))
            PlaySettings->PlayMode = static_cast<EPlayMode>(PlayIdx);

        ImGui::InputInt("Listen Port", &PlaySettings->ListenPort);
        char Addr[128] = {};
        std::snprintf(Addr, sizeof(Addr), "%s", PlaySettings->ClientAddress.c_str());
        if (ImGui::InputText("Client Address", Addr, sizeof(Addr)))
            PlaySettings->ClientAddress = Addr;

        ImGui::Checkbox("Auto-save map before Play", &PlaySettings->bAutoSaveMapBeforePlay);
        PlaySettings->Clamp();

        if (PlaySettings->NumberOfPlayers > 1 || PlaySettings->NetMode != EPlayNetMode::Standalone) {
            ImGui::TextDisabled("Multi-instance: host in viewport; clients spawn as child processes.");
        }
    }

    void FToolbarPanel::Draw(const std::string& InProjectName, const std::string& InMapName,
                             const std::string& InStatusMessage) {
        ImGui::AlignTextToFramePadding();

        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::Save, "TB_Save", true, kToolbarBtnH)) {
            if (OnSaveMap)
                OnSaveMap();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Save Map");

        DrawToolbarDivider();

        if (!bPlaying) {
            FControlStyle playStyle;
            playStyle.bOverrideAccent = true;
            playStyle.Accent = FEditorTheme::GetTokens().Success;
            playStyle.bOverrideFrame = true;
            playStyle.Frame = FEditorTheme::GetTokens().Success;
            if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Play, "Play", "TB_Play", kToolbarBtnH, &playStyle)) {
                if (OnPlay)
                    OnPlay();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("Play In Editor");
        } else {
            FControlStyle stopStyle;
            stopStyle.bOverrideAccent = true;
            stopStyle.Accent = FEditorTheme::GetTokens().Destructive;
            stopStyle.bOverrideFrame = true;
            stopStyle.Frame = FEditorTheme::GetTokens().Destructive;
            if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Square, "Stop", "TB_Stop", kToolbarBtnH, &stopStyle)) {
                if (OnStop)
                    OnStop();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("Stop Play In Editor (Esc)");
        }

        ToolbarSameLine();
        if (FEditorWidgets::DrawToolbarIconButton(ELucideIcon::Settings, "TB_PlaySettings", !bPlaying, kToolbarBtnH)) {
            ImGui::OpenPopup("##PlaySettingsPopup");
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            ImGui::SetTooltip("Play Settings");
        if (ImGui::BeginPopup("##PlaySettingsPopup")) {
            DrawPlaySettingsPopup();
            ImGui::EndPopup();
        }

        DrawToolbarDivider();

        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Sun, "Bake Draft", "TB_BakeDraft", kToolbarBtnH)) {
            if (OnBakeDraft)
                OnBakeDraft();
        }
        ToolbarSameLine();
        if (FEditorWidgets::DrawToolbarButton(ELucideIcon::Flame, "Bake Production", "TB_BakeProd", kToolbarBtnH)) {
            if (OnBakeProduction)
                OnBakeProduction();
        }

        if (!InStatusMessage.empty() && InStatusMessage != "Ready") {
            DrawToolbarDivider();
            ImGui::AlignTextToFramePadding();
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
                ToolbarSameLine(kGroupGap);

            ImGui::AlignTextToFramePadding();
            const FUiTokens& t = FEditorTheme::GetTokens();
            ImGui::TextDisabled("Project:");
            ToolbarSameLine(6.0f);
            ImGui::TextColored(t.Accent, "%s", InProjectName.empty() ? "None" : InProjectName.c_str());

            ToolbarSameLine(14.0f);
            ImGui::TextDisabled("Map:");
            ToolbarSameLine(6.0f);
            ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%s",
                               InMapName.empty() ? "Untitled" : InMapName.c_str());
        }
    }

} // namespace Leon::Editor
