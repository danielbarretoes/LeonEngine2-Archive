#include "Editor/Subsystems/FEditorSelectionSubsystem.hpp"
#include "Gameplay/AActor.hpp"

namespace Leon::Editor {

    void FEditorSelectionSubsystem::SelectActor(AActor* InActor, bool bAdditive) {
        if (!InActor) {
            if (!bAdditive) {
                ClearActorSelection();
            }
            return;
        }

        if (!bAdditive) {
            SelectedActors.clear();
        }

        SelectedActors.insert(InActor);
        PrimarySelectedActor = InActor;
        NotifyActorSelectionChanged();
    }

    void FEditorSelectionSubsystem::DeselectActor(AActor* InActor) {
        if (!InActor)
            return;

        SelectedActors.erase(InActor);
        if (PrimarySelectedActor == InActor) {
            PrimarySelectedActor = SelectedActors.empty() ? nullptr : *SelectedActors.begin();
        }
        NotifyActorSelectionChanged();
    }

    void FEditorSelectionSubsystem::ToggleActorSelection(AActor* InActor) {
        if (!InActor)
            return;

        if (IsActorSelected(InActor)) {
            DeselectActor(InActor);
        } else {
            SelectActor(InActor, true);
        }
    }

    void FEditorSelectionSubsystem::SetSelectedActors(const std::vector<AActor*>& InActors) {
        SelectedActors.clear();
        for (AActor* actor : InActors) {
            if (actor) {
                SelectedActors.insert(actor);
            }
        }
        PrimarySelectedActor = SelectedActors.empty() ? nullptr : *SelectedActors.begin();
        NotifyActorSelectionChanged();
    }

    void FEditorSelectionSubsystem::ClearActorSelection() {
        if (SelectedActors.empty() && PrimarySelectedActor == nullptr)
            return;

        SelectedActors.clear();
        PrimarySelectedActor = nullptr;
        NotifyActorSelectionChanged();
    }

    bool FEditorSelectionSubsystem::IsActorSelected(AActor* InActor) const {
        if (!InActor)
            return false;
        return SelectedActors.find(InActor) != SelectedActors.end();
    }

    void FEditorSelectionSubsystem::SelectAsset(const std::string& InAssetPath, bool bAdditive) {
        if (InAssetPath.empty()) {
            if (!bAdditive) {
                ClearAssetSelection();
            }
            return;
        }

        if (!bAdditive) {
            SelectedAssets.clear();
        }

        SelectedAssets.insert(InAssetPath);
        PrimarySelectedAsset = InAssetPath;
        NotifyAssetSelectionChanged();
    }

    void FEditorSelectionSubsystem::DeselectAsset(const std::string& InAssetPath) {
        if (InAssetPath.empty())
            return;

        SelectedAssets.erase(InAssetPath);
        if (PrimarySelectedAsset == InAssetPath) {
            PrimarySelectedAsset = SelectedAssets.empty() ? "" : *SelectedAssets.begin();
        }
        NotifyAssetSelectionChanged();
    }

    void FEditorSelectionSubsystem::ToggleAssetSelection(const std::string& InAssetPath) {
        if (InAssetPath.empty())
            return;

        if (IsAssetSelected(InAssetPath)) {
            DeselectAsset(InAssetPath);
        } else {
            SelectAsset(InAssetPath, true);
        }
    }

    void FEditorSelectionSubsystem::SetSelectedAssets(const std::vector<std::string>& InAssetPaths) {
        SelectedAssets.clear();
        for (const auto& path : InAssetPaths) {
            if (!path.empty()) {
                SelectedAssets.insert(path);
            }
        }
        PrimarySelectedAsset = SelectedAssets.empty() ? "" : *SelectedAssets.begin();
        NotifyAssetSelectionChanged();
    }

    void FEditorSelectionSubsystem::ClearAssetSelection() {
        if (SelectedAssets.empty() && PrimarySelectedAsset.empty())
            return;

        SelectedAssets.clear();
        PrimarySelectedAsset.clear();
        NotifyAssetSelectionChanged();
    }

    bool FEditorSelectionSubsystem::IsAssetSelected(const std::string& InAssetPath) const {
        if (InAssetPath.empty())
            return false;
        return SelectedAssets.find(InAssetPath) != SelectedAssets.end();
    }

    void FEditorSelectionSubsystem::NotifyActorSelectionChanged() {
        if (OnActorSelectionChanged) {
            OnActorSelectionChanged(SelectedActors);
        }
    }

    void FEditorSelectionSubsystem::NotifyAssetSelectionChanged() {
        if (OnAssetSelectionChanged) {
            OnAssetSelectionChanged(SelectedAssets);
        }
    }

} // namespace Leon::Editor
