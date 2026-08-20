#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <imgui.h>

namespace Leon::Editor {

    void FOutlinerPanel::SetSelectedActor(AActor* InActor) {
        if (Context) {
            Context->GetSelection().SelectActor(InActor, false);
        } else {
            FallbackSelectedActor = InActor;
        }
    }

    AActor* FOutlinerPanel::GetSelectedActor() const {
        if (Context) {
            return Context->GetSelection().GetPrimarySelectedActor();
        }
        return FallbackSelectedActor;
    }

    bool FOutlinerPanel::PassesCategoryFilter(AActor* InActor) const {
        if (!InActor)
            return false;
        if (ActiveCategory == EOutlinerFilterCategory::All)
            return true;

        if (ActiveCategory == EOutlinerFilterCategory::StaticMeshes) {
            return InActor->HasComponent<FStaticMeshComponent>() || InActor->HasComponent<FMeshComponent>();
        }
        if (ActiveCategory == EOutlinerFilterCategory::Lights) {
            return InActor->HasComponent<FDirectionalLightComponent>() ||
                   InActor->HasComponent<FPointLightComponent>() || InActor->HasComponent<FSpotLightComponent>();
        }
        if (ActiveCategory == EOutlinerFilterCategory::Cameras) {
            return InActor->HasComponent<FCameraComponent>();
        }
        if (ActiveCategory == EOutlinerFilterCategory::Characters) {
            return InActor->GetName().find("Character") != std::string::npos ||
                   InActor->GetName().find("Player") != std::string::npos;
        }
        if (ActiveCategory == EOutlinerFilterCategory::Volumes) {
            return InActor->HasComponent<FBoxCollisionComponent>() ||
                   InActor->GetName().find("Volume") != std::string::npos;
        }
        return true;
    }

    void FOutlinerPanel::Draw(UWorld* InWorld, bool* bInOutOpen) {
        ImGui::Begin("World Outliner", bInOutOpen);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            // Search Bar
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30.0f);
            ImGui::InputTextWithHint("##OutlinerSearch", "Search Actors...", FilterBuffer, sizeof(FilterBuffer));
            ImGui::SameLine();
            if (ImGui::SmallButton("X##ClearOutlinerSearch")) {
                FilterBuffer[0] = '\0';
            }

