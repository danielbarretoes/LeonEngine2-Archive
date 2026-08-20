#include "Editor/Commands/FDuplicateActorsCommand.hpp"
#include "Editor/Context/FEditorSelection.hpp"

namespace Leon::Editor {

    FDuplicateActorsCommand::FDuplicateActorsCommand(UWorld* InWorld, FEditorSelection* InSelection,
                                                     std::vector<AActor*> InSources, float InOffsetX)
        : World(InWorld), Selection(InSelection), OffsetX(InOffsetX) {
        if (Selection)
            PreviousSelection = Selection->GetSelectedActors();

        for (AActor* src : InSources) {
            if (src && !src->IsPendingKill())
                SourceSnapshots.push_back(CaptureActorSnapshot(*src));
        }

        if (SourceSnapshots.size() == 1)
            Description = "Duplicate " + SourceSnapshots[0].Name;
        else
            Description = "Duplicate " + std::to_string(SourceSnapshots.size()) + " Actors";
    }

    void FDuplicateActorsCommand::Execute() {
        if (!World || SourceSnapshots.empty())
            return;

        if (bHasExecuted) {
            // Redo: restore from snapshots again (previous duplicates were destroyed on Undo).
            Duplicates.clear();
        }

        Duplicates.clear();
        for (const auto& snap : SourceSnapshots) {
            FActorEditorSnapshot copy = snap;
            copy.Name = snap.Name + "_Copy";
            copy.Guid = FUUID{}; // new identity
            if (copy.bHasTransform)
                copy.Transform.Translation.x += OffsetX;

            AActor* dup = RestoreActorSnapshot(*World, copy);
            if (dup)
                Duplicates.push_back(dup);
        }

        if (Selection && !Duplicates.empty())
            Selection->SetSelectedActors(Duplicates);

        bHasExecuted = true;
    }

    void FDuplicateActorsCommand::Undo() {
        if (!World)
            return;

        for (AActor* dup : Duplicates) {
            if (dup && !dup->IsPendingKill())
                World->DestroyActor(dup);
        }
        Duplicates.clear();

        if (Selection)
            Selection->SetSelectedActors(PreviousSelection);
    }

} // namespace Leon::Editor
