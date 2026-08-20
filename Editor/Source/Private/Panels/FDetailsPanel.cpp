#include "Editor/Panels/FDetailsPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>

namespace Leon::Editor {

    namespace {
        bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f,
                             float columnWidth = 100.0f) {
            bool bModified = false;
            ImGui::PushID(label.c_str());

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, columnWidth);
            ImGui::Text("%s", label.c_str());
            ImGui::NextColumn();

            ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));

            float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
            ImVec2 buttonSize = {lineHeight + 3.0f, lineHeight};

            // X
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
            if (ImGui::Button("X", buttonSize)) {
                values.x = resetValue;
                bModified = true;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
                bModified = true;
            ImGui::PopItemWidth();
            ImGui::SameLine();

            // Y
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
            if (ImGui::Button("Y", buttonSize)) {
                values.y = resetValue;
                bModified = true;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
                bModified = true;
            ImGui::PopItemWidth();
            ImGui::SameLine();

            // Z
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
            if (ImGui::Button("Z", buttonSize)) {
                values.z = resetValue;
                bModified = true;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
                bModified = true;
            ImGui::PopItemWidth();

            ImGui::PopStyleVar();
            ImGui::Columns(1);
            ImGui::PopID();

            return bModified;
        }
    } // namespace

    void FDetailsPanel::Draw(AActor* InSelectedActor) {
        ImGui::Begin("Details");

        if (!InSelectedActor) {
            ImGui::TextDisabled("Select an actor to view details.");
            ImGui::End();
            return;
        }

        // Header with Name and GUID
        char nameBuf[128];
#ifdef _WIN32
        strncpy_s(nameBuf, sizeof(nameBuf), InSelectedActor->GetName().c_str(), _TRUNCATE);
#else
        std::strncpy(nameBuf, InSelectedActor->GetName().c_str(), sizeof(nameBuf) - 1);
        nameBuf[sizeof(nameBuf) - 1] = '\0';
#endif
        ImGui::Text("Name:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
        if (ImGui::InputText("##ActorName", nameBuf, sizeof(nameBuf))) {
            InSelectedActor->SetName(nameBuf);
        }

        ImGui::SameLine();
        DrawAddComponentMenu(*InSelectedActor);

        AActor* parent = InSelectedActor->GetAttachParentActor();
        if (parent) {
            ImGui::TextDisabled("Parent: %s", parent->GetName().c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("Detach")) {
                InSelectedActor->DetachFromActor();
            }
        } else {
            ImGui::TextDisabled("Parent: [Root Level]");
        }

        ImGui::Separator();
        ImGui::Spacing();

        DrawTransformComponent(*InSelectedActor);
        DrawStaticMeshComponent(*InSelectedActor);
        DrawLightComponents(*InSelectedActor);
        DrawCameraComponent(*InSelectedActor);

        ImGui::End();
    }

    void FDetailsPanel::DrawTransformComponent(AActor& InActor) {
        if (!InActor.HasComponent<FTransformComponent>())
            return;

        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            AActor* parent = InActor.GetAttachParentActor();
            if (parent) {
                ImGui::RadioButton("World", !bLocalTransformMode);
                ImGui::SameLine();
                ImGui::RadioButton("Local (Relative to Parent)", bLocalTransformMode);
                ImGui::Spacing();
            }

            if (parent && bLocalTransformMode) {
                glm::vec3 relLoc = InActor.GetRelativeLocation();
                if (DrawVec3Control("Location", relLoc)) {
                    InActor.SetRelativeLocation(relLoc);
                }

                glm::vec3 relRot = InActor.GetRelativeRotation();
                if (DrawVec3Control("Rotation", relRot)) {
                    InActor.SetRelativeRotation(relRot);
                }

                glm::vec3 relScale = InActor.GetRelativeScale();
                if (DrawVec3Control("Scale", relScale, 1.0f)) {
                    InActor.SetRelativeScale(relScale);
                }
            } else {
                auto& tc = InActor.GetComponent<FTransformComponent>();
                DrawVec3Control("Location", tc.Translation);
                DrawVec3Control("Rotation", tc.Rotation);
                DrawVec3Control("Scale", tc.Scale, 1.0f);
            }
        }
    }

    void FDetailsPanel::DrawStaticMeshComponent(AActor& InActor) {
        if (!InActor.HasComponent<FStaticMeshComponent>())
            return;

        if (ImGui::CollapsingHeader("Static Mesh Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& smc = InActor.GetComponent<FStaticMeshComponent>();

            char pathBuf[256];
#ifdef _WIN32
            strncpy_s(pathBuf, sizeof(pathBuf), smc.AssetPath.c_str(), _TRUNCATE);
#else
            std::strncpy(pathBuf, smc.AssetPath.c_str(), sizeof(pathBuf) - 1);
            pathBuf[sizeof(pathBuf) - 1] = '\0';
#endif
            ImGui::Text("Mesh Asset:");
            ImGui::SameLine();
            if (ImGui::InputText("##MeshAssetPath", pathBuf, sizeof(pathBuf))) {
                smc.AssetPath = pathBuf;
            }

            ImGui::Checkbox("Cast Shadows", &smc.bCastShadows);
            ImGui::SameLine();
            ImGui::Checkbox("Receive Shadows", &smc.bReceiveShadows);
            ImGui::SameLine();
            ImGui::Checkbox("Planar Reflection", &smc.bVisibleInReflection);
        }
    }

    void FDetailsPanel::DrawLightComponents(AActor& InActor) {
        if (InActor.HasComponent<FDirectionalLightComponent>()) {
            if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& dlc = InActor.GetComponent<FDirectionalLightComponent>();
                ImGui::Checkbox("Enabled", &dlc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(dlc.Light.Color));
                ImGui::DragFloat("Intensity", &dlc.Light.Intensity, 0.1f, 0.0f, 100.0f);
            }
        }

        if (InActor.HasComponent<FPointLightComponent>()) {
            if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& plc = InActor.GetComponent<FPointLightComponent>();
                ImGui::Checkbox("Enabled", &plc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(plc.Light.Color));
                ImGui::DragFloat("Intensity", &plc.Light.Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Attenuation Radius", &plc.Light.Radius, 0.5f, 0.1f, 500.0f);
            }
        }

        if (InActor.HasComponent<FSpotLightComponent>()) {
            if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& slc = InActor.GetComponent<FSpotLightComponent>();
                ImGui::Checkbox("Enabled", &slc.bEnabled);
                ImGui::ColorEdit3("Light Color", glm::value_ptr(slc.Light.Color));
                ImGui::DragFloat("Intensity", &slc.Light.Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Radius", &slc.Light.Radius, 0.5f, 0.1f, 500.0f);
                ImGui::SliderFloat("Inner Cone", &slc.Light.CutOff, 0.0f, 89.0f, "%.1f deg");
                ImGui::SliderFloat("Outer Cone", &slc.Light.OuterCutOff, 0.0f, 89.0f, "%.1f deg");
            }
        }
    }

    void FDetailsPanel::DrawCameraComponent(AActor& InActor) {
        if (!InActor.HasComponent<FCameraComponent>())
            return;

        if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& cc = InActor.GetComponent<FCameraComponent>();
            ImGui::Checkbox("Primary Camera", &cc.bPrimary);
            float fov = cc.Camera.GetFOV();
            if (ImGui::SliderFloat("FOV", &fov, 30.0f, 120.0f)) {
                cc.Camera.SetFOV(fov);
            }
        }
    }

    void FDetailsPanel::DrawAddComponentMenu(AActor& InActor) {
        if (ImGui::Button("+ Component")) {
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
