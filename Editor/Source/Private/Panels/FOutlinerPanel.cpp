#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Core/FLog.hpp"
#include "Editor/Commands/FDuplicateActorsCommand.hpp"
#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/UClassRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <imgui.h>

namespace Leon::Editor {

    namespace {

        std::string FolderLeafName(const std::string& InPath) {
            const size_t slash = InPath.rfind('/');
            return slash == std::string::npos ? InPath : InPath.substr(slash + 1);
        }

        std::string FolderParentPath(const std::string& InPath) {
            const size_t slash = InPath.rfind('/');
            return slash == std::string::npos ? std::string{} : InPath.substr(0, slash);
        }

        struct FOutlinerFolderEntry {
            std::string Name;
            std::string FullPath;
            std::map<std::string, FOutlinerFolderEntry> Children;
            std::vector<AActor*> Actors;
        };

        void EnsureFolderChain(FOutlinerFolderEntry& InRoot, const std::string& InPath) {
            if (InPath.empty())
                return;
            FOutlinerFolderEntry* node = &InRoot;
            size_t start = 0;
            while (start <= InPath.size()) {
                size_t slash = InPath.find('/', start);
                std::string segment =
                    slash == std::string::npos ? InPath.substr(start) : InPath.substr(start, slash - start);
                if (!segment.empty()) {
                    auto& child = node->Children[segment];
                    if (child.Name.empty()) {
                        child.Name = segment;
                        child.FullPath = (node->FullPath.empty() ? segment : node->FullPath + "/" + segment);
                    }
                    node = &child;
                }
                if (slash == std::string::npos)
                    break;
                start = slash + 1;
            }
        }

        FOutlinerFolderEntry* FindFolderNode(FOutlinerFolderEntry& InRoot, const std::string& InPath) {
            if (InPath.empty())
                return &InRoot;
            FOutlinerFolderEntry* node = &InRoot;
            size_t start = 0;
            while (start <= InPath.size()) {
                size_t slash = InPath.find('/', start);
                std::string segment =
                    slash == std::string::npos ? InPath.substr(start) : InPath.substr(start, slash - start);
                if (!segment.empty()) {
                    auto it = node->Children.find(segment);
                    if (it == node->Children.end())
                        return nullptr;
                    node = &it->second;
                }
                if (slash == std::string::npos)
                    break;
                start = slash + 1;
            }
            return node;
        }

    } // namespace

    void FOutlinerPanel::SetEditorContext(FEditorContext* InContext) {
        Context = InContext;
    }

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

