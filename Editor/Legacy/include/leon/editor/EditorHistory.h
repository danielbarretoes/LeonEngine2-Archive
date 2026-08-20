#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace leon {
class Engine;
} // namespace leon

namespace leon::editor {

struct EditorContext;

/// Snapshot-based undo/redo over `.llev` byte snapshots (Unreal-like transaction stack lite).
class EditorHistory {
public:
    /// Capture current level+camera before a mutating edit (optional Unreal-like name).
    /// Returns true if a new undo entry was pushed (false if skipped / applying).
    bool Capture(const EditorContext& ctx, std::string_view transactionName = "Edit");
    /// Drop the last Capture when a follow-up mutation failed (only if Capture returned true).
    void DiscardLastCapture();
    [[nodiscard]] bool CanUndo() const { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const { return !redo_.empty(); }
    [[nodiscard]] std::string_view UndoTransactionName() const {
        return undoNames_.empty() ? std::string_view{} : std::string_view{undoNames_.back()};
    }
    [[nodiscard]] std::string_view RedoTransactionName() const {
        return redoNames_.empty() ? std::string_view{} : std::string_view{redoNames_.back()};
    }
    [[nodiscard]] bool Undo(leon::Engine& engine, EditorContext& ctx);
    [[nodiscard]] bool Redo(leon::Engine& engine, EditorContext& ctx);
    void Clear();

private:
    static constexpr std::size_t kMaxDepth = 64;
    std::vector<std::string> undo_;
    std::vector<std::string> redo_;
    std::vector<std::string> undoNames_;
    std::vector<std::string> redoNames_;
    bool applying_ = false;

    [[nodiscard]] static std::string Snapshot(const EditorContext& ctx);
    [[nodiscard]] bool ApplySnapshot(leon::Engine& engine, EditorContext& ctx,
                                     const std::string& snapshot);
};

} // namespace leon::editor
