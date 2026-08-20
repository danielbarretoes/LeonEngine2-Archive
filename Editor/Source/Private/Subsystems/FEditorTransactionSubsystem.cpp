#include "Editor/Subsystems/FEditorTransactionSubsystem.hpp"
#include "Core/FLog.hpp"

namespace Leon::Editor {

    void FEditorTransactionSubsystem::ExecuteCommand(std::unique_ptr<IEditorCommand> InCommand) {
        if (!InCommand)
            return;

        try {
            InCommand->Execute();

            // Discard any forward redo history if we are in the middle of stack
            if (UndoIndex < static_cast<int>(CommandHistory.size())) {
                CommandHistory.erase(CommandHistory.begin() + UndoIndex, CommandHistory.end());
            }

            CommandHistory.push_back(std::move(InCommand));
            UndoIndex = static_cast<int>(CommandHistory.size());

            // Limit history size
            if (CommandHistory.size() > MaxHistorySize) {
                CommandHistory.erase(CommandHistory.begin());
                UndoIndex = static_cast<int>(CommandHistory.size());
            }
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FEditorTransactionSubsystem: Error executing command: {0}", e.what());
        }
    }

    void FEditorTransactionSubsystem::Undo() {
        if (!CanUndo())
            return;

        try {
            --UndoIndex;
            CommandHistory[UndoIndex]->Undo();
            LE_CORE_INFO("FEditorTransactionSubsystem: Undo -> {0}", CommandHistory[UndoIndex]->GetDescription());
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FEditorTransactionSubsystem: Error during Undo: {0}", e.what());
        }
    }

    void FEditorTransactionSubsystem::Redo() {
        if (!CanRedo())
            return;

        try {
            CommandHistory[UndoIndex]->Execute();
            LE_CORE_INFO("FEditorTransactionSubsystem: Redo -> {0}", CommandHistory[UndoIndex]->GetDescription());
            ++UndoIndex;
        } catch (const std::exception& e) {
            LE_CORE_ERROR("FEditorTransactionSubsystem: Error during Redo: {0}", e.what());
        }
    }

    std::string FEditorTransactionSubsystem::GetUndoDescription() const {
        if (!CanUndo())
            return "";
        return CommandHistory[UndoIndex - 1]->GetDescription();
    }

    std::string FEditorTransactionSubsystem::GetRedoDescription() const {
        if (!CanRedo())
            return "";
        return CommandHistory[UndoIndex]->GetDescription();
    }

    void FEditorTransactionSubsystem::ClearHistory() {
        CommandHistory.clear();
        UndoIndex = 0;
    }

} // namespace Leon::Editor
