#pragma once

#include "Core/Base.hpp"
#include "Editor/Commands/IEditorCommand.hpp"
#include <memory>
#include <vector>

namespace Leon::Editor {

    /**
     * @brief Editor Undo / Redo Command History Stack.
     */
    class FEditorHistory {
    public:
        FEditorHistory() = default;

        void PushAndExecute(std::unique_ptr<IEditorCommand> InCommand);
        bool Undo();
        bool Redo();

        bool CanUndo() const { return UndoIndex > 0; }
        bool CanRedo() const { return UndoIndex < Commands.size(); }

        std::string GetUndoDescription() const;
        std::string GetRedoDescription() const;

        void Clear();

    private:
        std::vector<std::unique_ptr<IEditorCommand>> Commands;
        size_t UndoIndex = 0;
        size_t MaxHistorySize = 100;
    };

} // namespace Leon::Editor
