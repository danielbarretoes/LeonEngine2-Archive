#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/Components.hpp"

#include <imgui.h>
#include <algorithm>
#include <cctype>

namespace Leon::Editor {

    void FOutlinerPanel::Draw(UWorld* InWorld) {
        ImGui::Begin("World Outliner");

        if (!InWorld) {
            ImGui::TextDisabled("No active world loaded.");
            ImGui::End();
            return;
        }

        // Search Bar & Add Button
        ImGui::SetNextItemWidth(-100.0f);
        ImGui::InputTextWithHint("##OutlinerFilter", "Search actors...", FilterBuffer, sizeof(FilterBuffer));
        ImGui::SameLine();

        if (ImGui::Button("+ Add Actor", ImVec2(90.0f, 0.0f))) {
            ImGui::OpenPopup("AddActorPopup");
        }

        if (ImGui::BeginPopup("AddActorPopup")) {
            if (ImGui::MenuItem("Static Mesh Actor"))
                SpawnNewActor(*InWorld, "StaticMesh");
            if (ImGui::MenuItem("Directional Light"))
                SpawnNewActor(*InWorld, "DirectionalLight");
            if (ImGui::MenuItem("Point Light"))
                SpawnNewActor(*InWorld, "PointLight");
            if (ImGui::MenuItem("Spot Light"))
                SpawnNewActor(*InWorld, "SpotLight");
            if (ImGui::MenuItem("Camera Actor"))
                SpawnNewActor(*InWorld, "Camera");
            if (ImGui::MenuItem("Empty Actor"))
                SpawnNewActor(*InWorld, "Empty");
            ImGui::EndPopup();
        }

        ImGui::Separator();
        ImGui::Spacing();

        DrawActorTree(*InWorld);

        // Click outside on background to deselect
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
            SelectedActor = nullptr;
            if (OnActorSelected)
                OnActorSelected(nullptr);
        }

        ImGui::End();
    }

    void FOutlinerPanel::DrawActorTree(UWorld& InWorld) {
        const auto& actors = InWorld.GetAllActors();

        std::string filter = FilterBuffer;
        std::transform(filter.begin(), filter.end(), filter.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        ImGui::BeginChild("OutlinerActorsScroll", ImVec2(0, 0), false);

        for (size_t i = 0; i < actors.size(); ++i) {
            AActor* actor = actors[i].get();
            if (!actor)
                continue;

            const std::string& name = actor->GetName();
            if (!filter.empty()) {
                std::string lowerName = name;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lowerName.find(filter) == std::string::npos) {
                    continue;
                }
            }

            ImGui::PushID(static_cast<int>(i));

            bool bIsSelected = (actor == SelectedActor);
            ImGuiTreeNodeFlags flags =
                ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (bIsSelected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            // Determine badge icon / type tag
            const char* typeTag = "[Actor]";
            if (actor->HasComponent<FStaticMeshComponent>())
                typeTag = "[Mesh]";
            else if (actor->HasComponent<FDirectionalLightComponent>())
                typeTag = "[DirLight]";
            else if (actor->HasComponent<FPointLightComponent>())
                typeTag = "[PointLight]";
            else if (actor->HasComponent<FSpotLightComponent>())
                typeTag = "[SpotLight]";
            else if (actor->HasComponent<FCameraComponent>())
                typeTag = "[Camera]";

            ImGui::TextDisabled("%s", typeTag);
            ImGui::SameLine();

            ImGui::TreeNodeEx((void*)(uintptr_t)i, flags, "%s", name.c_str());

            if (ImGui::IsItemClicked()) {
                SelectedActor = actor;
                if (OnActorSelected)
                    OnActorSelected(actor);
            }

            // Double click to focus camera
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (OnActorFocus)
                    OnActorFocus(actor);
            }

            // Context menu for actor
            if (ImGui::BeginPopupContextItem()) {
                SelectedActor = actor;
                if (OnActorSelected)
                    OnActorSelected(actor);
                DrawContextMenu(InWorld, actor);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    void FOutlinerPanel::DrawContextMenu(UWorld& InWorld, AActor* InActor) {
        if (!InActor)
            return;

        ImGui::TextDisabled("Actor: %s", InActor->GetName().c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Focus in Viewport (F)")) {
            if (OnActorFocus)
                OnActorFocus(InActor);
        }

        if (ImGui::MenuItem("Duplicate")) {
            AActor* dup = InWorld.SpawnActor(InActor->GetName() + "_Copy");
            if (dup && InActor->HasComponent<FTransformComponent>()) {
                auto& srcT = InActor->GetComponent<FTransformComponent>();
                auto& dstT = dup->GetComponent<FTransformComponent>();
                dstT.Translation = srcT.Translation + glm::vec3(1.0f, 0.0f, 1.0f);
                dstT.Rotation = srcT.Rotation;
                dstT.Scale = srcT.Scale;
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Delete Actor", "Del")) {
            if (SelectedActor == InActor) {
                SelectedActor = nullptr;
                if (OnActorSelected)
                    OnActorSelected(nullptr);
            }
            InWorld.DestroyActor(InActor);
        }
    }

    void FOutlinerPanel::SpawnNewActor(UWorld& InWorld, const std::string& InType) {
        if (InType == "StaticMesh") {
            AActor* a = InWorld.SpawnActor("StaticMeshActor");
            a->AddComponent<FStaticMeshComponent>();
            SelectedActor = a;
        } else if (InType == "DirectionalLight") {
            AActor* a = InWorld.SpawnActor("DirectionalLight");
            a->AddComponent<FDirectionalLightComponent>();
            SelectedActor = a;
        } else if (InType == "PointLight") {
            AActor* a = InWorld.SpawnActor("PointLight");
            a->AddComponent<FPointLightComponent>();
            SelectedActor = a;
        } else if (InType == "SpotLight") {
            AActor* a = InWorld.SpawnActor("SpotLight");
            a->AddComponent<FSpotLightComponent>();
            SelectedActor = a;
        } else if (InType == "Camera") {
            AActor* a = InWorld.SpawnActor("CameraActor");
            a->AddComponent<FCameraComponent>();
            SelectedActor = a;
        } else {
            AActor* a = InWorld.SpawnActor("Actor");
            SelectedActor = a;
        }

        if (OnActorSelected) {
            OnActorSelected(SelectedActor);
        }
    }

} // namespace Leon::Editor
