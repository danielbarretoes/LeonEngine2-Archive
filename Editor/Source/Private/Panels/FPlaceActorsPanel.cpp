#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"
#include <imgui.h>

namespace Leon::Editor {

    void FPlaceActorsPanel::Draw(UWorld* InWorld) {
        ImGui::Begin("Place Actors");

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
            FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 8.0f, curPos.y + 7.0f),
                                   ImVec2(curPos.x + 26.0f, curPos.y + 25.0f), icon, color);
        };

        if (SelectedCategory == 0) {
            drawPlaceItem("   Empty Actor", "Empty", ELucideIcon::Package, IM_COL32(180, 180, 190, 255));
            drawPlaceItem("   Character Actor", "Character", ELucideIcon::PersonStanding, IM_COL32(100, 200, 255, 255));
            drawPlaceItem("   Pawn Actor", "Pawn", ELucideIcon::User, IM_COL32(120, 220, 140, 255));
            drawPlaceItem("   Camera Actor", "Camera", ELucideIcon::Clapperboard, IM_COL32(200, 120, 255, 255));
            drawPlaceItem("   Player Start", "PlayerStart", ELucideIcon::Waypoints, IM_COL32(255, 180, 60, 255));
        } else if (SelectedCategory == 1) {
            drawPlaceItem("   Directional Light", "DirectionalLight", ELucideIcon::Sun, IM_COL32(255, 220, 80, 255));
            drawPlaceItem("   Point Light", "PointLight", ELucideIcon::Lightbulb, IM_COL32(255, 180, 60, 255));
            drawPlaceItem("   Spot Light", "SpotLight", ELucideIcon::Crosshair, IM_COL32(255, 130, 60, 255));
            drawPlaceItem("   Sky Light / Skybox", "Skybox", ELucideIcon::Sun, IM_COL32(100, 220, 255, 255));
        } else if (SelectedCategory == 2) {
            drawPlaceItem("   Cube (Static Mesh)", "Cube", ELucideIcon::Box, IM_COL32(80, 160, 255, 255));
            drawPlaceItem("   Sphere (Static Mesh)", "Sphere", ELucideIcon::Circle, IM_COL32(80, 180, 255, 255));
            drawPlaceItem("   Cylinder (Static Mesh)", "Cylinder", ELucideIcon::Boxes, IM_COL32(100, 160, 255, 255));
            drawPlaceItem("   Plane (Static Mesh)", "Plane", ELucideIcon::Square, IM_COL32(120, 160, 255, 255));
        } else if (SelectedCategory == 3) {
            drawPlaceItem("   Blocking Volume", "BlockingVolume", ELucideIcon::Hexagon, IM_COL32(255, 100, 100, 255));
            drawPlaceItem("   NavMesh Bounds Volume", "NavMeshBoundsVolume", ELucideIcon::LayoutGrid,
                          IM_COL32(100, 255, 150, 255));
            drawPlaceItem("   Physics Volume", "PhysicsVolume", ELucideIcon::Atom, IM_COL32(255, 200, 60, 255));
        }

        ImGui::End();
    }

    void FPlaceActorsPanel::SpawnActor(UWorld& InWorld, const std::string& InType) {
        AActor* spawned = nullptr;

        if (InType == "Empty") {
            spawned = InWorld.SpawnActor("Actor");
        } else if (InType == "DirectionalLight") {
            spawned = InWorld.SpawnActor("DirectionalLight");
            spawned->AddComponent<FDirectionalLightComponent>();
        } else if (InType == "PointLight") {
            spawned = InWorld.SpawnActor("PointLight");
            spawned->AddComponent<FPointLightComponent>();
        } else if (InType == "SpotLight") {
            spawned = InWorld.SpawnActor("SpotLight");
            spawned->AddComponent<FSpotLightComponent>();
        } else if (InType == "Skybox") {
            spawned = InWorld.SpawnActor("SkyboxActor");
            spawned->AddComponent<FSkyboxComponent>();
        } else if (InType == "Camera") {
            spawned = InWorld.SpawnActor("CameraActor");
            spawned->AddComponent<FCameraComponent>();
        } else if (InType == "Cube" || InType == "Sphere" || InType == "Cylinder" || InType == "Plane") {
            spawned = InWorld.SpawnActor(InType + "Actor");
            auto& smc = spawned->AddComponent<FStaticMeshComponent>();
            smc.AssetPath = "/Engine/Assets/Meshes/" + InType + ".lmesh";
        } else {
            spawned = InWorld.SpawnActor(InType);
        }

        if (spawned && OnActorSpawned) {
            OnActorSpawned(spawned);
        }
    }

} // namespace Leon::Editor
