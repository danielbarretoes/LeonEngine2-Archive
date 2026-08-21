#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ABlockingVolume.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/ASkyLight.hpp"
#include "Gameplay/ATriggerVolume.hpp"
#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include <cstring>
#include <imgui.h>

namespace Leon::Editor {

    AActor* FPlaceActorsPanel::SpawnActorAt(UWorld& InWorld, const std::string& InType, const glm::vec3& InLocation) {
        try {
            AActor* spawned = nullptr;

            if (InType == "PlayerStart") {
                spawned = InWorld.SpawnActor<APlayerStart>("PlayerStart");
            } else if (InType == "Character") {
                spawned = InWorld.SpawnActor<ACharacter>("Character");
            } else if (InType == "Pawn") {
                spawned = InWorld.SpawnActor<APawn>("Pawn");
            } else if (InType == "Camera") {
                spawned = InWorld.SpawnActor<ACameraActor>("CameraActor");
            } else if (InType == "DirectionalLight") {
                spawned = InWorld.SpawnActor("DirectionalLight");
                if (spawned)
                    spawned->AddComponent<FDirectionalLightComponent>();
            } else if (InType == "PointLight") {
                spawned = InWorld.SpawnActor("PointLight");
                if (spawned)
                    spawned->AddComponent<FPointLightComponent>();
            } else if (InType == "SpotLight") {
                spawned = InWorld.SpawnActor("SpotLight");
                if (spawned) {
                    auto& spot = spawned->AddComponent<FSpotLightComponent>();
                    SyncSpotLightFromTransform(spot.Light, InLocation, spawned->GetActorRotation());
                }
            } else if (InType == "Cube" || InType == "Sphere" || InType == "Cylinder" || InType == "Plane" ||
                       InType == "Ramp") {
                spawned = FProceduralPrimitiveSpawner::SpawnShape(&InWorld, InType, InType + "Actor", InLocation);
                return spawned;
            } else if (InType == "BlockingVolume") {
                spawned = InWorld.SpawnActor<ABlockingVolume>("BlockingVolume");
            } else if (InType == "TriggerVolume") {
                spawned = InWorld.SpawnActor<ATriggerVolume>("TriggerVolume");
            } else if (InType == "Skybox" || InType == "SkyLight") {
                for (const auto& existing : InWorld.GetAllActors()) {
                    if (existing && existing->HasComponent<FSkyboxComponent>()) {
                        LE_CORE_WARN("FPlaceActorsPanel: Sky Light already exists ('{}'); maps support one skybox",
                                     existing->GetName());
                        return nullptr;
                    }
                }
                spawned = InWorld.SpawnActor<ASkyLight>("SkyLight");
            } else if (UClassRegistry::Get().HasClass(InType)) {
                spawned = UClassRegistry::Get().CreateActorOfClass(InType, &InWorld, InType);
            } else if (UClassRegistry::Get().HasClass("A" + InType)) {
                spawned = UClassRegistry::Get().CreateActorOfClass("A" + InType, &InWorld, InType);
            } else if (InType == "Empty") {
                spawned = InWorld.SpawnActor("EmptyActor");
            } else {
                spawned = InWorld.SpawnActor(InType + "Actor");
            }

            if (spawned && spawned->HasComponent<FTransformComponent>()) {
                spawned->GetComponent<FTransformComponent>().Translation = InLocation;
            }

            return spawned;
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FPlaceActorsPanel: Exception in SpawnActorAt: {}", e.what());
            return nullptr;
        }
    }

    void FPlaceActorsPanel::Draw(UWorld* InWorld, bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::PlaceActors, bInOutOpen, ELucideIcon::Boxes);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            // Category Tabs
            const char* categories[] = {"Basic", "Lights", "Shapes", "Volumes"};
            const ELucideIcon categoryIcons[] = {ELucideIcon::Package, ELucideIcon::Lightbulb, ELucideIcon::Box,
                                                 ELucideIcon::Hexagon};
            FEditorWidgets::DrawSegmentedControl("PlaceCategory", &SelectedCategory, categoryIcons, categories, 4);

            ImGui::Separator();
            ImGui::Spacing();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            (void)drawList;

            auto drawPlaceItem = [this, InWorld](const char* label, const char* type, ELucideIcon icon) {
                if (FEditorWidgets::DrawButton(icon, type, label, ImVec2(-1.0f, 32.0f))) {
                    SpawnActor(*InWorld, type);
                }

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("PLACE_ACTOR_TYPE", type, std::strlen(type) + 1);
                    ImGui::Text("Spawn: %s", label);
                    ImGui::EndDragDropSource();
                }
            };

            if (SelectedCategory == 0) {
                drawPlaceItem("Empty Actor", "Empty", ELucideIcon::Package);
                drawPlaceItem("Character Actor", "Character", ELucideIcon::PersonStanding);
                drawPlaceItem("Pawn Actor", "Pawn", ELucideIcon::User);
                drawPlaceItem("Camera Actor", "Camera", ELucideIcon::Clapperboard);
                drawPlaceItem("Player Start", "PlayerStart", ELucideIcon::Waypoints);
            } else if (SelectedCategory == 1) {
                drawPlaceItem("Directional Light", "DirectionalLight", ELucideIcon::Sun);
                drawPlaceItem("Point Light", "PointLight", ELucideIcon::Lightbulb);
                drawPlaceItem("Spot Light", "SpotLight", ELucideIcon::Crosshair);
                drawPlaceItem("Sky Light / Skybox", "Skybox", ELucideIcon::Globe);
            } else if (SelectedCategory == 2) {
                drawPlaceItem("Cube", "Cube", ELucideIcon::Box);
                drawPlaceItem("Sphere", "Sphere", ELucideIcon::Circle);
                drawPlaceItem("Cylinder", "Cylinder", ELucideIcon::Boxes);
                drawPlaceItem("Plane", "Plane", ELucideIcon::Square);
                drawPlaceItem("Ramp", "Ramp", ELucideIcon::Layers);
            } else if (SelectedCategory == 3) {
                drawPlaceItem("Blocking Volume", "BlockingVolume", ELucideIcon::Hexagon);
                drawPlaceItem("Trigger Volume", "TriggerVolume", ELucideIcon::Activity);
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FPlaceActorsPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Place Actors Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FPlaceActorsPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Place Actors: Unknown error encountered");
        }

        ImGui::End();
    }

    void FPlaceActorsPanel::SpawnActor(UWorld& InWorld, const std::string& InType) {
        AActor* spawned = SpawnActorAt(InWorld, InType, glm::vec3(0.0f, 0.0f, 0.0f));
        if (spawned && OnActorSpawned) {
            OnActorSpawned(spawned);
        }
    }

} // namespace Leon::Editor
