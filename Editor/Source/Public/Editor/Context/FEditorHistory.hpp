#pragma once

#include "Editor/Commands/IEditorCommand.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Leon::Editor {

    class FEditorHistory {
    public:
        explicit FEditorHistory(size_t InMaxUndoSteps = 64);
        ~FEditorHistory() = default;

        void ExecuteCommand(std::unique_ptr<IEditorCommand> InCommand);
        /** Push a command that already mutated state (e.g. gizmo drag finished). */
        void PushExecutedCommand(std::unique_ptr<IEditorCommand> InCommand);
        bool Undo();
        bool Redo();

        [[nodiscard]] bool CanUndo() const;
        [[nodiscard]] bool CanRedo() const;

        [[nodiscard]] std::string GetUndoDescription() const;
        [[nodiscard]] std::string GetRedoDescription() const;

        void Clear();

    private:
        size_t MaxUndoSteps;
        std::vector<std::unique_ptr<IEditorCommand>> UndoStack;
        std::vector<std::unique_ptr<IEditorCommand>> RedoStack;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorHistory;
} // namespace Leon