            // Quick Category Filters
            const char* filterNames[] = {"All", "Meshes", "Lights", "Cameras", "Characters", "Volumes"};
            for (int i = 0; i < 6; ++i) {
                if (i > 0)
                    ImGui::SameLine();
                bool bActive = (static_cast<int>(ActiveCategory) == i);
                if (bActive)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.8f, 1.0f));
                if (ImGui::SmallButton(filterNames[i])) {
                    ActiveCategory = static_cast<EOutlinerFilterCategory>(i);
                }
                if (bActive)
                    ImGui::PopStyleColor();
            }

            ImGui::Separator();
            ImGui::Spacing();

            std::string filterStr = FilterBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // Tree Scroll Region
            ImGui::BeginChild("OutlinerTreeChildRegion", ImVec2(0, -26.0f), false);

            const auto& allActors = InWorld->GetAllActors();
            for (const auto& actorPtr : allActors) {
                AActor* actor = actorPtr.get();
                if (!actor)
                    continue;

                // Only draw root actors here; attached children will be drawn hierarchically
                if (actor->GetAttachParentActor() == nullptr) {
                    DrawActorNode(*InWorld, actor, filterStr);
                }
            }

            // Drag & Drop to root (detach from parent)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_PTR")) {
                    AActor* dropped = *reinterpret_cast<AActor**>(payload->Data);
                    if (dropped) {
                        dropped->DetachFromActor();
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Empty background click: clear selection
            if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
                if (Context) {
                    Context->GetSelection().ClearActorSelection();
                } else {
                    FallbackSelectedActor = nullptr;
                }
            }

            // Empty background context menu: Add Actor
            if (ImGui::BeginPopupContextWindow("OutlinerBackgroundContextMenu",
                                               ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Add Actor to Scene");
                ImGui::Separator();
                if (ImGui::MenuItem("Empty Actor"))
                    SpawnNewActor(*InWorld, "Empty");
                if (ImGui::MenuItem("Static Mesh Cube"))
                    SpawnNewActor(*InWorld, "Cube");
                if (ImGui::MenuItem("Static Mesh Sphere"))
                    SpawnNewActor(*InWorld, "Sphere");
                if (ImGui::MenuItem("Static Mesh Cylinder"))
                    SpawnNewActor(*InWorld, "Cylinder");
                if (ImGui::MenuItem("Static Mesh Plane"))
                    SpawnNewActor(*InWorld, "Plane");
                ImGui::Separator();
                if (ImGui::MenuItem("Directional Light"))
                    SpawnNewActor(*InWorld, "DirectionalLight");
                if (ImGui::MenuItem("Point Light"))
                    SpawnNewActor(*InWorld, "PointLight");
                if (ImGui::MenuItem("Spot Light"))
                    SpawnNewActor(*InWorld, "SpotLight");
                if (ImGui::MenuItem("Camera Actor"))
                    SpawnNewActor(*InWorld, "Camera");
                ImGui::EndPopup();
            }

            ImGui::EndChild();

            // Footer info
            ImGui::Separator();
            size_t totalCount = allActors.size();
            size_t selCount =
                Context ? Context->GetSelection().GetSelectedActorCount() : (FallbackSelectedActor ? 1 : 0);
            ImGui::TextDisabled("%zu Actors  |  %zu Selected", totalCount, selCount);

        } catch (const std::exception& e) {
            LE_CORE_ERROR("FOutlinerPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Outliner Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FOutlinerPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Outliner: Unknown error encountered");
        }

        ImGui::End();
    }

    void FOutlinerPanel::DrawActorNode(UWorld& InWorld, AActor* InActor, const std::string& InFilter) {
        if (!InActor)
            return;

        std::string actorName = InActor->GetName();
        std::string lowerName = actorName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (!InFilter.empty() && lowerName.find(InFilter) == std::string::npos) {
            return;
        }

        if (!PassesCategoryFilter(InActor)) {
            return;
        }

        const auto& attachedChildren = InActor->GetAttachedActors();
        bool bHasChildren = !attachedChildren.empty();
        bool bIsSelected =
            Context ? Context->GetSelection().IsActorSelected(InActor) : (FallbackSelectedActor == InActor);
        bool bIsLocked = IsActorLocked(InActor);
        bool bIsHidden = IsActorHiddenInEditor(InActor);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (bIsSelected)
            flags |= ImGuiTreeNodeFlags_Selected;
        if (!bHasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        ImGui::PushID(InActor);

        // Visibility Toggle Button (Eye)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::SmallButton(bIsHidden ? "[H]" : "[V]")) {
            if (bIsHidden)
                HiddenActors.erase(InActor);
            else
                HiddenActors.insert(InActor);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Lock Toggle Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (ImGui::SmallButton(bIsLocked ? "[L]" : "[U]")) {
            if (bIsLocked)
                LockedActors.erase(InActor);
            else
                LockedActors.insert(InActor);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        // Node Icon & Tint
        ELucideIcon icon = ELucideIcon::Package;
        ImU32 iconColor = IM_COL32(180, 180, 190, 255);
        std::string typeBadge = "Actor";

        if (InActor->HasComponent<FStaticMeshComponent>() || InActor->HasComponent<FMeshComponent>()) {
            icon = ELucideIcon::Box;
            iconColor = IM_COL32(80, 160, 255, 255);
            typeBadge = "StaticMesh";
        } else if (InActor->HasComponent<FDirectionalLightComponent>()) {
            icon = ELucideIcon::Sun;
            iconColor = IM_COL32(255, 220, 80, 255);
            typeBadge = "DirLight";
        } else if (InActor->HasComponent<FPointLightComponent>()) {
            icon = ELucideIcon::Lightbulb;
            iconColor = IM_COL32(255, 180, 60, 255);
            typeBadge = "PointLight";
        } else if (InActor->HasComponent<FSpotLightComponent>()) {
            icon = ELucideIcon::Crosshair;
            iconColor = IM_COL32(255, 130, 60, 255);
            typeBadge = "SpotLight";
        } else if (InActor->HasComponent<FCameraComponent>()) {
            icon = ELucideIcon::Clapperboard;
            iconColor = IM_COL32(200, 120, 255, 255);
            typeBadge = "Camera";
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 curPos = ImGui::GetCursorScreenPos();

        // If currently renaming this actor, render inline text input
        bool bOpen = false;
        if (bRenamingActor && RenameTargetActor == InActor) {
            ImGui::SetNextItemWidth(180.0f);
            if (ImGui::InputText("##InlineRename", RenameBuffer, sizeof(RenameBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                if (RenameBuffer[0] != '\0') {
                    InActor->SetName(RenameBuffer);
                }
                bRenamingActor = false;
                RenameTargetActor = nullptr;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                bRenamingActor = false;
                RenameTargetActor = nullptr;
            }
        } else {
            bOpen = ImGui::TreeNodeEx("##ActorTreeNode", flags, "   %s", actorName.c_str());

            // Draw Lucide icon
            FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 18.0f, curPos.y + 2.0f),
                                   ImVec2(curPos.x + 32.0f, curPos.y + 16.0f), icon, iconColor);

            // Right Type Badge
            float rightEdge = ImGui::GetWindowWidth() - 75.0f;
            if (ImGui::GetCursorPosX() < rightEdge) {
                ImGui::SameLine(rightEdge);
                ImGui::TextDisabled("%s", typeBadge.c_str());
            }
        }

        // Selection Handling with Ctrl/Shift
        if (!bIsLocked && (ImGui::IsItemClicked(0) || ImGui::IsItemClicked(1))) {
            bool bCtrl = ImGui::GetIO().KeyCtrl;
            if (Context) {
                if (bCtrl) {
                    Context->GetSelection().ToggleActorSelection(InActor);
                } else {
                    Context->GetSelection().SelectActor(InActor, false);
                }
            } else {
                FallbackSelectedActor = InActor;
            }

            if (OnActorSelected) {
                OnActorSelected(InActor);
            }
        }

        // Double Click: Focus Camera on Actor
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (OnActorFocus) {
                OnActorFocus(InActor);
            }
        }

        // Drag & Drop Source: Re-parenting
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            AActor* draggedActor = InActor;
            ImGui::SetDragDropPayload("OUTLINER_ACTOR_PTR", &draggedActor, sizeof(AActor*));
            ImGui::Text("Attach: %s", actorName.c_str());
            ImGui::EndDragDropSource();
        }

        // Drag & Drop Target: Attach child to this actor
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_PTR")) {
                AActor* dropped = *reinterpret_cast<AActor**>(payload->Data);
                if (dropped && dropped != InActor) {
                    dropped->AttachToActor(InActor);
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Context Menu
        DrawContextMenu(InWorld, InActor);

        // Recursive hierarchy for children
        if (bOpen && bHasChildren) {
            for (AActor* child : attachedChildren) {
                if (child) {
                    DrawActorNode(InWorld, child, InFilter);
                }
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void FOutlinerPanel::DrawContextMenu(UWorld& InWorld, AActor* InActor) {
        if (!InActor)
            return;

        if (ImGui::BeginPopupContextItem("ActorNodeContext")) {
            if (Context) {
                Context->GetSelection().SelectActor(InActor, false);
            } else {
                FallbackSelectedActor = InActor;
            }

            ImGui::TextDisabled("%s", InActor->GetName().c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Focus in Viewport", "F")) {
                if (OnActorFocus)
                    OnActorFocus(InActor);
            }

            if (ImGui::MenuItem("Rename", "F2")) {
                bRenamingActor = true;
                RenameTargetActor = InActor;
                strncpy_s(RenameBuffer, InActor->GetName().c_str(), sizeof(RenameBuffer));
            }

            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                AActor* dup = InWorld.SpawnActor(InActor->GetName() + "_Copy");
                if (dup && InActor->HasComponent<FTransformComponent>()) {
                    dup->GetComponent<FTransformComponent>() = InActor->GetComponent<FTransformComponent>();
                    dup->GetComponent<FTransformComponent>().Translation += glm::vec3(1.0f, 0.0f, 0.0f);
                }
                if (Context)
                    Context->GetSelection().SelectActor(dup, false);
            }

            if (InActor->GetAttachParentActor() != nullptr) {
                if (ImGui::MenuItem("Detach from Parent")) {
                    InActor->DetachFromActor();
                }
            }

            if (ImGui::BeginMenu("Transform")) {
                if (ImGui::MenuItem("Reset Location")) {
                    if (InActor->HasComponent<FTransformComponent>()) {
                        InActor->GetComponent<FTransformComponent>().Translation = glm::vec3(0.0f);
                    }
                }
                if (ImGui::MenuItem("Reset Rotation")) {
                    if (InActor->HasComponent<FTransformComponent>()) {
                        InActor->GetComponent<FTransformComponent>().Rotation = glm::vec3(0.0f);
                    }
                }
                if (ImGui::MenuItem("Reset Scale")) {
                    if (InActor->HasComponent<FTransformComponent>()) {
                        InActor->GetComponent<FTransformComponent>().Scale = glm::vec3(1.0f);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del")) {
                InWorld.DestroyActor(InActor);
                if (Context)
                    Context->GetSelection().ClearActorSelection();
            }

            ImGui::EndPopup();
        }
    }

    void FOutlinerPanel::SpawnNewActor(UWorld& InWorld, const std::string& InType) {
        AActor* spawned = FPlaceActorsPanel::SpawnActorAt(InWorld, InType, glm::vec3(0.0f, 0.0f, 0.0f));
        if (spawned) {
            if (Context) {
                Context->GetSelection().SelectActor(spawned, false);
            } else {
                FallbackSelectedActor = spawned;
            }
            if (OnActorSelected) {
                OnActorSelected(spawned);
            }
        }
    }

} // namespace Leon::Editor
