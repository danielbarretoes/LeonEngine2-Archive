#include "Editor/Panels/FDetailsPanel.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/Components.hpp"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstring>

namespace Leon::Editor {

    namespace {
        void DrawVec3Control(const char* label, glm::vec3& values, float resetValue = 0.0f) {
            ImGui::PushID(label);
            ImGui::Text("%s", label);

            float fullWidth = ImGui::GetContentRegionAvail().x;
            float itemWidth = (fullWidth - 80.0f) / 3.0f;

            // X
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
            if (ImGui::Button("X"))
                values.x = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::DragFloat("##X", &values.x, 0.1f);
            ImGui::SameLine();

            // Y
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
            if (ImGui::Button("Y"))
                values.y = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::DragFloat("##Y", &values.y, 0.1f);
            ImGui::SameLine();

            // Z
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
            if (ImGui::Button("Z"))
                values.z = resetValue;
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            ImGui::DragFloat("##Z", &values.z, 0.1f);

            ImGui::PopID();
        }
    } // namespace

    void FDetailsPanel::Draw(AActor* InSelectedActor) {
        ImGui::Begin("Details");

        if (!InSelectedActor) {
            ImGui::TextDisabled("Select an actor in the Outliner or Viewport to view details.");
            ImGui::End();
            return;
        }

        // Actor Header
        char nameBuf[128];
#ifdef _WIN32
        strncpy_s(nameBuf, sizeof(nameBuf), InSelectedActor->GetName().c_str(), _TRUNCATE);
#else
        std::strncpy(nameBuf, InSelectedActor->GetName().c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
#endif

        ImGui::Text("Actor Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##ActorName", nameBuf, sizeof(nameBuf))) {
            InSelectedActor->SetName(nameBuf);
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Components List
        DrawTransformComponent(*InSelectedActor);
        DrawStaticMeshComponent(*InSelectedActor);
        DrawLightComponents(*InSelectedActor);
        DrawCameraComponent(*InSelectedActor);

        ImGui::Spacing();
        ImGui::Separator();
        DrawAddComponentMenu(*InSelectedActor);

        ImGui::End();
    }

    void FDetailsPanel::DrawTransformComponent(AActor& InActor) {
        if (!InActor.HasComponent<FTransformComponent>())
            return;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& tc = InActor.GetComponent<FTransformComponent>();

            DrawVec3Control("Location", tc.Translation, 0.0f);
            DrawVec3Control("Rotation", tc.Rotation, 0.0f);
            DrawVec3Control("Scale", tc.Scale, 1.0f);
        }
    }

    void FDetailsPanel::DrawStaticMeshComponent(AActor& InActor) {
        if (!InActor.HasComponent<FStaticMeshComponent>())
            return;

        if (ImGui::CollapsingHeader("Static Mesh Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& smc = InActor.GetComponent<FStaticMeshComponent>();

            char pathBuf[256] = "";
            if (!smc.AssetPath.empty()) {
#ifdef _WIN32
                strncpy_s(pathBuf, sizeof(pathBuf), smc.AssetPath.c_str(), _TRUNCATE);
#else
                std::strncpy(pathBuf, smc.AssetPath.c_str(), sizeof(pathBuf) - 1);
                pathBuf[sizeof(pathBuf) - 1] = '\0';
#endif
            }
            ImGui::Text("Mesh Asset Path:");
            if (ImGui::InputText("##AssetPath", pathBuf, sizeof(pathBuf))) {
                smc.AssetPath = pathBuf;
            }

            ImGui::Checkbox("Cast Shadows", &smc.bCastShadows);
            ImGui::Checkbox("Receive Shadows", &smc.bReceiveShadows);
            ImGui::Checkbox("Visible in Reflection", &smc.bVisibleInReflection);
        }
    }

    void FDetailsPanel::DrawLightComponents(AActor& InActor) {
        if (InActor.HasComponent<FDirectionalLightComponent>()) {
            if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& dlc = InActor.GetComponent<FDirectionalLightComponent>();
                ImGui::Checkbox("Light Enabled", &dlc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(dlc.Light.Color));
                ImGui::DragFloat("Intensity", &dlc.Light.Intensity, 0.1f, 0.0f, 100.0f);
            }
        }

        if (InActor.HasComponent<FPointLightComponent>()) {
            if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& plc = InActor.GetComponent<FPointLightComponent>();
                ImGui::Checkbox("Light Enabled", &plc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(plc.Light.Color));
                ImGui::DragFloat("Intensity", &plc.Light.Intensity, 0.1f, 0.0f, 500.0f);
                ImGui::DragFloat("Attenuation Radius", &plc.Light.Radius, 0.5f, 0.1f, 1000.0f);
            }
        }

        if (InActor.HasComponent<FSpotLightComponent>()) {
            if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& slc = InActor.GetComponent<FSpotLightComponent>();
                ImGui::Checkbox("Light Enabled", &slc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(slc.Light.Color));
                ImGui::DragFloat("Intensity", &slc.Light.Intensity, 0.1f, 0.0f, 500.0f);
                ImGui::DragFloat("Attenuation Radius", &slc.Light.Radius, 0.5f, 0.1f, 1000.0f);
                ImGui::DragFloat("Inner Cone Angle", &slc.Light.CutOff, 0.5f, 0.0f, 89.0f);
                ImGui::DragFloat("Outer Cone Angle", &slc.Light.OuterCutOff, 0.5f, 0.0f, 90.0f);
            }
        }
    }

    void FDetailsPanel::DrawCameraComponent(AActor& InActor) {
        if (!InActor.HasComponent<FCameraComponent>())
            return;

        if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cc = InActor.GetComponent<FCameraComponent>();
            ImGui::Checkbox("Primary Camera", &cc.bPrimary);
        }
    }

    void FDetailsPanel::DrawAddComponentMenu(AActor& InActor) {
        if (ImGui::Button("+ Add Component", ImVec2(-1.0f, 30.0f))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (!InActor.HasComponent<FStaticMeshComponent>() && ImGui::MenuItem("Static Mesh Component")) {
                InActor.AddComponent<FStaticMeshComponent>();
            }
            if (!InActor.HasComponent<FDirectionalLightComponent>() && ImGui::MenuItem("Directional Light Component")) {
                InActor.AddComponent<FDirectionalLightComponent>();
            }
            if (!InActor.HasComponent<FPointLightComponent>() && ImGui::MenuItem("Point Light Component")) {
                InActor.AddComponent<FPointLightComponent>();
            }
            if (!InActor.HasComponent<FSpotLightComponent>() && ImGui::MenuItem("Spot Light Component")) {
                InActor.AddComponent<FSpotLightComponent>();
            }
            if (!InActor.HasComponent<FCameraComponent>() && ImGui::MenuItem("Camera Component")) {
                InActor.AddComponent<FCameraComponent>();
            }
            ImGui::EndPopup();
        }
    }

} // namespace Leon::Editor
