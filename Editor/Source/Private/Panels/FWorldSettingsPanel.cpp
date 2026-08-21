#include "Editor/Panels/FWorldSettingsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FWorldSettingsPanel::Draw(UWorld* InWorld, bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::WorldSettings, bInOutOpen, ELucideIcon::Globe);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            if (ImGui::CollapsingHeader("GameMode", ImGuiTreeNodeFlags_DefaultOpen)) {
                FEditorWidgets::DrawInputText("GameMode Override", "##GameModeOverride", GameModeOverride,
                                              sizeof(GameModeOverride));
            }

            if (ImGui::CollapsingHeader("Lightmass / Static Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                FEditorWidgets::BeginPropertyGrid();
                FEditorWidgets::DrawPropertyCheckbox("Enable Static Lighting", "##EnableStaticLighting",
                                                     &bEnableStaticLighting);
                FEditorWidgets::DrawPropertyDragInt("Default Lightmap Resolution", "##LightmapRes", &LightmapResolution,
                                                    16, 32, 4096);
                FEditorWidgets::EndPropertyGrid();
            }

            if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                FEditorWidgets::BeginPropertyGrid();
                FEditorWidgets::DrawPropertyDragFloat("Global Gravity Z", "##GravityZ", &Gravity, 0.1f, -50.0f, 50.0f);
                FEditorWidgets::EndPropertyGrid();
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FWorldSettingsPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FWorldSettingsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings: Unknown error encountered");
        }

        ImGui::End();
    }

} // namespace Leon::Editor
