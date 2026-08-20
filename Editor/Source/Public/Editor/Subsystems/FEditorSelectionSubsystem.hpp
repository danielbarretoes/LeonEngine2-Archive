#pragma once

#include "Core/Base.hpp"
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Leon {
    class AActor;
}

namespace Leon::Editor {

    /**
     * @brief Central selection manager for actors and assets.
     * Decouples ImGui UI panels from engine state and maintains global editor selection.
     */
    class FEditorSelectionSubsystem {
    public:
        using FOnActorSelectionChanged = std::function<void(const std::unordered_set<AActor*>& InSelectedActors)>;
        using FOnAssetSelectionChanged = std::function<void(const std::unordered_set<std::string>& InSelectedAssets)>;

        FEditorSelectionSubsystem() = default;

        // Actor Selection
        void SelectActor(AActor* InActor, bool bAdditive = false);
        void DeselectActor(AActor* InActor);
        void ToggleActorSelection(AActor* InActor);
        void SetSelectedActors(const std::vector<AActor*>& InActors);
        void ClearActorSelection();

        bool IsActorSelected(AActor* InActor) const;
        AActor* GetPrimarySelectedActor() const { return PrimarySelectedActor; }
        const std::unordered_set<AActor*>& GetSelectedActors() const { return SelectedActors; }
        size_t GetSelectedActorCount() const { return SelectedActors.size(); }

        // Asset Selection
        void SelectAsset(const std::string& InAssetPath, bool bAdditive = false);
        void DeselectAsset(const std::string& InAssetPath);
        void ToggleAssetSelection(const std::string& InAssetPath);
        void SetSelectedAssets(const std::vector<std::string>& InAssetPaths);
        void ClearAssetSelection();

        bool IsAssetSelected(const std::string& InAssetPath) const;
        std::string GetPrimarySelectedAsset() const { return PrimarySelectedAsset; }
        const std::unordered_set<std::string>& GetSelectedAssets() const { return SelectedAssets; }
        size_t GetSelectedAssetCount() const { return SelectedAssets.size(); }

        // Event Callbacks
        void SetOnActorSelectionChanged(FOnActorSelectionChanged InCallback) {
            OnActorSelectionChanged = std::move(InCallback);
        }
        void SetOnAssetSelectionChanged(FOnAssetSelectionChanged InCallback) {
            OnAssetSelectionChanged = std::move(InCallback);
        }

    private:
        void NotifyActorSelectionChanged();
        void NotifyAssetSelectionChanged();

        std::unordered_set<AActor*> SelectedActors;
        AActor* PrimarySelectedActor = nullptr;

        std::unordered_set<std::string> SelectedAssets;
        std::string PrimarySelectedAsset;

        FOnActorSelectionChanged OnActorSelectionChanged;
        FOnAssetSelectionChanged OnAssetSelectionChanged;
    };

} // namespace Leon::Editor
