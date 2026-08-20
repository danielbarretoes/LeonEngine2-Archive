#pragma once

#include <cstddef>
#include <cstdint>
#include <leon/editor/EditorContext.h>
#include <leon/editor/EditorSelection.h>
#include <string>

namespace leon::editor {

/// World Outliner: Level StaticMeshes / Lights / PlayerStarts / volumes tree + Delete key.
class OutlinerPanel {
public:
    void Draw(EditorContext& ctx);

private:
    void HandleDelete(EditorContext& ctx);
    void HandleRenameHotkey(EditorContext& ctx);
    void BeginRename(EditorContext& ctx, EEditorSelectionKind kind, std::size_t index,
                     const std::string& currentLabel);
    [[nodiscard]] bool ApplyRename(EditorContext& ctx);
    void CancelRename();

    char searchBuf_[128]{};
    bool renaming_ = false;
    EEditorSelectionKind renameKind_ = EEditorSelectionKind::None;
    std::size_t renameIndex_ = 0;
    char renameBuf_[128]{};
    bool renameFocusPending_ = false;
};

} // namespace leon::editor
