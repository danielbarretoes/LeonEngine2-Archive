#pragma once

#include "Editor/Commands/IEditorCommand.hpp"
#include "Gameplay/AActor.hpp"

#include <string>
#include <vector>

namespace Leon::Editor {

    class FEditorSelection;

    /** Undoable actor selection change (Outliner / Viewport), Unreal-style. */
    class FSelectActorsCommand : public IEditorCommand {
    public:
        FSelectActorsCommand(FEditorSelection* InSelection, std::vector<AActor*> InBefore,
                             std::vector<AActor*> InAfter);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetDescription() const override { return Description; }

    private:
        FEditorSelection* Selection = nullptr;
        std::vector<AActor*> Before;
        std::vector<AActor*> After;
        std::string Description;
    };

} // namespace Leon::Editor
