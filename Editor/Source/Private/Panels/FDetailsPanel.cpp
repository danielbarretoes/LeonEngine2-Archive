#include "Editor/Panels/FDetailsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>

namespace Leon::Editor {

    namespace {

        bool MatchesFilter(const std::string& InText, const std::string& InFilter) {
            if (InFilter.empty())
                return true;
            std::string textLower = InText;
            std::transform(textLower.begin(), textLower.end(), textLower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return textLower.find(InFilter) != std::string::npos;
        }

        bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f,
                             float columnWidth = 100.0f) {
            bool bModified = false;
            ImGui::PushID(label.c_str());

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, columnWidth);
            ImGui::Text("%s", label.c_str());
            ImGui::NextColumn();

            ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth() - 32.0f);
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

            // Reset to Default button (↶)
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::SmallButton("##ResetAll")) {
                values = glm::vec3(resetValue);
                bModified = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset to Default");
            }
            ImVec2 rMin = ImGui::GetItemRectMin();
            ImVec2 rMax = ImGui::GetItemRectMax();
            FLucideIcons::DrawIcon(ImGui::GetWindowDrawList(), ImVec2(rMin.x + 2.0f, rMin.y + 2.0f),
                                   ImVec2(rMax.x - 2.0f, rMax.y - 2.0f), ELucideIcon::RefreshCw,
                                   IM_COL32(180, 180, 190, 255));
            ImGui::PopStyleColor();

            ImGui::PopStyleVar();
            ImGui::Columns(1);
            ImGui::PopID();

            return bModified;
        }

    } // namespace

    void FDetailsPanel::Draw(AActor* InSelectedActor, bool* bInOutOpen) {
        ImGui::Begin("Details", bInOutOpen);

        try {
            // Determine active selection from Subsystem if available
            std::vector<AActor*> selectedActors;
            if (SelectionSubsystem && SelectionSubsystem->GetSelectedActorCount() > 0) {
                for (AActor* act : SelectionSubsystem->GetSelectedActors()) {
                    if (act)
                        selectedActors.push_back(act);
                }
            } else if (InSelectedActor) {
                selectedActors.push_back(InSelectedActor);
            }

            if (selectedActors.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("No actor selected.");
                ImGui::TextDisabled("Select an actor in the Viewport or World Outliner to inspect properties.");
                ImGui::End();
                return;
            }

            // Search Bar
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
            ImGui::InputTextWithHint("##DetailsSearch", "Search Details...", SearchBuffer, sizeof(SearchBuffer));
            ImGui::SameLine();
            if (ImGui::SmallButton("X##ClearDetailsSearch")) {
                SearchBuffer[0] = '\0';
            }

            ImGui::Separator();
            ImGui::Spacing();

            std::string filterStr = SearchBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (selectedActors.size() > 1) {
                DrawMultiActorDetails(selectedActors, filterStr);
            } else {
                DrawSingleActorDetails(*selectedActors[0], filterStr);
            }

        } catch (const std::exception& e) {
            LE_CORE_ERROR("FDetailsPanel: Exception during Draw: {0}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Details Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FDetailsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Details: Unknown error encountered");
        }

        ImGui::End();
    }

    void FDetailsPanel::DrawSingleActorDetails(AActor& InActor, const std::string& InFilter) {
        // Dynamic Actor Header
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
        ImGui::BeginChild("ActorHeaderCard", ImVec2(0, 48.0f), true);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cPos = ImGui::GetCursorScreenPos();
        FLucideIcons::DrawIcon(drawList, ImVec2(cPos.x + 4.0f, cPos.y + 4.0f), ImVec2(cPos.x + 28.0f, cPos.y + 28.0f),
                               ELucideIcon::Package, IM_COL32(80, 160, 255, 255));

        ImGui::SetCursorScreenPos(ImVec2(cPos.x + 36.0f, cPos.y + 2.0f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90.0f);
        char nameBuffer[128] = "";
        strncpy_s(nameBuffer, InActor.GetName().c_str(), sizeof(nameBuffer));
        if (ImGui::InputText("##ActorLabelEdit", nameBuffer, sizeof(nameBuffer),
                             ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            if (nameBuffer[0] != '\0') {
                InActor.SetName(nameBuffer);
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(cPos.x + 36.0f, cPos.y + 24.0f));
        ImGui::TextDisabled("%s", InActor.GetClass().empty() ? "AActor" : InActor.GetClass().c_str());

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Components & Properties
        DrawTransformComponent(InActor, InFilter);
        DrawStaticMeshComponent(InActor, InFilter);
        DrawMaterialComponent(InActor, InFilter);
        DrawLightComponents(InActor, InFilter);
        DrawCameraComponent(InActor, InFilter);
        DrawBoxCollisionComponent(InActor, InFilter);

        ImGui::Separator();
        ImGui::Spacing();

        DrawAddComponentMenu(InActor);
    }

    void FDetailsPanel::DrawMultiActorDetails(const std::vector<AActor*>& InActors, const std::string& InFilter) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
        ImGui::BeginChild("MultiActorHeaderCard", ImVec2(0, 42.0f), true);
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "%zu Actors Selected", InActors.size());
        ImGui::TextDisabled("Editing common properties across all selected actors");
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();

        if (MatchesFilter("Transform Location Rotation Scale", InFilter)) {
            if (ImGui::CollapsingHeader("Transform##Multi", ImGuiTreeNodeFlags_DefaultOpen)) {
                glm::vec3 firstLoc = InActors[0]->GetActorLocation();
                bool bSameLoc = true;
                for (size_t i = 1; i < InActors.size(); ++i) {
                    if (InActors[i]->GetActorLocation() != firstLoc) {
                        bSameLoc = false;
                        break;
                    }
                }

                if (bSameLoc) {
                    if (DrawVec3Control("Location", firstLoc, 0.0f)) {
                        for (AActor* act : InActors)
                            act->SetActorLocation(firstLoc);
                    }
                } else {
                    ImGui::TextDisabled("Location: Multiple Values");
                }
            }
        }
    }

    void FDetailsPanel::DrawTransformComponent(AActor& InActor, const std::string& InFilter) {
        if (!MatchesFilter("Transform Location Rotation Scale Mobility", InFilter))
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
        if (!InFilter.empty())
            flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::CollapsingHeader("Transform", flags)) {
            ImGui::Checkbox("Local Transform Mode", &bLocalTransformMode);
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle between Actor World Transform and Component Relative Transform.");
            }

            if (bLocalTransformMode) {
                glm::vec3 relLoc = InActor.GetRelativeLocation();
                if (DrawVec3Control("Relative Location", relLoc, 0.0f)) {
                    InActor.SetRelativeLocation(relLoc);
                }

                glm::vec3 relRot = InActor.GetRelativeRotation();
                if (DrawVec3Control("Relative Rotation", relRot, 0.0f)) {
                    InActor.SetRelativeRotation(relRot);
                }

                glm::vec3 relScale = InActor.GetRelativeScale();
                if (DrawVec3Control("Relative Scale", relScale, 1.0f)) {
                    InActor.SetRelativeScale(relScale);
                }
            } else {
                glm::vec3 location = InActor.GetActorLocation();
                if (DrawVec3Control("Location", location, 0.0f)) {
                    InActor.SetActorLocation(location);
                }

                glm::vec3 rotation = InActor.GetActorRotation();
                if (DrawVec3Control("Rotation", rotation, 0.0f)) {
                    InActor.SetActorRotation(rotation);
                }

                glm::vec3 scale = InActor.GetActorScale();
                if (DrawVec3Control("Scale", scale, 1.0f)) {
                    InActor.SetActorScale(scale);
                }
            }

            // Mobility Enum
            const char* mobilityNames[] = {"Static", "Stationary", "Movable"};
            int currentMobility = 2; // Movable by default
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);
            ImGui::Text("Mobility");
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            ImGui::Combo("##MobilityCombo", &currentMobility, mobilityNames, 3);
            ImGui::Columns(1);
        }
    }

    void FDetailsPanel::DrawStaticMeshComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FStaticMeshComponent>())
            return;
        if (!MatchesFilter("Static Mesh Component Geometry Shadows", InFilter))
            return;

        if (ImGui::CollapsingHeader("Static Mesh Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& meshComp = InActor.GetComponent<FStaticMeshComponent>();

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);

            ImGui::Text("Mesh Asset");
            ImGui::NextColumn();

            char meshPathBuffer[256] = "";
            strncpy_s(meshPathBuffer, meshComp.AssetPath.c_str(), sizeof(meshPathBuffer));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##MeshAssetPath", meshPathBuffer, sizeof(meshPathBuffer))) {
                meshComp.AssetPath = meshPathBuffer;
            }

            // Drag & Drop Target for Mesh Asset
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    std::string droppedPath = static_cast<const char*>(payload->Data);
                    meshComp.AssetPath = droppedPath;
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::NextColumn();
            ImGui::Text("Cast Shadows");
            ImGui::NextColumn();
            ImGui::Checkbox("##CastShadowsMesh", &meshComp.bCastShadows);

            ImGui::Columns(1);
        }
    }

    void FDetailsPanel::DrawMaterialComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FMaterialComponent>())
            return;
        if (!MatchesFilter("Material Component Shader Texture", InFilter))
            return;

        if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& matComp = InActor.GetComponent<FMaterialComponent>();

            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);

            ImGui::Text("Material Asset");
            ImGui::NextColumn();

            char matPathBuffer[256] = "";
            strncpy_s(matPathBuffer, matComp.AssetPath.c_str(), sizeof(matPathBuffer));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##MatAssetPath", matPathBuffer, sizeof(matPathBuffer))) {
                matComp.AssetPath = matPathBuffer;
            }

            // Drag & Drop Target for Material
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    std::string droppedPath = static_cast<const char*>(payload->Data);
                    matComp.AssetPath = droppedPath;
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Columns(1);
        }
    }

    void FDetailsPanel::DrawLightComponents(AActor& InActor, const std::string& InFilter) {
        if (InActor.HasComponent<FDirectionalLightComponent>()) {
            if (MatchesFilter("Directional Light Sun Color Intensity", InFilter)) {
                if (ImGui::CollapsingHeader("Directional Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FDirectionalLightComponent>();
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(0, 100.0f);

                    ImGui::Text("Light Color");
                    ImGui::NextColumn();
                    ImGui::ColorEdit3("##DirColor", glm::value_ptr(comp.Light.Color), ImGuiColorEditFlags_Float);

                    ImGui::NextColumn();
                    ImGui::Text("Intensity");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##DirIntensity", &comp.Light.Intensity, 0.1f, 0.0f, 100.0f, "%.2f");

                    ImGui::NextColumn();
                    ImGui::Text("Enabled");
                    ImGui::NextColumn();
                    ImGui::Checkbox("##DirEnabled", &comp.bEnabled);

                    ImGui::Columns(1);
                }
            }
        }

        if (InActor.HasComponent<FPointLightComponent>()) {
            if (MatchesFilter("Point Light Color Intensity Radius", InFilter)) {
                if (ImGui::CollapsingHeader("Point Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FPointLightComponent>();
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(0, 100.0f);

                    ImGui::Text("Light Color");
                    ImGui::NextColumn();
                    ImGui::ColorEdit3("##PointColor", glm::value_ptr(comp.Light.Color), ImGuiColorEditFlags_Float);

                    ImGui::NextColumn();
                    ImGui::Text("Intensity");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##PointIntensity", &comp.Light.Intensity, 0.1f, 0.0f, 500.0f, "%.2f");

                    ImGui::NextColumn();
                    ImGui::Text("Attenuation Radius");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##PointRadius", &comp.Light.Radius, 0.5f, 0.1f, 1000.0f, "%.1f");

                    ImGui::Columns(1);
                }
            }
        }

        if (InActor.HasComponent<FSpotLightComponent>()) {
            if (MatchesFilter("Spot Light Color Intensity Cone Radius", InFilter)) {
                if (ImGui::CollapsingHeader("Spot Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FSpotLightComponent>();
                    ImGui::Columns(2);
                    ImGui::SetColumnWidth(0, 100.0f);

                    ImGui::Text("Light Color");
                    ImGui::NextColumn();
                    ImGui::ColorEdit3("##SpotColor", glm::value_ptr(comp.Light.Color), ImGuiColorEditFlags_Float);

                    ImGui::NextColumn();
                    ImGui::Text("Intensity");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##SpotIntensity", &comp.Light.Intensity, 0.1f, 0.0f, 500.0f, "%.2f");

                    ImGui::NextColumn();
                    ImGui::Text("Radius");
                    ImGui::NextColumn();
                    ImGui::DragFloat("##SpotRadius", &comp.Light.Radius, 0.5f, 0.1f, 1000.0f, "%.1f");

                    ImGui::NextColumn();
                    ImGui::Text("Inner Cone Angle");
                    ImGui::NextColumn();
                    ImGui::SliderFloat("##SpotInner", &comp.Light.CutOff, 0.0f, comp.Light.OuterCutOff, "%.1f deg");

                    ImGui::NextColumn();
                    ImGui::Text("Outer Cone Angle");
                    ImGui::NextColumn();
                    ImGui::SliderFloat("##SpotOuter", &comp.Light.OuterCutOff, comp.Light.CutOff, 89.0f, "%.1f deg");

                    ImGui::Columns(1);
                }
            }
        }
    }

    void FDetailsPanel::DrawCameraComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FCameraComponent>())
            return;
        if (!MatchesFilter("Camera FOV Clip Planes", InFilter))
            return;

        if (ImGui::CollapsingHeader("Camera Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& camComp = InActor.GetComponent<FCameraComponent>();
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);

            ImGui::Text("FOV");
            ImGui::NextColumn();
            float fov = camComp.Camera.GetFOV();
            if (ImGui::SliderFloat("##CamFOV", &fov, 20.0f, 130.0f, "%.1f deg")) {
                camComp.Camera.SetFOV(fov);
            }

            ImGui::NextColumn();
            ImGui::Text("Near Plane");
            ImGui::NextColumn();
            float nearClip = camComp.Camera.GetNearClip();
            float farClip = camComp.Camera.GetFarClip();
            if (ImGui::DragFloat("##CamNear", &nearClip, 0.01f, 0.001f, 10.0f, "%.3f")) {
                camComp.Camera.SetProjection(fov, camComp.Camera.GetAspectRatio(), nearClip, farClip);
            }

            ImGui::NextColumn();
            ImGui::Text("Far Plane");
            ImGui::NextColumn();
            if (ImGui::DragFloat("##CamFar", &farClip, 1.0f, 10.0f, 100000.0f, "%.0f")) {
                camComp.Camera.SetProjection(fov, camComp.Camera.GetAspectRatio(), nearClip, farClip);
            }

            ImGui::Columns(1);
        }
    }

    void FDetailsPanel::DrawBoxCollisionComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FBoxCollisionComponent>())
            return;
        if (!MatchesFilter("Box Collision Bounds Trigger", InFilter))
            return;

        if (ImGui::CollapsingHeader("Box Collision Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& col = InActor.GetComponent<FBoxCollisionComponent>();
            DrawVec3Control("Min Extent", col.LocalMin, -0.5f);
            DrawVec3Control("Max Extent", col.LocalMax, 0.5f);
            ImGui::Checkbox("Block Movement", &col.bBlockMovement);
        }
    }

    void FDetailsPanel::DrawAddComponentMenu(AActor& InActor) {
        if (ImGui::Button("+ Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 26.0f))) {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup")) {
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Add Component to Actor");
            ImGui::Separator();

            if (!InActor.HasComponent<FStaticMeshComponent>() && ImGui::MenuItem("Static Mesh Component")) {
                InActor.AddComponent<FStaticMeshComponent>();
            }
            if (!InActor.HasComponent<FMaterialComponent>() && ImGui::MenuItem("Material Component")) {
                InActor.AddComponent<FMaterialComponent>();
            }
            if (!InActor.HasComponent<FDirectionalLightComponent>() && ImGui::MenuItem("Directional Light")) {
                InActor.AddComponent<FDirectionalLightComponent>();
            }
            if (!InActor.HasComponent<FPointLightComponent>() && ImGui::MenuItem("Point Light")) {
                InActor.AddComponent<FPointLightComponent>();
            }
            if (!InActor.HasComponent<FSpotLightComponent>() && ImGui::MenuItem("Spot Light")) {
                InActor.AddComponent<FSpotLightComponent>();
            }
            if (!InActor.HasComponent<FCameraComponent>() && ImGui::MenuItem("Camera Component")) {
                InActor.AddComponent<FCameraComponent>();
            }
            if (!InActor.HasComponent<FBoxCollisionComponent>() && ImGui::MenuItem("Box Collision Component")) {
                InActor.AddComponent<FBoxCollisionComponent>();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Leon::Editor
