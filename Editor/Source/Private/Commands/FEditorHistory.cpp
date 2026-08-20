#include "Editor/Commands/FEditorHistory.hpp"

namespace Leon::Editor {

    void FEditorHistory::PushAndExecute(std::unique_ptr<IEditorCommand> InCommand) {
        if (!InCommand)
            return;

        // Discard forward redo history if we executed a new command
        if (UndoIndex < Commands.size()) {
            Commands.erase(Commands.begin() + UndoIndex, Commands.end());
        }

        InCommand->Execute();
        Commands.push_back(std::move(InCommand));
        UndoIndex = Commands.size();

        if (Commands.size() > MaxHistorySize) {
            Commands.erase(Commands.begin());
            UndoIndex--;
        }
    }

    bool FEditorHistory::Undo() {
        if (!CanUndo())
            return false;
        UndoIndex--;
        Commands[UndoIndex]->Undo();
        return true;
    }

    bool FEditorHistory::Redo() {
        if (!CanRedo())
            return false;
        Commands[UndoIndex]->Execute();
        UndoIndex++;
        return true;
    }

    std::string FEditorHistory::GetUndoDescription() const {
        if (!CanUndo())
            return "";
        return Commands[UndoIndex - 1]->GetDescription();
    }

    std::string FEditorHistory::GetRedoDescription() const {
        if (!CanRedo())
            return "";
        return Commands[UndoIndex]->GetDescription();
    }

    void FEditorHistory::Clear() {
        Commands.clear();
        UndoIndex = 0;
    }

} // namespace Leon::Editor
