#include "Editor/Commands/FSpawnActorsCommand.hpp"
#include "Editor/Context/FEditorSelection.hpp"

namespace Leon::Editor {

    FSpawnActorsCommand::FSpawnActorsCommand(UWorld* InWorld, FEditorSelection* InSelection,
                                             std::vector<AActor*> InActors)
        : World(InWorld), Selection(InSelection) {
        if (Selection)
            PreviousSelection = Selection->GetSelectedActors();

        for (AActor* actor : InActors) {
            if (!actor || actor->IsPendingKill())
                continue;
            Snapshots.push_back(CaptureActorSnapshot(*actor));
            LiveActors.push_back(actor);
        }

        if (Snapshots.size() == 1)
            Description = "Spawn " + Snapshots.front().Name;
        else
            Description = "Spawn " + std::to_string(Snapshots.size()) + " Actors";
    }

    void FSpawnActorsCommand::Execute() {
        if (!World || Snapshots.empty())
            return;

        LiveActors.clear();
        for (const FActorEditorSnapshot& snap : Snapshots) {
            if (AActor* restored = RestoreActorSnapshot(*World, snap))
                LiveActors.push_back(restored);
        }

        if (Selection && !LiveActors.empty())
            Selection->SetSelectedActors(LiveActors);
    }

    void FSpawnActorsCommand::Undo() {
        if (!World)
            return;

        for (AActor* actor : LiveActors) {
            if (actor && !actor->IsPendingKill())
                World->DestroyActor(actor);
        }
        LiveActors.clear();

        if (Selection)
            Selection->SetSelectedActors(PreviousSelection);
    }

    std::string FSpawnActorsCommand::GetDescription() const {
        return Description;
    }

} // namespace Leon::Editor
