#include "Editor/Panels/FWorldSettingsPanel.hpp"
#include "Core/FLog.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FWorldSettingsPanel::Draw(UWorld* InWorld, bool* bInOutOpen) {
        ImGui::Begin("World Settings", bInOutOpen);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            if (ImGui::CollapsingHeader("GameMode", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("GameMode Override:");
                ImGui::InputText("##GameModeOverride", GameModeOverride, sizeof(GameModeOverride));
            }

            if (ImGui::CollapsingHeader("Lightmass / Static Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Enable Static Lighting", &bEnableStaticLighting);
                ImGui::DragInt("Default Lightmap Resolution", &LightmapResolution, 16, 32, 4096);
            }

            if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat("Global Gravity Z", &Gravity, 0.1f, -50.0f, 50.0f);
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FWorldSettingsPanel: Exception during Draw: {0}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FWorldSettingsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "World Settings: Unknown error encountered");
        }

        ImGui::End();
    }

} // namespace Leon::Editor
