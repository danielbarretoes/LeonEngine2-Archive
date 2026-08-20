#include <iostream>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/LevelSaver.h>

namespace leon::editor {

std::string EditorHistory::Snapshot(const EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.camera == nullptr) {
        return {};
    }
    return SerializeLevelSnapshot(*ctx.level, *ctx.camera);
}

bool EditorHistory::ApplySnapshot(Engine& engine, EditorContext& ctx, const std::string& snapshot) {
    if (snapshot.empty()) {
        return false;
    }
    // Reuse the level path so pack-relative materials and lightmaps still resolve.
    const std::string sourcePath = ctx.levelPath.empty() ? std::string("undo/redo") : ctx.levelPath;
    applying_ = true;
    const bool ok = LoadLevelSnapshot(engine, snapshot, sourcePath);
    applying_ = false;
    if (!ok) {
        return false;
    }
    ctx.level = &engine.GetLevel();
    ctx.camera = &engine.GetCamera();
    ctx.ClearSelection();
    ctx.dirty = true;
    ctx.requestContentRefresh = true;
    return true;
}

bool EditorHistory::Capture(const EditorContext& ctx, std::string_view transactionName) {
    if (applying_) {
        return false;
    }
    const std::string snap = Snapshot(ctx);
    if (snap.empty()) {
        return false;
    }
    if (!undo_.empty() && undo_.back() == snap) {
        return false;
    }
    undo_.push_back(snap);
    undoNames_.push_back(transactionName.empty() ? std::string{"Edit"}
                                                 : std::string{transactionName});
    if (undo_.size() > kMaxDepth) {
        undo_.erase(undo_.begin());
        undoNames_.erase(undoNames_.begin());
    }
    redo_.clear();
    redoNames_.clear();
    return true;
}

void EditorHistory::DiscardLastCapture() {
    if (applying_ || undo_.empty()) {
        return;
    }
    undo_.pop_back();
    if (!undoNames_.empty()) {
        undoNames_.pop_back();
    }
}

bool EditorHistory::Undo(Engine& engine, EditorContext& ctx) {
    if (undo_.empty()) {
        return false;
    }
    const std::string current = Snapshot(ctx);
    const std::string prev = undo_.back();
    // Apply before mutating stacks so a failed restore leaves history intact.
    if (!ApplySnapshot(engine, ctx, prev)) {
        std::cerr << "EditorHistory: undo apply failed\n";
        return false;
    }
    undo_.pop_back();
    std::string name = "Edit";
    if (!undoNames_.empty()) {
        name = undoNames_.back();
        undoNames_.pop_back();
    }
    if (!current.empty()) {
        redo_.push_back(current);
        redoNames_.push_back(std::move(name));
    }
    return true;
}

bool EditorHistory::Redo(Engine& engine, EditorContext& ctx) {
    if (redo_.empty()) {
        return false;
    }
    const std::string current = Snapshot(ctx);
    const std::string next = redo_.back();
    if (!ApplySnapshot(engine, ctx, next)) {
        std::cerr << "EditorHistory: redo apply failed\n";
        return false;
    }
    redo_.pop_back();
    std::string name = "Edit";
    if (!redoNames_.empty()) {
        name = redoNames_.back();
        redoNames_.pop_back();
    }
    if (!current.empty()) {
        undo_.push_back(current);
        undoNames_.push_back(std::move(name));
    }
    return true;
}

void EditorHistory::Clear() {
    undo_.clear();
    redo_.clear();
    undoNames_.clear();
    redoNames_.clear();
}

} // namespace leon::editor
