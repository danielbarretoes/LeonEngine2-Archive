#include "Editor/Context/FEditorHistory.hpp"

namespace Leon::Editor {

    FEditorHistory::FEditorHistory(size_t InMaxUndoSteps) : MaxUndoSteps(InMaxUndoSteps) {}

    void FEditorHistory::ExecuteCommand(std::unique_ptr<IEditorCommand> InCommand) {
        if (!InCommand)
            return;

        InCommand->Execute();

        RedoStack.clear();

        UndoStack.push_back(std::move(InCommand));
        if (UndoStack.size() > MaxUndoSteps) {
            UndoStack.erase(UndoStack.begin());
        }
    }

    bool FEditorHistory::Undo() {
        if (!CanUndo())
            return false;

        auto command = std::move(UndoStack.back());
        UndoStack.pop_back();

        command->Undo();
        RedoStack.push_back(std::move(command));
        return true;
    }

    bool FEditorHistory::Redo() {
        if (!CanRedo())
            return false;

        auto command = std::move(RedoStack.back());
        RedoStack.pop_back();

        command->Execute();
        UndoStack.push_back(std::move(command));
        return true;
    }

    bool FEditorHistory::CanUndo() const {
        return !UndoStack.empty();
    }

    bool FEditorHistory::CanRedo() const {
        return !RedoStack.empty();
    }

    std::string FEditorHistory::GetUndoDescription() const {
        return UndoStack.empty() ? "" : UndoStack.back()->GetDescription();
    }

    std::string FEditorHistory::GetRedoDescription() const {
        return RedoStack.empty() ? "" : RedoStack.back()->GetDescription();
    }

    void FEditorHistory::Clear() {
        UndoStack.clear();
        RedoStack.clear();
    }

} // namespace Leon::Editor
