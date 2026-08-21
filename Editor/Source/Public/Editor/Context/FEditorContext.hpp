#pragma once

#include "Core/Base.hpp"
#include "Editor/Context/FEditorHistory.hpp"
#include "Editor/Context/FEditorSelection.hpp"

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Leon {
    class UWorld;
    class AActor;
}

namespace Leon::Editor {

    using Leon::UWorld;
    using Leon::AActor;

    /**
     * @brief Centralized state container and event bus for the LeonEditor.
     * Connects Viewport, Details, Outliner, Content Browser, and Toolbar.
     */
    class FEditorContext {
    public:
        using FWorldChangedCallback = std::function<void(UWorld*)>;
        using FProjectChangedCallback = std::function<void(const std::string&)>;

        FEditorContext();
        ~FEditorContext() = default;

        // Active World
        void SetActiveWorld(UWorld* InWorld);
        [[nodiscard]] UWorld* GetActiveWorld() const { return ActiveWorld; }

        // Project and Map State
        void SetActiveProjectPath(const std::string& InPath);
        [[nodiscard]] const std::string& GetActiveProjectPath() const { return ActiveProjectPath; }

        void SetActiveMapPath(const std::string& InPath);
        [[nodiscard]] const std::string& GetActiveMapPath() const { return ActiveMapPath; }

        // Core Managers
        [[nodiscard]] FEditorSelection& GetSelection() { return Selection; }
        [[nodiscard]] const FEditorSelection& GetSelection() const { return Selection; }

        [[nodiscard]] FEditorHistory& GetHistory() { return History; }
        [[nodiscard]] const FEditorHistory& GetHistory() const { return History; }

        /**
         * @brief Apply a selection mutation and record Undo (Unreal-like).
         * Mutator receives the selection; if the set changes, a FSelectActorsCommand is pushed.
         */
        void ModifyActorSelectionWithUndo(const std::function<void(FEditorSelection&)>& InMutator);
        void RecordSpawnedActor(AActor* InActor);

        void MarkMapDirty(bool bInDirty = true);
        void ClearMapDirty() { MarkMapDirty(false); }
        [[nodiscard]] bool IsMapDirty() const { return bMapDirty; }

        void SetActorHiddenInEditor(AActor* InActor, bool bInHidden);
        [[nodiscard]] bool IsActorHiddenInEditor(const AActor* InActor) const;
        void SetActorLockedInEditor(AActor* InActor, bool bInLocked);
        [[nodiscard]] bool IsActorLockedInEditor(const AActor* InActor) const;
        void ClearActorEditorFlags(AActor* InActor);
        void ClearEditorVisibilityState();

        // Callbacks & Events
        void RegisterWorldChangedCallback(FWorldChangedCallback InCallback);
        void RegisterProjectChangedCallback(FProjectChangedCallback InCallback);

        // Status & Notifications
        void SetStatusMessage(const std::string& InMessage);
        [[nodiscard]] const std::string& GetStatusMessage() const { return StatusMessage; }

    private:
        UWorld* ActiveWorld{nullptr};
        std::string ActiveProjectPath;
        std::string ActiveMapPath{"Untitled"};
        std::string StatusMessage{"Ready"};
        bool bMapDirty = false;

        FEditorSelection Selection;
        FEditorHistory History;

        std::unordered_set<AActor*> HiddenActors;
        std::unordered_set<AActor*> LockedActors;

        std::vector<FWorldChangedCallback> WorldChangedCallbacks;
        std::vector<FProjectChangedCallback> ProjectChangedCallbacks;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorContext;
} // namespace Leon
