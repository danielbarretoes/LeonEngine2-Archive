#include "Editor/Context/FEditorSelection.hpp"
#include "Gameplay/AActor.hpp"

#include <algorithm>

namespace Leon::Editor {

    void FEditorSelection::SelectActor(AActor* InActor, bool bAddToSelection) {
        if (!InActor) {
            ClearActorSelection();
            return;
        }

        if (!bAddToSelection) {
            SelectedActors.clear();
            SelectedActorsSet.clear();
        }

        if (SelectedActorsSet.insert(InActor).second) {
            SelectedActors.push_back(InActor);
        }

        NotifyActorSelectionChanged();
    }

    void FEditorSelection::DeselectActor(AActor* InActor) {
        if (!InActor)
            return;

        auto it = SelectedActorsSet.find(InActor);
        if (it != SelectedActorsSet.end()) {
            SelectedActorsSet.erase(it);
            SelectedActors.erase(std::remove(SelectedActors.begin(), SelectedActors.end(), InActor),
                                 SelectedActors.end());
            NotifyActorSelectionChanged();
        }
    }

    void FEditorSelection::ToggleActorSelection(AActor* InActor) {
        if (!InActor)
            return;

        if (IsActorSelected(InActor)) {
            DeselectActor(InActor);
        } else {
            SelectActor(InActor, true);
        }
    }

    void FEditorSelection::SetSelectedActors(const std::vector<AActor*>& InActors) {
        SelectedActors.clear();
        SelectedActorsSet.clear();

        for (AActor* actor : InActors) {
            if (actor && SelectedActorsSet.insert(actor).second) {
                SelectedActors.push_back(actor);
            }
        }

        NotifyActorSelectionChanged();
    }

    void FEditorSelection::ClearActorSelection() {
        if (!SelectedActors.empty()) {
            SelectedActors.clear();
            SelectedActorsSet.clear();
            NotifyActorSelectionChanged();
        }
    }

    bool FEditorSelection::IsActorSelected(const AActor* InActor) const {
        if (!InActor)
            return false;
        return SelectedActorsSet.find(const_cast<AActor*>(InActor)) != SelectedActorsSet.end();
    }

    AActor* FEditorSelection::GetPrimarySelectedActor() const {
        return SelectedActors.empty() ? nullptr : SelectedActors.front();
    }

    void FEditorSelection::SelectAsset(const std::string& InAssetPath, bool bAddToSelection) {
        if (InAssetPath.empty()) {
            ClearAssetSelection();
            return;
        }

        if (!bAddToSelection) {
            SelectedAssets.clear();
            SelectedAssetsSet.clear();
        }

        if (SelectedAssetsSet.insert(InAssetPath).second) {
            SelectedAssets.push_back(InAssetPath);
        }

        NotifyAssetSelectionChanged();
    }

    void FEditorSelection::DeselectAsset(const std::string& InAssetPath) {
        if (InAssetPath.empty())
            return;

        auto it = SelectedAssetsSet.find(InAssetPath);
        if (it != SelectedAssetsSet.end()) {
            SelectedAssetsSet.erase(it);
            SelectedAssets.erase(std::remove(SelectedAssets.begin(), SelectedAssets.end(), InAssetPath),
                                 SelectedAssets.end());
            NotifyAssetSelectionChanged();
        }
    }

    void FEditorSelection::ToggleAssetSelection(const std::string& InAssetPath) {
        if (InAssetPath.empty())
            return;

        if (IsAssetSelected(InAssetPath)) {
            DeselectAsset(InAssetPath);
        } else {
            SelectAsset(InAssetPath, true);
        }
    }

    void FEditorSelection::SetSelectedAssets(const std::vector<std::string>& InAssetPaths) {
        SelectedAssets.clear();
        SelectedAssetsSet.clear();

        for (const auto& path : InAssetPaths) {
            if (!path.empty() && SelectedAssetsSet.insert(path).second) {
                SelectedAssets.push_back(path);
            }
        }

        NotifyAssetSelectionChanged();
    }

    void FEditorSelection::ClearAssetSelection() {
        if (!SelectedAssets.empty()) {
            SelectedAssets.clear();
            SelectedAssetsSet.clear();
            NotifyAssetSelectionChanged();
        }
    }

    bool FEditorSelection::IsAssetSelected(const std::string& InAssetPath) const {
        if (InAssetPath.empty())
            return false;
        return SelectedAssetsSet.find(InAssetPath) != SelectedAssetsSet.end();
    }

    std::string FEditorSelection::GetPrimarySelectedAsset() const {
        return SelectedAssets.empty() ? "" : SelectedAssets.front();
    }

    void FEditorSelection::RegisterActorSelectionCallback(FActorSelectionCallback InCallback) {
        ActorSelectionCallbacks.push_back(std::move(InCallback));
    }

    void FEditorSelection::RegisterAssetSelectionCallback(FAssetSelectionCallback InCallback) {
        AssetSelectionCallbacks.push_back(std::move(InCallback));
    }

    void FEditorSelection::NotifyActorSelectionChanged() {
        for (const auto& cb : ActorSelectionCallbacks) {
            if (cb)
                cb(SelectedActors);
        }
    }

    void FEditorSelection::NotifyAssetSelectionChanged() {
        for (const auto& cb : AssetSelectionCallbacks) {
            if (cb)
                cb(SelectedAssets);
        }
    }

} // namespace Leon::Editor
