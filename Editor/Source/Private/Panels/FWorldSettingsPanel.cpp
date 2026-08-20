#include "Editor/Panels/FWorldSettingsPanel.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FWorldSettingsPanel::Draw(UWorld* InWorld) {
        ImGui::Begin("World Settings");

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

        ImGui::End();
    }

} // namespace Leon::Editor
