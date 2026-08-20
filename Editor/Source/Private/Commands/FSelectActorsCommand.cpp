#include "Editor/Commands/FSelectActorsCommand.hpp"
#include "Editor/Context/FEditorSelection.hpp"

namespace Leon::Editor {

    FSelectActorsCommand::FSelectActorsCommand(FEditorSelection* InSelection, std::vector<AActor*> InBefore,
                                               std::vector<AActor*> InAfter)
        : Selection(InSelection), Before(std::move(InBefore)), After(std::move(InAfter)) {
        if (After.empty())
            Description = "Select None";
        else if (After.size() == 1 && After[0])
            Description = "Select " + After[0]->GetName();
        else
            Description = "Select " + std::to_string(After.size()) + " Actors";
    }

    void FSelectActorsCommand::Execute() {
        if (Selection)
            Selection->SetSelectedActors(After);
    }

    void FSelectActorsCommand::Undo() {
        if (Selection)
            Selection->SetSelectedActors(Before);
    }

} // namespace Leon::Editor
