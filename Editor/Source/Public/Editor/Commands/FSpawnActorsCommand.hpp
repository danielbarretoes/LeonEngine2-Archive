#pragma once

#include "Editor/Commands/FDeleteActorsCommand.hpp"

#include <string>
#include <vector>

namespace Leon::Editor {

    /**
     * Undoable spawn: Execute restores from snapshot, Undo destroys the live actors.
     * First spawn is already in the world — push with FEditorHistory::PushExecutedCommand.
     */
    class FSpawnActorsCommand : public IEditorCommand {
    public:
        FSpawnActorsCommand(UWorld* InWorld, FEditorSelection* InSelection, std::vector<AActor*> InActors);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetDescription() const override;

    private:
        UWorld* World = nullptr;
        FEditorSelection* Selection = nullptr;
        std::vector<FActorEditorSnapshot> Snapshots;
        std::vector<AActor*> LiveActors;
        std::vector<AActor*> PreviousSelection;
        std::string Description;
    };

} // namespace Leon::Editor
