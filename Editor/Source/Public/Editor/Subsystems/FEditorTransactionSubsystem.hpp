#pragma once

#include "Core/Base.hpp"
#include "Editor/Commands/IEditorCommand.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Leon::Editor {

    /**
     * @brief Subsystem managing the Undo/Redo transaction stack for editor operations.
     */
    class FEditorTransactionSubsystem {
    public:
        FEditorTransactionSubsystem() = default;

        void ExecuteCommand(std::unique_ptr<IEditorCommand> InCommand);
        void Undo();
        void Redo();

        bool CanUndo() const { return UndoIndex > 0; }
        bool CanRedo() const { return UndoIndex < static_cast<int>(CommandHistory.size()); }

        std::string GetUndoDescription() const;
        std::string GetRedoDescription() const;

        void ClearHistory();

    private:
        std::vector<std::unique_ptr<IEditorCommand>> CommandHistory;
        int UndoIndex = 0;
        static constexpr size_t MaxHistorySize = 64;
    };

} // namespace Leon::Editor
