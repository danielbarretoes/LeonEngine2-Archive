#include "Editor/Panels/FDetailsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/Commands/FTransformActorsCommand.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Engine/EMobility.hpp"
#include "Gameplay/APlayerStart.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
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

        bool DrawMobilityCombo(const char* InId, int* InOutMobilityIndex) {
            const char* mobilityNames[] = {"Static", "Stationary", "Movable"};
            return FEditorWidgets::DrawPropertySelect("Mobility", InId, InOutMobilityIndex, mobilityNames, 3);
        }

    } // namespace

    void FDetailsPanel::BeginTransformUndoCapture(const std::vector<AActor*>& InActors) {
        if (bTransformUndoPending || !Context)
            return;
        TransformUndoBefore.clear();
        for (AActor* actor : InActors) {
            if (actor && !actor->IsPendingKill())
                TransformUndoBefore.push_back(FTransformActorsCommand::Capture(actor));
        }
        bTransformUndoPending = !TransformUndoBefore.empty();
    }

    void FDetailsPanel::CommitTransformUndoIfIdle() {
        if (!bTransformUndoPending || !Context)
            return;
        if (ImGui::IsAnyItemActive())
            return;

        std::vector<FActorTransformState> after;
        after.reserve(TransformUndoBefore.size());
        bool bAnyChange = false;
        for (const auto& before : TransformUndoBefore) {
            FActorTransformState state = FTransformActorsCommand::Capture(before.Actor);
            after.push_back(state);
            if (FTransformActorsCommand::Differ(before, state))
                bAnyChange = true;
        }

        if (bAnyChange) {
            Context->GetHistory().PushExecutedCommand(std::make_unique<FTransformActorsCommand>(
                std::move(TransformUndoBefore), std::move(after), "Edit Transform"));
        }

        TransformUndoBefore.clear();
        bTransformUndoPending = false;
    }

    void FDetailsPanel::Draw(AActor* InSelectedActor, bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::Details, bInOutOpen, ELucideIcon::Component);

        try {
            // Determine active selection from Context if available
            std::vector<AActor*> selectedActors;
            if (Context && Context->GetSelection().GetSelectedActorCount() > 0) {
                for (AActor* act : Context->GetSelection().GetSelectedActors()) {
                    if (act && !act->IsPendingKill())
                        selectedActors.push_back(act);
                }
            } else if (InSelectedActor && !InSelectedActor->IsPendingKill()) {
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
            FEditorWidgets::DrawSearchInput("DetailsSearch", SearchBuffer, sizeof(SearchBuffer), "Search Details...");

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

            CommitTransformUndoIfIdle();

        } catch (const std::exception& e) {
            LE_CORE_ERROR("FDetailsPanel: Exception during Draw: {}", e.what());
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
        DrawPlayerStartProperties(InActor, InFilter);
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
                    if (FEditorWidgets::DrawVec3Control("Location", firstLoc, 0.0f)) {
                        BeginTransformUndoCapture(InActors);
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
            FEditorWidgets::DrawCheckbox("##LocalTransformMode", &bLocalTransformMode, "Local Transform Mode");
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Toggle between Actor World Transform and Component Relative Transform.");
            }

            if (bLocalTransformMode) {
                glm::vec3 relLoc = InActor.GetRelativeLocation();
                if (FEditorWidgets::DrawVec3Control("Relative Location", relLoc, 0.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetRelativeLocation(relLoc);
                }

                glm::vec3 relRot = InActor.GetRelativeRotation();
                if (FEditorWidgets::DrawVec3Control("Relative Rotation", relRot, 0.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetRelativeRotation(relRot);
                }

                glm::vec3 relScale = InActor.GetRelativeScale();
                if (FEditorWidgets::DrawVec3Control("Relative Scale", relScale, 1.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetRelativeScale(relScale);
                }
            } else {
                glm::vec3 location = InActor.GetActorLocation();
                if (FEditorWidgets::DrawVec3Control("Location", location, 0.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetActorLocation(location);
                }

                glm::vec3 rotation = InActor.GetActorRotation();
                if (FEditorWidgets::DrawVec3Control("Rotation", rotation, 0.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetActorRotation(rotation);
                }

                glm::vec3 scale = InActor.GetActorScale();
                if (FEditorWidgets::DrawVec3Control("Scale", scale, 1.0f)) {
                    BeginTransformUndoCapture({&InActor});
                    InActor.SetActorScale(scale);
                }
            }

            // Mobility: lights use ELightMobility; meshes use EComponentMobility.
            if (InActor.HasComponent<FDirectionalLightComponent>() || InActor.HasComponent<FPointLightComponent>() ||
                InActor.HasComponent<FSpotLightComponent>()) {
                ELightMobility* mobility = nullptr;
                if (InActor.HasComponent<FDirectionalLightComponent>())
                    mobility = &InActor.GetComponent<FDirectionalLightComponent>().Mobility;
                else if (InActor.HasComponent<FPointLightComponent>())
                    mobility = &InActor.GetComponent<FPointLightComponent>().Mobility;
                else
                    mobility = &InActor.GetComponent<FSpotLightComponent>().Mobility;

                int currentMobility = static_cast<int>(*mobility);
                FEditorWidgets::BeginPropertyGrid();
                if (DrawMobilityCombo("##TransformLightMobility", &currentMobility))
                    *mobility = static_cast<ELightMobility>(currentMobility);
                FEditorWidgets::EndPropertyGrid();
            } else if (InActor.HasComponent<FStaticMeshComponent>() || InActor.HasComponent<FMeshComponent>()) {
                EComponentMobility* mobility = nullptr;
                if (InActor.HasComponent<FStaticMeshComponent>())
                    mobility = &InActor.GetComponent<FStaticMeshComponent>().Mobility;
                else
                    mobility = &InActor.GetComponent<FMeshComponent>().Mobility;

                int currentMobility = static_cast<int>(*mobility);
                FEditorWidgets::BeginPropertyGrid();
                if (DrawMobilityCombo("##TransformMeshMobility", &currentMobility))
                    *mobility = static_cast<EComponentMobility>(currentMobility);
                FEditorWidgets::EndPropertyGrid();
            }
        }
    }

    void FDetailsPanel::DrawPlayerStartProperties(AActor& InActor, const std::string& InFilter) {
        auto* start = dynamic_cast<APlayerStart*>(&InActor);
        if (!start)
            return;
        if (!MatchesFilter("PlayerStart Tag Team Spawn", InFilter))
            return;

        if (ImGui::CollapsingHeader("Player Start", ImGuiTreeNodeFlags_DefaultOpen)) {
            char tagBuf[128];
#ifdef _WIN32
            strncpy_s(tagBuf, sizeof(tagBuf), start->GetPlayerStartTag().c_str(), _TRUNCATE);
#else
            std::strncpy(tagBuf, start->GetPlayerStartTag().c_str(), sizeof(tagBuf) - 1);
            tagBuf[sizeof(tagBuf) - 1] = '\0';
#endif
            if (FEditorWidgets::DrawInputText("Player Start Tag", "##PlayerStartTag", tagBuf, sizeof(tagBuf)))
                start->SetPlayerStartTag(tagBuf);

            int team = start->GetTeamIndex();
            FEditorWidgets::BeginPropertyGrid();
            if (FEditorWidgets::DrawPropertyDragInt("Team Index", "##TeamIndex", &team, 1, 0, 32))
                start->SetTeamIndex(team);

            bool bEnabled = start->IsEnabled();
            if (FEditorWidgets::DrawPropertyCheckbox("Enabled", "##PlayerStartEnabled", &bEnabled))
                start->SetEnabled(bEnabled);
            FEditorWidgets::EndPropertyGrid();
        }
    }

    void FDetailsPanel::DrawStaticMeshComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FStaticMeshComponent>())
            return;
        if (!MatchesFilter("Static Mesh Component Geometry Shadows", InFilter))
            return;

        if (ImGui::CollapsingHeader("Static Mesh Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& meshComp = InActor.GetComponent<FStaticMeshComponent>();

            FEditorWidgets::BeginPropertyGrid();

            FEditorWidgets::BeginProperty("Mesh Asset");
            char meshPathBuffer[256] = "";
            strncpy_s(meshPathBuffer, meshComp.AssetPath.c_str(), sizeof(meshPathBuffer));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##MeshAssetPath", meshPathBuffer, sizeof(meshPathBuffer))) {
                meshComp.AssetPath = meshPathBuffer;
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    std::string droppedPath = static_cast<const char*>(payload->Data);
                    meshComp.AssetPath = droppedPath;
                }
                ImGui::EndDragDropTarget();
            }
            FEditorWidgets::EndProperty();

            FEditorWidgets::DrawPropertyCheckbox("Cast Shadows", "##CastShadowsMesh", &meshComp.bCastShadows);

            int mobility = static_cast<int>(meshComp.Mobility);
            if (DrawMobilityCombo("##StaticMeshMobility", &mobility))
                meshComp.Mobility = static_cast<EComponentMobility>(mobility);

            FEditorWidgets::EndPropertyGrid();
        }
    }

    void FDetailsPanel::DrawMaterialComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FMaterialComponent>())
            return;
        if (!MatchesFilter("Material Component Shader Texture", InFilter))
            return;

        if (ImGui::CollapsingHeader("Material Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& matComp = InActor.GetComponent<FMaterialComponent>();

            FEditorWidgets::BeginPropertyGrid();

            FEditorWidgets::BeginProperty("Material Asset");
            char matPathBuffer[256] = "";
            strncpy_s(matPathBuffer, matComp.AssetPath.c_str(), sizeof(matPathBuffer));
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##MatAssetPath", matPathBuffer, sizeof(matPathBuffer))) {
                matComp.AssetPath = matPathBuffer;
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ASSET")) {
                    std::string droppedPath = static_cast<const char*>(payload->Data);
                    matComp.AssetPath = droppedPath;
                }
                ImGui::EndDragDropTarget();
            }
            FEditorWidgets::EndProperty();

            FEditorWidgets::EndPropertyGrid();
        }
    }

    void FDetailsPanel::DrawLightComponents(AActor& InActor, const std::string& InFilter) {
        if (InActor.HasComponent<FDirectionalLightComponent>()) {
            if (MatchesFilter("Directional Light Sun Color Intensity", InFilter)) {
                if (ImGui::CollapsingHeader("Directional Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FDirectionalLightComponent>();
                    FEditorWidgets::BeginPropertyGrid();

                    int mobility = static_cast<int>(comp.Mobility);
                    if (DrawMobilityCombo("##DirMobility", &mobility))
                        comp.Mobility = static_cast<ELightMobility>(mobility);
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip(
                            "Static: fully baked (needs valid lightmaps).\n"
                            "Stationary: baked indirect + dynamic direct (recommended for sun).\n"
                            "Movable: fully dynamic.");
                    }

                    FEditorWidgets::DrawPropertyColorEdit3("Light Color", "##DirColor", glm::value_ptr(comp.Light.Color));
                    FEditorWidgets::DrawPropertyDragFloat("Intensity", "##DirIntensity", &comp.Light.Intensity, 0.1f,
                                                          0.0f, 100.0f);
                    FEditorWidgets::DrawPropertyCheckbox("Enabled", "##DirEnabled", &comp.bEnabled);

                    FEditorWidgets::EndPropertyGrid();
                }
            }
        }

        if (InActor.HasComponent<FPointLightComponent>()) {
            if (MatchesFilter("Point Light Color Intensity Radius", InFilter)) {
                if (ImGui::CollapsingHeader("Point Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FPointLightComponent>();
                    FEditorWidgets::BeginPropertyGrid();

                    int mobility = static_cast<int>(comp.Mobility);
                    if (DrawMobilityCombo("##PointMobility", &mobility))
                        comp.Mobility = static_cast<ELightMobility>(mobility);

                    FEditorWidgets::DrawPropertyColorEdit3("Light Color", "##PointColor",
                                                           glm::value_ptr(comp.Light.Color));
                    FEditorWidgets::DrawPropertyDragFloat("Intensity", "##PointIntensity", &comp.Light.Intensity, 0.1f,
                                                          0.0f, 500.0f);
                    FEditorWidgets::DrawPropertyDragFloat("Attenuation Radius", "##PointRadius", &comp.Light.Radius,
                                                          0.5f, 0.1f, 1000.0f, "%.1f");
                    FEditorWidgets::DrawPropertyCheckbox("Enabled", "##PointEnabled", &comp.bEnabled);

                    FEditorWidgets::EndPropertyGrid();
                }
            }
        }

        if (InActor.HasComponent<FSpotLightComponent>()) {
            if (MatchesFilter("Spot Light Color Intensity Cone Radius", InFilter)) {
                if (ImGui::CollapsingHeader("Spot Light Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& comp = InActor.GetComponent<FSpotLightComponent>();
                    FEditorWidgets::BeginPropertyGrid();

                    int mobility = static_cast<int>(comp.Mobility);
                    if (DrawMobilityCombo("##SpotMobility", &mobility))
                        comp.Mobility = static_cast<ELightMobility>(mobility);

                    FEditorWidgets::DrawPropertyColorEdit3("Light Color", "##SpotColor",
                                                           glm::value_ptr(comp.Light.Color));
                    FEditorWidgets::DrawPropertyDragFloat("Intensity", "##SpotIntensity", &comp.Light.Intensity, 0.1f,
                                                          0.0f, 500.0f);
                    FEditorWidgets::DrawPropertyDragFloat("Radius", "##SpotRadius", &comp.Light.Radius, 0.5f, 0.1f,
                                                          1000.0f, "%.1f");
                    FEditorWidgets::DrawPropertySliderFloat("Inner Cone Angle", "##SpotInner", &comp.Light.CutOff, 0.0f,
                                                            comp.Light.OuterCutOff, "%.1f deg");
                    FEditorWidgets::DrawPropertySliderFloat("Outer Cone Angle", "##SpotOuter", &comp.Light.OuterCutOff,
                                                            comp.Light.CutOff, 89.0f, "%.1f deg");
                    FEditorWidgets::DrawPropertyCheckbox("Enabled", "##SpotEnabled", &comp.bEnabled);

                    FEditorWidgets::EndPropertyGrid();
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
            FEditorWidgets::BeginPropertyGrid();

            float fov = camComp.Camera.GetFOV();
            if (FEditorWidgets::DrawPropertySliderFloat("FOV", "##CamFOV", &fov, 20.0f, 130.0f, "%.1f deg")) {
                camComp.Camera.SetFOV(fov);
            }

            float nearClip = camComp.Camera.GetNearClip();
            float farClip = camComp.Camera.GetFarClip();
            if (FEditorWidgets::DrawPropertyDragFloat("Near Plane", "##CamNear", &nearClip, 0.01f, 0.001f, 10.0f,
                                                     "%.3f")) {
                camComp.Camera.SetProjection(fov, camComp.Camera.GetAspectRatio(), nearClip, farClip);
            }
            if (FEditorWidgets::DrawPropertyDragFloat("Far Plane", "##CamFar", &farClip, 1.0f, 10.0f, 100000.0f,
                                                     "%.0f")) {
                camComp.Camera.SetProjection(fov, camComp.Camera.GetAspectRatio(), nearClip, farClip);
            }

            FEditorWidgets::EndPropertyGrid();
        }
    }

    void FDetailsPanel::DrawBoxCollisionComponent(AActor& InActor, const std::string& InFilter) {
        if (!InActor.HasComponent<FBoxCollisionComponent>())
            return;
        if (!MatchesFilter("Box Collision Bounds Trigger", InFilter))
            return;

        if (ImGui::CollapsingHeader("Box Collision Component", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& col = InActor.GetComponent<FBoxCollisionComponent>();
            FEditorWidgets::DrawVec3Control("Min Extent", col.LocalMin, -0.5f);
            FEditorWidgets::DrawVec3Control("Max Extent", col.LocalMax, 0.5f);
            FEditorWidgets::DrawCheckbox("##BlockMovement", &col.bBlockMovement, "Block Movement");
        }
    }

    void FDetailsPanel::DrawAddComponentMenu(AActor& InActor) {
        if (FEditorWidgets::DrawButton(ELucideIcon::Plus, "##AddComponent", "Add Component",
                                       ImVec2(ImGui::GetContentRegionAvail().x, 26.0f))) {
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
