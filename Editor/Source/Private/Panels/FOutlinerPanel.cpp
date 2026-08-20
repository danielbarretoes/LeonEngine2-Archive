#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/AActor.hpp"

#include <algorithm>
#include <cctype>
#include <imgui.h>

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

        std::string filter = FilterBuffer;
        std::transform(filter.begin(), filter.end(), filter.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        ImGui::BeginChild("OutlinerActorsScroll", ImVec2(0, 0), false);

        const auto& actors = InWorld->GetAllActors();
        for (const auto& actorPtr : actors) {
            AActor* actor = actorPtr.get();
            if (!actor)
                continue;

            // Only draw root actors at the top level; children are drawn recursively
            if (actor->GetAttachParentActor() == nullptr || !filter.empty()) {
                DrawActorNode(*InWorld, actor, filter);
            }
        }

        // Click on empty background to deselect
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered()) {
            SelectedActor = nullptr;
            if (OnActorSelected)
                OnActorSelected(nullptr);
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void FOutlinerPanel::DrawActorNode(UWorld& InWorld, AActor* InActor, const std::string& InFilter) {
        if (!InActor)
            return;

        const std::string& name = InActor->GetName();
        if (!InFilter.empty()) {
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowerName.find(InFilter) == std::string::npos) {
                return;
            }
        }

        const auto& children = InActor->GetAttachedActors();
        bool bHasChildren = !children.empty();

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bHasChildren) {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow;
        } else {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (InActor == SelectedActor) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(InActor);

        // Determine Lucide Icon by component type
        ELucideIcon icon = ELucideIcon::Package;
        ImU32 iconColor = IM_COL32(180, 180, 190, 255);

        if (InActor->HasComponent<FStaticMeshComponent>()) {
            icon = ELucideIcon::Box;
            iconColor = IM_COL32(80, 160, 255, 255);
        } else if (InActor->HasComponent<FDirectionalLightComponent>()) {
            icon = ELucideIcon::Sun;
            iconColor = IM_COL32(255, 220, 80, 255);
        } else if (InActor->HasComponent<FPointLightComponent>()) {
            icon = ELucideIcon::Lightbulb;
            iconColor = IM_COL32(255, 180, 60, 255);
        } else if (InActor->HasComponent<FSpotLightComponent>()) {
            icon = ELucideIcon::Crosshair;
            iconColor = IM_COL32(255, 140, 60, 255);
        } else if (InActor->HasComponent<FCameraComponent>()) {
            icon = ELucideIcon::Clapperboard;
            iconColor = IM_COL32(200, 100, 255, 255);
        } else if (bHasChildren) {
            icon = ELucideIcon::Boxes;
            iconColor = IM_COL32(120, 220, 120, 255);
        }

        // Draw icon
        ImVec2 curPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x, curPos.y + 2.0f), ImVec2(curPos.x + 16.0f, curPos.y + 18.0f),
                               icon, iconColor);
        ImGui::Dummy(ImVec2(18.0f, 18.0f));
        ImGui::SameLine();

        bool bNodeOpen = ImGui::TreeNodeEx((void*)InActor, flags, "%s", name.c_str());

        if (ImGui::IsItemClicked()) {
            SelectedActor = InActor;
            if (OnActorSelected)
                OnActorSelected(InActor);
        }

        // Double-click to focus
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnActorFocus)
                OnActorFocus(InActor);
        }

        // Drag & Drop Source (reparenting)
        if (ImGui::BeginDragDropSource()) {
            AActor* dragActor = InActor;
            ImGui::SetDragDropPayload("OUTLINER_ACTOR", &dragActor, sizeof(AActor*));
            ImGui::Text("Attach: %s", name.c_str());
            ImGui::EndDragDropSource();
        }

        // Drag & Drop Target (attach onto this actor)
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR")) {
                AActor* droppedActor = *(AActor**)payload->Data;
                if (droppedActor && droppedActor != InActor) {
                    droppedActor->AttachToActor(InActor);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Context Menu
        if (ImGui::BeginPopupContextItem()) {
            SelectedActor = InActor;
            if (OnActorSelected)
                OnActorSelected(InActor);
            DrawContextMenu(InWorld, InActor);
            ImGui::EndPopup();
        }

        // Recurse children if opened
        if (bHasChildren && bNodeOpen) {
            for (AActor* child : children) {
                DrawActorNode(InWorld, child, InFilter);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
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

        if (InActor->GetAttachParentActor() != nullptr) {
            if (ImGui::MenuItem("Detach from Parent")) {
                InActor->DetachFromActor();
            }
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
