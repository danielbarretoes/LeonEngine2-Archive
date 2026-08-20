#pragma once

#include "Core/Base.hpp"
#include "Editor/Context/FEditorContext.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Leon::Editor {

    enum class EOutlinerFilterCategory { All, StaticMeshes, Lights, Cameras, Characters, Audio, Volumes };

    /**
     * @brief Live World Outliner: folders, actor hierarchy, multi-select (Ctrl/Shift),
     * visibility/lock, and drag-and-drop reparent / move-to-folder.
     */
    class FOutlinerPanel {
    public:
        using FOnActorSelected = std::function<void(AActor* InActor)>;
        using FOnActorFocus = std::function<void(AActor* InActor)>;

        FOutlinerPanel() = default;

        void SetEditorContext(FEditorContext* InContext);
        void SetOnActorSelected(FOnActorSelected InCallback) { OnActorSelected = std::move(InCallback); }
        void SetOnActorFocus(FOnActorFocus InCallback) { OnActorFocus = std::move(InCallback); }
        void SetOnDeleteRequested(std::function<void()> InCallback) { OnDeleteRequested = std::move(InCallback); }

        void SetSelectedActor(AActor* InActor);
        AActor* GetSelectedActor() const;
        void ScrollToActor(AActor* InActor);

        bool IsActorHiddenInEditor(AActor* InActor) const { return HiddenActors.find(InActor) != HiddenActors.end(); }
        bool IsActorLocked(AActor* InActor) const { return LockedActors.find(InActor) != LockedActors.end(); }

        void Draw(UWorld* InWorld, bool* bInOutOpen = nullptr);

    private:
        void DrawFolderNode(UWorld& InWorld, const std::string& InFolderPath, const std::string& InDisplayName,
                            const std::vector<AActor*>& InDirectActors, const std::string& InFilter);
        void DrawActorNode(UWorld& InWorld, AActor* InActor, const std::string& InFilter);
        void DrawContextMenu(UWorld& InWorld, AActor* InActor);
        void DrawFolderContextMenu(UWorld& InWorld, const std::string& InFolderPath);
        void DrawBackgroundContextMenu(UWorld& InWorld);
        void SpawnNewActor(UWorld& InWorld, const std::string& InType, const std::string& InFolderPath = {});
        void HandleActorSelectionClick(AActor* InActor);
        void MoveActorsToFolder(UWorld& InWorld, const std::vector<AActor*>& InActors, const std::string& InFolderPath);
        void MoveSelectedActorsToFolder(UWorld& InWorld, const std::string& InFolderPath);
        bool PassesCategoryFilter(AActor* InActor) const;
        void BeginCreateFolderPopup(const std::string& InParentPath);
        void DrawCreateFolderModal(UWorld& InWorld);
        void BeginRenameFolderPopup(const std::string& InFolderPath);
        void DrawRenameFolderModal(UWorld& InWorld);

        FEditorContext* Context = nullptr;
        FOnActorSelected OnActorSelected;
        FOnActorFocus OnActorFocus;
        std::function<void()> OnDeleteRequested;

        AActor* FallbackSelectedActor = nullptr;
        char FilterBuffer[128] = "";
        EOutlinerFilterCategory ActiveCategory = EOutlinerFilterCategory::All;

        bool bRequestScroll = false;
        AActor* ActorToScrollTo = nullptr;

        std::unordered_set<AActor*> HiddenActors;
        std::unordered_set<AActor*> LockedActors;

        bool bRenamingActor = false;
        AActor* RenameTargetActor = nullptr;
        char RenameBuffer[128] = "";

        /** Flat draw order for Shift-range multi-select (rebuilt each frame). */
        std::vector<AActor*> VisibleActorOrder;
        AActor* LastClickedActor = nullptr;

        bool bCreateFolderPopup = false;
        std::string CreateFolderParentPath;
        char CreateFolderBuffer[128] = "";

        bool bRenameFolderPopup = false;
        std::string RenameFolderOldPath;
        char RenameFolderBuffer[128] = "";
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::EOutlinerFilterCategory;
    using Editor::FOutlinerPanel;
} // namespace Leon
