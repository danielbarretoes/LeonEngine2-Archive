#include "Editor/Panels/FProjectSettingsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <cstring>
#include <imgui.h>

namespace Leon::Editor {

    void FProjectSettingsPanel::Draw(FProjectDescriptor& InOutDescriptor, const std::string& InProjectPath,
                                     bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::ProjectSettings, bInOutOpen, ELucideIcon::Settings);

        try {
            char nameBuf[128];
            char mapBuf[256];
            char gmBuf[128];

#ifdef _WIN32
            strncpy_s(nameBuf, sizeof(nameBuf), InOutDescriptor.ProjectName.c_str(), _TRUNCATE);
            strncpy_s(mapBuf, sizeof(mapBuf), InOutDescriptor.DefaultMap.c_str(), _TRUNCATE);
            strncpy_s(gmBuf, sizeof(gmBuf), InOutDescriptor.DefaultGameMode.c_str(), _TRUNCATE);
#else
            std::strncpy(nameBuf, InOutDescriptor.ProjectName.c_str(), sizeof(nameBuf) - 1);
            std::strncpy(mapBuf, InOutDescriptor.DefaultMap.c_str(), sizeof(mapBuf) - 1);
            std::strncpy(gmBuf, InOutDescriptor.DefaultGameMode.c_str(), sizeof(gmBuf) - 1);
#endif

            ImGui::TextColored(FEditorTheme::GetTokens().Accent, "General Project Settings");
            ImGui::Separator();
            ImGui::Spacing();

            if (FEditorWidgets::DrawInputText("Project Name", "##ProjectName", nameBuf, sizeof(nameBuf))) {
                InOutDescriptor.ProjectName = nameBuf;
            }

            ImGui::TextDisabled("Engine Version: %s", InOutDescriptor.EngineVersion.c_str());

            ImGui::Spacing();
            ImGui::TextColored(FEditorTheme::GetTokens().Accent, "Default Maps & Modes");
            ImGui::Separator();
            ImGui::Spacing();

            if (FEditorWidgets::DrawInputText("Editor Startup Map", "##DefaultMap", mapBuf, sizeof(mapBuf))) {
                InOutDescriptor.DefaultMap = mapBuf;
            }

            if (FEditorWidgets::DrawInputText("Default GameMode", "##DefaultGM", gmBuf, sizeof(gmBuf))) {
                InOutDescriptor.DefaultGameMode = gmBuf;
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (FEditorWidgets::DrawPrimaryButton(ELucideIcon::Save, "##SaveSettings", "Save Settings",
                                                  ImVec2(160.0f, 32.0f))) {
                if (!InProjectPath.empty()) {
                    InOutDescriptor.Save(InProjectPath);
                }
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FProjectSettingsPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Project Settings Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FProjectSettingsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Project Settings: Unknown error encountered");
        }

        ImGui::End();
    }

} // namespace Leon::Editor
