#pragma once

#include "Editor/Commands/FDeleteActorsCommand.hpp"
#include "Editor/Commands/IEditorCommand.hpp"
#include "Gameplay/AActor.hpp"

#include <string>
#include <vector>

namespace Leon::Editor {

    class FEditorSelection;

    /**
     * @brief Duplicate selected actors (Ctrl+D). Undo destroys the duplicates.
     * Offset mirrors Unreal (+100 UU ≈ +1 unit on X in Leon).
     */
    class FDuplicateActorsCommand : public IEditorCommand {
    public:
        FDuplicateActorsCommand(UWorld* InWorld, FEditorSelection* InSelection, std::vector<AActor*> InSources,
                                float InOffsetX = 1.0f);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetDescription() const override { return Description; }

    private:
        UWorld* World = nullptr;
        FEditorSelection* Selection = nullptr;
        std::vector<FActorEditorSnapshot> SourceSnapshots;
        std::vector<AActor*> Duplicates;
        std::vector<AActor*> PreviousSelection;
        float OffsetX = 1.0f;
        std::string Description;
        bool bHasExecuted = false;
    };

} // namespace Leon::Editor
