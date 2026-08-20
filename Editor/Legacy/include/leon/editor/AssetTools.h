#pragma once

#include <algorithm>
#include <leon/editor/EditorContext.h>
#include <set>
#include <string>

namespace leon::editor {

/// Result of a Content Browser asset operation (Unreal Asset Tools–like).
struct AssetToolsResult {
    bool ok = false;
    int fixedUpReferences = 0;
    std::string error;
};

/// True when `absPath` is a writable asset under the open pack's Content/ (not Engine).
[[nodiscard]] bool IsEditablePackAsset(const EditorContext& ctx, const std::string& absPath);

/// Count soft references to `absPath` (or folder prefix) in the open level + pack `.llev`/`.lmat`.
[[nodiscard]] int CountAssetReferences(const EditorContext& ctx, const std::string& absPath);

/// Human-readable referencer paths for delete modal (cap `maxResults`, trailing "…" if truncated).
[[nodiscard]] std::vector<std::string>
CollectAssetReferencers(const EditorContext& ctx, const std::string& absPath, int maxResults = 20);

/// Rewrite soft refs after a path change. Empty `newAbsOrRel` clears matching refs (Force Delete).
/// `oldIsDirectory` must reflect the source before a delete (path may already be gone).
[[nodiscard]] int FixUpReferences(EditorContext& ctx, const std::string& oldAbsOrRel,
                                  const std::string& newAbsOrRel, bool oldIsDirectory);

/// Resolve all `.lredirect` assets: rewrite referencers to final targets and delete redirectors.
[[nodiscard]] int FixUpRedirectors(EditorContext& ctx);

/// Flow: Content Browser Rename — same folder, new base name (extension preserved).
[[nodiscard]] AssetToolsResult RenameAsset(EditorContext& ctx, const std::string& fromAbs,
                                           const std::string& newName);

/// Flow: Content Browser Move — drag asset onto a Content folder.
[[nodiscard]] AssetToolsResult MoveAsset(EditorContext& ctx, const std::string& fromAbs,
                                         const std::string& destFolderAbs);

/// Flow: Content Browser Duplicate — copy asset beside source with `_Copy` suffix.
[[nodiscard]] AssetToolsResult DuplicateAsset(EditorContext& ctx, const std::string& fromAbs);

/// Flow: Content Browser Delete Assets — remove file/folder and fix up (clear) references.
[[nodiscard]] AssetToolsResult DeleteAssets(EditorContext& ctx, const std::string& pathAbs);

/// Copy `<stem>_BuiltData.lmb` when Save As to a new level path (keeps the original sidecar).
void CopyMapBuiltDataBesideLevel(const std::string& oldLevelAbs, const std::string& newLevelAbs);

/// Reload `StaticMeshComponent::material` from `materialPath` after Invalidate / Fix Up.
void RebindLevelMaterials(EditorContext& ctx);

} // namespace leon::editor
