#pragma once

#include "Core/Base.hpp"
#include "Editor/Context/FEditorContext.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <functional>
#include <string>
#include <unordered_set>

namespace Leon::Editor {

    enum class EOutlinerFilterCategory { All, StaticMeshes, Lights, Cameras, Characters, Audio, Volumes };

    /**
     * @brief Live World Outliner panel: displays actor hierarchy, component subtrees,
     * type icons, visibility toggles, lock controls, drag-and-drop re-parenting, and selection synchronization.
     */
    class FOutlinerPanel {
    public:
        using FOnActorSelected = std::function<void(AActor* InActor)>;
        using FOnActorFocus = std::function<void(AActor* InActor)>;

        FOutlinerPanel() = default;

        void SetEditorContext(FEditorContext* InContext) { Context = InContext; }
        void SetOnActorSelected(FOnActorSelected InCallback) { OnActorSelected = std::move(InCallback); }
        void SetOnActorFocus(FOnActorFocus InCallback) { OnActorFocus = std::move(InCallback); }

        void SetSelectedActor(AActor* InActor);
        AActor* GetSelectedActor() const;

        bool IsActorHiddenInEditor(AActor* InActor) const { return HiddenActors.find(InActor) != HiddenActors.end(); }
        bool IsActorLocked(AActor* InActor) const { return LockedActors.find(InActor) != LockedActors.end(); }

        void Draw(UWorld* InWorld, bool* bInOutOpen = nullptr);

    private:
        void DrawActorNode(UWorld& InWorld, AActor* InActor, const std::string& InFilter);
        void DrawContextMenu(UWorld& InWorld, AActor* InActor);
        void SpawnNewActor(UWorld& InWorld, const std::string& InType);
        bool PassesCategoryFilter(AActor* InActor) const;

        FEditorContext* Context = nullptr;
        FOnActorSelected OnActorSelected;
        FOnActorFocus OnActorFocus;

        AActor* FallbackSelectedActor = nullptr;
        char FilterBuffer[128] = "";
        EOutlinerFilterCategory ActiveCategory = EOutlinerFilterCategory::All;

        // Visibility & Lock States
        std::unordered_set<AActor*> HiddenActors;
        std::unordered_set<AActor*> LockedActors;

        // Renaming
        bool bRenamingActor = false;
        AActor* RenameTargetActor = nullptr;
        char RenameBuffer[128] = "";
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::EOutlinerFilterCategory;
    using Editor::FOutlinerPanel;
} // namespace Leon