    void FOutlinerPanel::ScrollToActor(AActor* InActor) {
        ActorToScrollTo = InActor;
        bRequestScroll = true;
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
            return dynamic_cast<ACharacter*>(InActor) != nullptr || dynamic_cast<APawn*>(InActor) != nullptr ||
                   dynamic_cast<APlayerStart*>(InActor) != nullptr ||
                   InActor->GetClass().find("Character") != std::string::npos ||
                   InActor->GetClass().find("Pawn") != std::string::npos ||
                   InActor->GetClass().find("PlayerStart") != std::string::npos;
        }
        if (ActiveCategory == EOutlinerFilterCategory::Volumes) {
            return InActor->HasComponent<FBoxCollisionComponent>() ||
                   InActor->GetName().find("Volume") != std::string::npos;
        }
        return true;
    }

    void FOutlinerPanel::HandleActorSelectionClick(AActor* InActor) {
        if (!InActor)
            return;

        const bool bCtrl = ImGui::GetIO().KeyCtrl;
        const bool bShift = ImGui::GetIO().KeyShift;

        if (!Context) {
            FallbackSelectedActor = InActor;
            LastClickedActor = InActor;
            if (OnActorSelected)
                OnActorSelected(InActor);
            return;
        }

        if (bShift && LastClickedActor && !VisibleActorOrder.empty()) {
            auto itA = std::find(VisibleActorOrder.begin(), VisibleActorOrder.end(), LastClickedActor);
            auto itB = std::find(VisibleActorOrder.begin(), VisibleActorOrder.end(), InActor);
            Context->ModifyActorSelectionWithUndo([&](FEditorSelection& selection) {
                if (itA != VisibleActorOrder.end() && itB != VisibleActorOrder.end()) {
                    auto beginIt = itA;
                    auto endIt = itB;
                    if (beginIt > endIt)
                        std::swap(beginIt, endIt);
                    std::vector<AActor*> range;
                    if (bCtrl) {
                        range = selection.GetSelectedActors();
                    }
                    for (auto it = beginIt; it <= endIt; ++it) {
                        if (*it && std::find(range.begin(), range.end(), *it) == range.end())
                            range.push_back(*it);
                    }
                    selection.SetSelectedActors(range);
                } else {
                    selection.SelectActor(InActor, bCtrl);
                }
            });
        } else if (bCtrl) {
            Context->ModifyActorSelectionWithUndo(
                [&](FEditorSelection& selection) { selection.ToggleActorSelection(InActor); });
        } else {
            Context->ModifyActorSelectionWithUndo(
                [&](FEditorSelection& selection) { selection.SelectActor(InActor, false); });
        }

        LastClickedActor = InActor;
        if (OnActorSelected)
            OnActorSelected(InActor);
    }

    void FOutlinerPanel::MoveActorsToFolder(UWorld& InWorld, const std::vector<AActor*>& InActors,
                                            const std::string& InFolderPath) {
        const std::string folder = AActor::NormalizeFolderPath(InFolderPath);
        if (!folder.empty())
            InWorld.RegisterEditorFolder(folder);
        for (AActor* actor : InActors) {
            if (actor)
                actor->SetFolderPath(folder);
        }
    }

    void FOutlinerPanel::MoveSelectedActorsToFolder(UWorld& InWorld, const std::string& InFolderPath) {
        if (!Context)
            return;
        MoveActorsToFolder(InWorld, Context->GetSelection().GetSelectedActors(), InFolderPath);
    }

    void FOutlinerPanel::BeginCreateFolderPopup(const std::string& InParentPath) {
        CreateFolderParentPath = AActor::NormalizeFolderPath(InParentPath);
        CreateFolderBuffer[0] = '\0';
        bCreateFolderPopup = true;
    }

    void FOutlinerPanel::DrawCreateFolderModal(UWorld& InWorld) {
        if (bCreateFolderPopup) {
            ImGui::OpenPopup("Create Outliner Folder");
            bCreateFolderPopup = false;
        }
        if (ImGui::BeginPopupModal("Create Outliner Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(CreateFolderParentPath.empty() ? "New folder at root"
                                                                  : ("Parent: " + CreateFolderParentPath).c_str());
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputText("##NewFolderName", CreateFolderBuffer, sizeof(CreateFolderBuffer));
            if (FEditorWidgets::DrawPrimaryButton(ELucideIcon::FolderPlus, "##CreateFolder", "Create",
                                                  ImVec2(120, 0))) {
                std::string name = CreateFolderBuffer;
                while (!name.empty() && (name.front() == '/' || name.front() == '\\'))
                    name.erase(name.begin());
                if (!name.empty()) {
                    const std::string full =
                        CreateFolderParentPath.empty() ? name : CreateFolderParentPath + "/" + name;
                    InWorld.RegisterEditorFolder(full);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::X, "##CancelCreateFolder", "Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void FOutlinerPanel::BeginRenameFolderPopup(const std::string& InFolderPath) {
        RenameFolderOldPath = AActor::NormalizeFolderPath(InFolderPath);
        strncpy_s(RenameFolderBuffer, FolderLeafName(RenameFolderOldPath).c_str(), sizeof(RenameFolderBuffer));
        bRenameFolderPopup = true;
    }

    void FOutlinerPanel::DrawRenameFolderModal(UWorld& InWorld) {
        if (bRenameFolderPopup) {
            ImGui::OpenPopup("Rename Outliner Folder");
            bRenameFolderPopup = false;
        }
        if (ImGui::BeginPopupModal("Rename Outliner Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputText("##RenameFolderName", RenameFolderBuffer, sizeof(RenameFolderBuffer));
            if (FEditorWidgets::DrawPrimaryButton(ELucideIcon::Pencil, "##RenameFolder", "Rename", ImVec2(120, 0))) {
                std::string leaf = RenameFolderBuffer;
                while (!leaf.empty() && (leaf.front() == '/' || leaf.front() == '\\'))
                    leaf.erase(leaf.begin());
                if (!leaf.empty()) {
                    const std::string parent = FolderParentPath(RenameFolderOldPath);
                    const std::string full = parent.empty() ? leaf : parent + "/" + leaf;
                    InWorld.RenameEditorFolder(RenameFolderOldPath, full);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::X, "##CancelRenameFolder", "Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void FOutlinerPanel::Draw(UWorld* InWorld, bool* bInOutOpen) {
        FEditorWidgets::BeginPanelWindow(FPanelWindowTitles::WorldOutliner, bInOutOpen, ELucideIcon::Layers);

        try {
            if (!InWorld) {
                ImGui::TextDisabled("No active world loaded.");
                ImGui::End();
                return;
            }

            VisibleActorOrder.clear();

            FEditorWidgets::DrawSearchInput("OutlinerSearch", FilterBuffer, sizeof(FilterBuffer), "Search Actors...");

            const char* filterNames[] = {"All", "Meshes", "Lights", "Cameras", "Characters", "Volumes"};
            const ELucideIcon filterIcons[] = {ELucideIcon::LayoutGrid, ELucideIcon::Box,     ELucideIcon::Sun,
                                               ELucideIcon::Clapperboard, ELucideIcon::PersonStanding,
                                               ELucideIcon::Hexagon};
            int cat = static_cast<int>(ActiveCategory);
            if (FEditorWidgets::DrawSegmentedControl("OutlinerCategory", &cat, filterIcons, filterNames, 6)) {
                ActiveCategory = static_cast<EOutlinerFilterCategory>(cat);
            }

            ImGui::Separator();
            ImGui::Spacing();

            std::string filterStr = FilterBuffer;
            std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // Build folder tree
            FOutlinerFolderEntry root;
            root.Name = "";
            root.FullPath = "";
            for (const auto& folder : InWorld->GetEditorFolders())
                EnsureFolderChain(root, folder);

            const auto& allActors = InWorld->GetAllActors();
            for (const auto& actorPtr : allActors) {
                AActor* actor = actorPtr.get();
                if (!actor || actor->GetAttachParentActor() != nullptr)
                    continue;
                if (actor->HasComponent<FSkyboxComponent>() || actor->HasComponent<FWorldSettingsComponent>())
                    continue;

                const std::string& fp = actor->GetFolderPath();
                if (!fp.empty())
                    EnsureFolderChain(root, fp);
                FOutlinerFolderEntry* node = FindFolderNode(root, fp);
                if (node)
                    node->Actors.push_back(actor);
                else
                    root.Actors.push_back(actor);
            }

            ImGui::BeginChild("OutlinerTreeChildRegion", ImVec2(0, -26.0f), false);

            for (auto& [name, child] : root.Children)
                DrawFolderNode(*InWorld, child.FullPath, child.Name, child.Actors, filterStr);

            // Nested folder children of root are drawn above; also draw nested recursively via DrawFolderNode.
            // DrawFolderNode must recurse into Children — pass child map via drawing child.Children inside.

            for (AActor* actor : root.Actors)
                DrawActorNode(*InWorld, actor, filterStr);

            // Drop on empty = root (clear folder + detach)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_PTR")) {
                    AActor* dropped = *reinterpret_cast<AActor**>(payload->Data);
                    if (dropped) {
                        std::vector<AActor*> toMove;
                        if (Context && Context->GetSelection().IsActorSelected(dropped))
                            toMove = Context->GetSelection().GetSelectedActors();
                        else
                            toMove = {dropped};
                        for (AActor* a : toMove) {
                            if (!a)
                                continue;
                            a->DetachFromActor();
                            a->SetFolderPath("");
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                !ImGui::IsAnyItemHovered()) {
                if (Context) {
                    Context->ModifyActorSelectionWithUndo(
                        [](FEditorSelection& selection) { selection.ClearActorSelection(); });
                } else {
                    FallbackSelectedActor = nullptr;
                }
            }

            DrawBackgroundContextMenu(*InWorld);

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::GetIO().WantTextInput) {
                if (OnDeleteRequested)
                    OnDeleteRequested();
            }

            ImGui::EndChild();

            ImGui::Separator();
            size_t totalCount = allActors.size();
            size_t selCount =
                Context ? Context->GetSelection().GetSelectedActorCount() : (FallbackSelectedActor ? 1 : 0);
            ImGui::TextDisabled("%zu Actors  |  %zu Selected  |  %zu Folders", totalCount, selCount,
                                InWorld->GetEditorFolders().size());

            DrawCreateFolderModal(*InWorld);
            DrawRenameFolderModal(*InWorld);

        } catch (const std::exception& e) {
            LE_CORE_ERROR("FOutlinerPanel: Exception during Draw: {}", e.what());
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Outliner Error: %s", e.what());
        } catch (...) {
            LE_CORE_ERROR("FOutlinerPanel: Unknown exception during Draw");
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Outliner: Unknown error encountered");
        }

        ImGui::End();
    }

    void FOutlinerPanel::DrawFolderNode(UWorld& InWorld, const std::string& InFolderPath,
                                        const std::string& InDisplayName, const std::vector<AActor*>& InDirectActors,
                                        const std::string& InFilter) {
        const std::string prefix = InFolderPath + "/";
        std::set<std::string> childNames;
        auto considerPath = [&](const std::string& path) {
            if (path.rfind(prefix, 0) != 0)
                return;
            std::string rest = path.substr(prefix.size());
            size_t slash = rest.find('/');
            std::string child = slash == std::string::npos ? rest : rest.substr(0, slash);
            if (!child.empty())
                childNames.insert(child);
        };
        for (const auto& f : InWorld.GetEditorFolders())
            considerPath(f);
        for (const auto& actorRef : InWorld.GetAllActors()) {
            if (actorRef)
                considerPath(actorRef->GetFolderPath());
        }

        ImGui::PushID(InFolderPath.c_str());
        ImGuiTreeNodeFlags flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 curPos = ImGui::GetCursorScreenPos();
        const bool bOpen = ImGui::TreeNodeEx("##FolderNode", flags, "   %s", InDisplayName.c_str());
        FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 18.0f, curPos.y + 2.0f),
                               ImVec2(curPos.x + 32.0f, curPos.y + 16.0f),
                               bOpen ? ELucideIcon::FolderOpen : ELucideIcon::Folder, IM_COL32(230, 190, 80, 255));

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_PTR")) {
                AActor* dropped = *reinterpret_cast<AActor**>(payload->Data);
                if (dropped) {
                    std::vector<AActor*> toMove;
                    if (Context && Context->GetSelection().IsActorSelected(dropped))
                        toMove = Context->GetSelection().GetSelectedActors();
                    else
                        toMove = {dropped};
                    MoveActorsToFolder(InWorld, toMove, InFolderPath);
                    for (AActor* a : toMove) {
                        if (a)
                            a->DetachFromActor();
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        DrawFolderContextMenu(InWorld, InFolderPath);

        if (bOpen) {
            for (const std::string& childName : childNames) {
                const std::string childPath = InFolderPath + "/" + childName;
                std::vector<AActor*> childActors;
                for (const auto& actorRef : InWorld.GetAllActors()) {
                    AActor* a = actorRef.get();
                    if (!a || a->GetAttachParentActor() != nullptr)
                        continue;
                    if (a->HasComponent<FSkyboxComponent>() || a->HasComponent<FWorldSettingsComponent>())
                        continue;
                    if (a->GetFolderPath() == childPath)
                        childActors.push_back(a);
                }
                DrawFolderNode(InWorld, childPath, childName, childActors, InFilter);
            }
            for (AActor* actor : InDirectActors)
                DrawActorNode(InWorld, actor, InFilter);
            ImGui::TreePop();
        }

        ImGui::PopID();
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

        VisibleActorOrder.push_back(InActor);

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

        if (FEditorWidgets::DrawToolbarIconButton(bIsHidden ? ELucideIcon::EyeOff : ELucideIcon::Eye, "##Vis", true,
                                                  18.0f)) {
            if (Context)
                Context->SetActorHiddenInEditor(InActor, !bIsHidden);
        }
        ImGui::SameLine();

        if (FEditorWidgets::DrawToolbarIconButton(bIsLocked ? ELucideIcon::Lock : ELucideIcon::LockOpen, "##Lock",
                                                  true, 18.0f)) {
            if (Context)
                Context->SetActorLockedInEditor(InActor, !bIsLocked);
        }
        ImGui::SameLine();

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

            const bool bRowClickedL = ImGui::IsItemClicked(0);
            const bool bRowClickedR = ImGui::IsItemClicked(1);
            const bool bRowHovered = ImGui::IsItemHovered();

            FLucideIcons::DrawIcon(drawList, ImVec2(curPos.x + 18.0f, curPos.y + 2.0f),
                                   ImVec2(curPos.x + 32.0f, curPos.y + 16.0f), icon, iconColor);

            float rightEdge = ImGui::GetWindowWidth() - 75.0f;
            if (ImGui::GetCursorPosX() < rightEdge) {
                ImGui::SameLine(rightEdge);
                ImGui::BeginDisabled();
                ImGui::TextDisabled("%s", typeBadge.c_str());
                ImGui::EndDisabled();
            }

            if (bRequestScroll && (InActor == ActorToScrollTo || (ActorToScrollTo == nullptr && bIsSelected))) {
                ImGui::SetScrollHereY(0.5f);
                bRequestScroll = false;
                ActorToScrollTo = nullptr;
            }

            if (!bIsLocked && (bRowClickedL || bRowClickedR)) {
                HandleActorSelectionClick(InActor);
            }

            if (bRowHovered && ImGui::IsMouseDoubleClicked(0)) {
                if (OnActorFocus)
                    OnActorFocus(InActor);
            }
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            AActor* draggedActor = InActor;
            ImGui::SetDragDropPayload("OUTLINER_ACTOR_PTR", &draggedActor, sizeof(AActor*));
            const size_t selCount = Context ? Context->GetSelection().GetSelectedActorCount() : 1;
            if (Context && Context->GetSelection().IsActorSelected(InActor) && selCount > 1)
                ImGui::Text("Move %zu actors", selCount);
            else
                ImGui::Text("Move: %s", actorName.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("OUTLINER_ACTOR_PTR")) {
                AActor* dropped = *reinterpret_cast<AActor**>(payload->Data);
                if (dropped && dropped != InActor) {
                    dropped->AttachToActor(InActor);
                    dropped->SetFolderPath(InActor->GetFolderPath());
                }
            }
            ImGui::EndDragDropTarget();
        }

        DrawContextMenu(InWorld, InActor);

        if (bOpen && bHasChildren) {
            for (AActor* child : attachedChildren) {
                if (child)
                    DrawActorNode(InWorld, child, InFilter);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void FOutlinerPanel::DrawFolderContextMenu(UWorld& InWorld, const std::string& InFolderPath) {
        if (!ImGui::BeginPopupContextItem("FolderNodeContext"))
            return;

        ImGui::TextDisabled("%s", InFolderPath.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Create Subfolder"))
            BeginCreateFolderPopup(InFolderPath);
        if (ImGui::MenuItem("Rename Folder"))
            BeginRenameFolderPopup(InFolderPath);

        if (Context && Context->GetSelection().GetSelectedActorCount() > 0) {
            if (ImGui::MenuItem("Move Selection Here"))
                MoveSelectedActorsToFolder(InWorld, InFolderPath);
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Delete Folder")) {
            InWorld.UnregisterEditorFolder(InFolderPath);
        }

        ImGui::EndPopup();
    }

    void FOutlinerPanel::DrawBackgroundContextMenu(UWorld& InWorld) {
        if (!ImGui::BeginPopupContextWindow("OutlinerBackgroundContextMenu",
                                            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
            return;

        if (ImGui::MenuItem("Create Folder"))
            BeginCreateFolderPopup("");

        if (Context && Context->GetSelection().GetSelectedActorCount() > 0) {
            if (ImGui::MenuItem("Move Selection to Root"))
                MoveSelectedActorsToFolder(InWorld, "");
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Add Actor to Scene");
        ImGui::Separator();
        if (ImGui::MenuItem("Empty Actor"))
            SpawnNewActor(InWorld, "Empty");
        if (ImGui::MenuItem("Static Mesh Cube"))
            SpawnNewActor(InWorld, "Cube");
        if (ImGui::MenuItem("Static Mesh Sphere"))
            SpawnNewActor(InWorld, "Sphere");
        if (ImGui::MenuItem("Static Mesh Cylinder"))
            SpawnNewActor(InWorld, "Cylinder");
        if (ImGui::MenuItem("Static Mesh Plane"))
            SpawnNewActor(InWorld, "Plane");
        ImGui::Separator();
        if (ImGui::MenuItem("Directional Light"))
            SpawnNewActor(InWorld, "DirectionalLight");
        if (ImGui::MenuItem("Point Light"))
            SpawnNewActor(InWorld, "PointLight");
        if (ImGui::MenuItem("Spot Light"))
            SpawnNewActor(InWorld, "SpotLight");
        if (ImGui::MenuItem("Camera Actor"))
            SpawnNewActor(InWorld, "Camera");
        ImGui::EndPopup();
    }

    void FOutlinerPanel::DrawContextMenu(UWorld& InWorld, AActor* InActor) {
        if (!InActor)
            return;

        if (!ImGui::BeginPopupContextItem("ActorNodeContext"))
            return;

        // Keep multi-selection if the actor is already selected.
        if (Context) {
            if (!Context->GetSelection().IsActorSelected(InActor) && !ImGui::GetIO().KeyCtrl &&
                !ImGui::GetIO().KeyShift) {
                Context->GetSelection().SelectActor(InActor, false);
            }
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

        if (ImGui::BeginMenu("Move to Folder")) {
            auto moveTargets = [&]() -> std::vector<AActor*> {
                if (Context && Context->GetSelection().GetSelectedActorCount() > 0)
                    return Context->GetSelection().GetSelectedActors();
                return {InActor};
            };
            if (ImGui::MenuItem("Root"))
                MoveActorsToFolder(InWorld, moveTargets(), "");
            for (const auto& folder : InWorld.GetEditorFolders()) {
                if (ImGui::MenuItem(folder.c_str()))
                    MoveActorsToFolder(InWorld, moveTargets(), folder);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("New Folder..."))
                BeginCreateFolderPopup("");
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
            if (Context) {
                std::vector<AActor*> sources = Context->GetSelection().GetSelectedActors();
                if (sources.empty())
                    sources.push_back(InActor);
                auto command =
                    std::make_unique<FDuplicateActorsCommand>(&InWorld, &Context->GetSelection(), sources, 1.0f);
                Context->GetHistory().ExecuteCommand(std::move(command));
            } else {
                AActor* dup = nullptr;
                const std::string& className = InActor->GetClass();
                if (!className.empty() && UClassRegistry::Get().HasClass(className)) {
                    dup = UClassRegistry::Get().CreateActorOfClass(className, &InWorld, InActor->GetName() + "_Copy");
                }
                if (!dup)
                    dup = InWorld.SpawnActor(InActor->GetName() + "_Copy");
                if (dup && InActor->HasComponent<FTransformComponent>()) {
                    if (!dup->HasComponent<FTransformComponent>())
                        dup->AddComponent<FTransformComponent>();
                    dup->GetComponent<FTransformComponent>() = InActor->GetComponent<FTransformComponent>();
                    dup->GetComponent<FTransformComponent>().Translation += glm::vec3(1.0f, 0.0f, 0.0f);
                    dup->SetFolderPath(InActor->GetFolderPath());
                }
                if (auto* srcStart = dynamic_cast<APlayerStart*>(InActor)) {
                    if (auto* dupStart = dynamic_cast<APlayerStart*>(dup)) {
                        dupStart->SetPlayerStartTag(srcStart->GetPlayerStartTag());
                        dupStart->SetTeamIndex(srcStart->GetTeamIndex());
                        dupStart->SetEnabled(srcStart->IsEnabled());
                    }
                }
            }
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
            if (Context && !Context->GetSelection().IsActorSelected(InActor)) {
                Context->GetSelection().SelectActor(InActor, false);
            } else if (!Context) {
                FallbackSelectedActor = InActor;
            }
            if (OnDeleteRequested)
                OnDeleteRequested();
        }

        ImGui::EndPopup();
    }

    void FOutlinerPanel::SpawnNewActor(UWorld& InWorld, const std::string& InType, const std::string& InFolderPath) {
        AActor* spawned = FPlaceActorsPanel::SpawnActorAt(InWorld, InType, glm::vec3(0.0f, 0.0f, 0.0f));
        if (spawned) {
            if (!InFolderPath.empty())
                spawned->SetFolderPath(InFolderPath);
            if (Context) {
                Context->RecordSpawnedActor(spawned);
            } else {
                FallbackSelectedActor = spawned;
            }
            if (OnActorSelected) {
                OnActorSelected(spawned);
            }
        }
    }

} // namespace Leon::Editor
