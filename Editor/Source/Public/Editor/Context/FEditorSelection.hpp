#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace Leon {

    class AActor;

    class FEditorSelection {
    public:
        using FActorSelectionCallback = std::function<void(const std::vector<AActor*>&)>;
        using FAssetSelectionCallback = std::function<void(const std::vector<std::string>&)>;

        FEditorSelection() = default;
        ~FEditorSelection() = default;

        // Actor Selection
        void SelectActor(AActor* InActor, bool bAddToSelection = false);
        void DeselectActor(AActor* InActor);
        void ToggleActorSelection(AActor* InActor);
        void SetSelectedActors(const std::vector<AActor*>& InActors);
        void ClearActorSelection();

        [[nodiscard]] bool IsActorSelected(const AActor* InActor) const;
        [[nodiscard]] const std::vector<AActor*>& GetSelectedActors() const { return SelectedActors; }
        [[nodiscard]] size_t GetSelectedActorCount() const { return SelectedActors.size(); }
        [[nodiscard]] AActor* GetPrimarySelectedActor() const;

        // Asset Selection
        void SelectAsset(const std::string& InAssetPath, bool bAddToSelection = false);
        void DeselectAsset(const std::string& InAssetPath);
        void ToggleAssetSelection(const std::string& InAssetPath);
        void SetSelectedAssets(const std::vector<std::string>& InAssetPaths);
        void ClearAssetSelection();

        [[nodiscard]] bool IsAssetSelected(const std::string& InAssetPath) const;
        [[nodiscard]] const std::vector<std::string>& GetSelectedAssets() const { return SelectedAssets; }
        [[nodiscard]] size_t GetSelectedAssetCount() const { return SelectedAssets.size(); }
        [[nodiscard]] std::string GetPrimarySelectedAsset() const;

        // Callbacks & Observers
        void RegisterActorSelectionCallback(FActorSelectionCallback InCallback);
        void RegisterAssetSelectionCallback(FAssetSelectionCallback InCallback);

    private:
        void NotifyActorSelectionChanged();
        void NotifyAssetSelectionChanged();

        std::vector<AActor*> SelectedActors;
        std::unordered_set<AActor*> SelectedActorsSet;

        std::vector<std::string> SelectedAssets;
        std::unordered_set<std::string> SelectedAssetsSet;

        std::vector<FActorSelectionCallback> ActorSelectionCallbacks;
        std::vector<FAssetSelectionCallback> AssetSelectionCallbacks;
    };

} // namespace Leon
