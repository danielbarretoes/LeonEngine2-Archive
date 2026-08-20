#include "Editor/Context/FEditorContext.hpp"

#include "Editor/Commands/FSelectActorsCommand.hpp"
#include "Gameplay/AActor.hpp"

#include <memory>
#include <unordered_set>

namespace Leon::Editor {

    namespace {

        std::vector<AActor*> SnapshotSelectedActors(const FEditorSelection& InSelection) {
            return InSelection.GetSelectedActors();
        }

        bool SameActorSet(const std::vector<AActor*>& InA, const std::vector<AActor*>& InB) {
            if (InA.size() != InB.size())
                return false;
            std::unordered_set<AActor*> setA(InA.begin(), InA.end());
            for (AActor* actor : InB) {
                if (!setA.count(actor))
                    return false;
            }
            return true;
        }

    } // namespace

    FEditorContext::FEditorContext() : ActiveWorld(nullptr) {}

    void FEditorContext::SetActiveWorld(UWorld* InWorld) {
        if (ActiveWorld != InWorld) {
            ActiveWorld = InWorld;
            Selection.ClearActorSelection();

            for (const auto& cb : WorldChangedCallbacks) {
                if (cb)
                    cb(ActiveWorld);
            }
        }
    }

    void FEditorContext::SetActiveProjectPath(const std::string& InPath) {
        if (ActiveProjectPath != InPath) {
            ActiveProjectPath = InPath;
            for (const auto& cb : ProjectChangedCallbacks) {
                if (cb)
                    cb(ActiveProjectPath);
            }
        }
    }

    void FEditorContext::SetActiveMapPath(const std::string& InPath) {
        ActiveMapPath = InPath;
    }

    void FEditorContext::SetStatusMessage(const std::string& InMessage) {
        StatusMessage = InMessage;
    }

    void FEditorContext::ModifyActorSelectionWithUndo(const std::function<void(FEditorSelection&)>& InMutator) {
        if (!InMutator)
            return;

        const std::vector<AActor*> before = SnapshotSelectedActors(Selection);
        InMutator(Selection);
        const std::vector<AActor*> after = SnapshotSelectedActors(Selection);

        if (SameActorSet(before, after))
            return;

        History.PushExecutedCommand(std::make_unique<FSelectActorsCommand>(&Selection, before, after));
    }

    void FEditorContext::RegisterWorldChangedCallback(FWorldChangedCallback InCallback) {
        WorldChangedCallbacks.push_back(std::move(InCallback));
    }

    void FEditorContext::RegisterProjectChangedCallback(FProjectChangedCallback InCallback) {
        ProjectChangedCallbacks.push_back(std::move(InCallback));
    }

} // namespace Leon::Editor
