#pragma once

#include <glm/vec3.hpp>

#include <leon/core/Transform.h>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorHistory.h>
#include <leon/editor/EditorSelection.h>
#include <string>
#include <vector>

namespace leon::editor {

/// Clipboard entry for editor copy/paste of level objects.
struct EditorClipboardItem {
    EEditorSelectionKind kind = EEditorSelectionKind::None;
    /// Serialized actor/light JSON object (schema fragment).
    std::string json;
};

/// Shared edit ops: delete, duplicate, copy/paste (works with multi-select).
class EditorCommands {
public:
    static void DeleteSelection(EditorContext& ctx, EditorHistory* history);
    static void DuplicateSelection(EditorContext& ctx, EditorHistory* history);
    static void CopySelection(EditorContext& ctx);
    static void PasteClipboard(EditorContext& ctx, EditorHistory* history);
    [[nodiscard]] static bool HasClipboard() { return !Clipboard().empty(); }

    /// Snap a world position to the editor grid.
    [[nodiscard]] static glm::vec3 SnapPosition(const glm::vec3& p, float gridSize);
    [[nodiscard]] static float SnapAngle(float degrees, float stepDegrees);
    static void SnapTransform(leon::Transform& t, float gridSize, float angleStep, bool snapScale);

    /// Place Actors / Modes: spawn one actor at `worldPos` from the active kind.
    [[nodiscard]] static bool PlaceEditorActor(EditorContext& ctx, EEditorPlaceActorsKind kind,
                                               const glm::vec3& worldPos,
                                               const std::string& blueprintPath = {});

    /// Record a successful place for the Place Actors → Recent list.
    static void PushPlaceActorsRecent(EditorContext& ctx, EEditorPlaceActorsKind kind,
                                      const std::string& blueprintPath, const std::string& label);

    [[nodiscard]] static bool IsEditorLocked(const EditorContext& ctx, EEditorSelectionKind kind,
                                             std::size_t index);
    [[nodiscard]] static bool* EditorLockedPtr(EditorContext& ctx, EEditorSelectionKind kind,
                                               std::size_t index);
    [[nodiscard]] static bool* HiddenPtr(EditorContext& ctx, EEditorSelectionKind kind,
                                         std::size_t index);

    /// Mutable actor label / tag storage for Outliner rename (nullptr if kind is not renamable).
    [[nodiscard]] static std::string* ActorLabelPtr(EditorContext& ctx, EEditorSelectionKind kind,
                                                    std::size_t index);
    [[nodiscard]] static bool CanRenameSelection(const EditorContext& ctx);

    /// Soft path for Browse to Asset (mesh / material / blueprint). Empty if N/A.
    [[nodiscard]] static std::string BrowsePathForSelection(const EditorContext& ctx,
                                                            const EditorSelection& sel);

    /// Unreal-like selection RMB items. Returns true when Rename was chosen (caller BeginRename).
    [[nodiscard]] static bool DrawSelectionContextMenuItems(EditorContext& ctx);

private:
    static std::vector<EditorClipboardItem>& Clipboard();
};

} // namespace leon::editor
