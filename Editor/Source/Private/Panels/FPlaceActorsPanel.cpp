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
                if (spawned)
                    spawned->AddComponent<FSpotLightComponent>();
            } else if (InType == "Cube" || InType == "Sphere" || InType == "Cylinder" || InType == "Plane") {
                spawned = FProceduralPrimitiveSpawner::SpawnShape(&InWorld, InType, InType + "Actor", InLocation);
                return spawned;
            } else if (InType == "BlockingVolume") {
                spawned = InWorld.SpawnActor<ABlockingVolume>("BlockingVolume");
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
        FEditorWidgets::BeginPanelWindow("  Place Actors", bInOutOpen, ELucideIcon::Boxes);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            // Category Tabs
            const char* categories[] = {"Basic", "Lights", "Shapes", "Volumes"};
            for (int i = 0; i < 4; ++i) {
                if (i > 0)
                    ImGui::SameLine();
                if (ImGui::RadioButton(categories[i], SelectedCategory == i)) {
                    SelectedCategory = i;
                }
            }

            ImGui::Separator();
            ImGui::Spacing();

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            auto drawPlaceItem = [this, InWorld, drawList](const char* label, const char* type, ELucideIcon icon,
                                                           ImU32 color) {
                ImVec2 curPos = ImGui::GetCursorScreenPos();
                if (ImGui::Button(label, ImVec2(-1.0f, 32.0f))) {
                    SpawnActor(*InWorld, type);
                }

                // Drag & Drop Source for Viewport
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                    ImGui::SetDragDropPayload("PLACE_ACTOR_TYPE", type, std::strlen(type) + 1);
                    ImGui::Text("Spawn: %s", label);
                    ImGui::EndDragDropSource();
                }

                FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 8.0f, curPos.y + 7.0f),
                                       ImVec2(curPos.x + 26.0f, curPos.y + 25.0f), icon, color);
            };

            if (SelectedCategory == 0) {
                drawPlaceItem("   Empty Actor", "Empty", ELucideIcon::Package, IM_COL32(180, 180, 190, 255));
                drawPlaceItem("   Character Actor", "Character", ELucideIcon::PersonStanding,
                              IM_COL32(100, 200, 255, 255));
                drawPlaceItem("   Pawn Actor", "Pawn", ELucideIcon::User, IM_COL32(120, 220, 140, 255));
                drawPlaceItem("   Camera Actor", "Camera", ELucideIcon::Clapperboard, IM_COL32(200, 120, 255, 255));
                drawPlaceItem("   Player Start", "PlayerStart", ELucideIcon::Waypoints, IM_COL32(255, 180, 60, 255));
            } else if (SelectedCategory == 1) {
                drawPlaceItem("   Directional Light", "DirectionalLight", ELucideIcon::Sun,
                              IM_COL32(255, 220, 80, 255));
                drawPlaceItem("   Point Light", "PointLight", ELucideIcon::Lightbulb, IM_COL32(255, 180, 60, 255));
                drawPlaceItem("   Spot Light", "SpotLight", ELucideIcon::Crosshair, IM_COL32(255, 130, 60, 255));
                drawPlaceItem("   Sky Light / Skybox", "Skybox", ELucideIcon::Sun, IM_COL32(100, 220, 255, 255));
            } else if (SelectedCategory == 2) {
                drawPlaceItem("   Cube", "Cube", ELucideIcon::Box, IM_COL32(80, 160, 255, 255));
                drawPlaceItem("   Sphere", "Sphere", ELucideIcon::Circle, IM_COL32(80, 180, 255, 255));
                drawPlaceItem("   Cylinder", "Cylinder", ELucideIcon::Boxes, IM_COL32(100, 160, 255, 255));
                drawPlaceItem("   Plane", "Plane", ELucideIcon::Square, IM_COL32(120, 160, 255, 255));
            } else if (SelectedCategory == 3) {
                drawPlaceItem("   Blocking Volume", "BlockingVolume", ELucideIcon::Hexagon,
                              IM_COL32(255, 100, 100, 255));
                drawPlaceItem("   Trigger Volume", "TriggerVolume", ELucideIcon::Activity, IM_COL32(255, 180, 50, 255));
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
