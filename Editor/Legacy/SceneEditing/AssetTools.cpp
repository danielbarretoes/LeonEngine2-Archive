#include <cctype>
#include <filesystem>
#include <iostream>
#include <leon/content/CookedSkeletal.h>
#include <leon/content/LeonBlueprint.h>
#include <leon/content/LeonRedirector.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/level/LeonLevelFormat.h>
#include <leon/level/Level.h>
#include <leon/level/MapBuildData.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/LeonMaterialGraph.h>
#include <leon/render/ResourceCache.h>
#include <string>
#include <vector>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::string ExtLower(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

[[nodiscard]] std::string ResolveBesideOwnerPath(const fs::path& ownerFile,
                                                 const std::string& field) {
    if (field.empty()) {
        return {};
    }
    const fs::path p(field);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    return (ownerFile.parent_path() / p).lexically_normal().generic_string();
}

/// `.lmat` stores `Parent=` and maps relative to the asset folder (parent of `Materials/`).
[[nodiscard]] std::string ResolveMaterialInstanceFieldPath(const fs::path& lmatFile,
                                                           const std::string& field) {
    if (field.empty()) {
        return {};
    }
    const fs::path p(field);
    if (p.is_absolute()) {
        return p.lexically_normal().generic_string();
    }
    const fs::path miDir = lmatFile.parent_path();
    const std::string normalized = p.generic_string();
    if (miDir.filename() == "Materials" &&
        (normalized.rfind("Materials/", 0) == 0 || normalized.rfind("Materials\\", 0) == 0)) {
        return (miDir.parent_path() / p).lexically_normal().generic_string();
    }
    return ResolveBesideOwnerPath(lmatFile, field);
}

[[nodiscard]] std::string NormalizeKey(const EditorContext& ctx, const std::string& path) {
    if (path.empty()) {
        return {};
    }
    return fs::path(MakePackRelativeAssetPath(ctx, path)).lexically_normal().generic_string();
}

[[nodiscard]] bool IsDirectoryAsset(const std::string& absPath) {
    std::error_code ec;
    return fs::is_directory(absPath, ec) && !ec;
}

[[nodiscard]] fs::path PackContentRoot(const EditorContext& ctx) {
    if (ctx.projectPath.empty()) {
        return {};
    }
    return ProjectContentDirectory(ctx.projectPath);
}

[[nodiscard]] bool RemapPathString(const EditorContext& ctx, std::string& field,
                                   const std::string& oldKey, const std::string& newKey,
                                   bool oldIsDir) {
    if (field.empty() || oldKey.empty()) {
        return false;
    }
    const std::string key = NormalizeKey(ctx, field);
    if (key.empty()) {
        return false;
    }

    std::string mapped;
    if (oldIsDir) {
        if (key == oldKey) {
            mapped = newKey;
        } else if (key.size() > oldKey.size() && key.compare(0, oldKey.size(), oldKey) == 0 &&
                   key[oldKey.size()] == '/') {
            mapped = newKey.empty() ? std::string{} : newKey + key.substr(oldKey.size());
        } else {
            return false;
        }
    } else if (key == oldKey) {
        mapped = newKey;
    } else {
        return false;
    }

    // Store content-relative when possible (empty clears the soft ref).
    field = mapped.empty() ? std::string{} : MakePackRelativeAssetPath(ctx, mapped);
    return true;
}

int RemapOpenLevel(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                   bool oldIsDir) {
    if (ctx.level == nullptr) {
        return 0;
    }
    int count = 0;
    Level& level = *ctx.level;
    for (StaticMeshComponent& mesh : level.StaticMeshes()) {
        if (RemapPathString(ctx, mesh.meshPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, mesh.materialPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        for (std::string& slot : mesh.materialPaths) {
            if (RemapPathString(ctx, slot, oldKey, newKey, oldIsDir)) {
                ++count;
            }
        }
        if (RemapPathString(ctx, mesh.lightmapPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    }
    for (BlueprintInstance& bp : level.BlueprintInstances()) {
        if (RemapPathString(ctx, bp.blueprintPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    }
    std::string env = level.EnvironmentPath();
    if (RemapPathString(ctx, env, oldKey, newKey, oldIsDir)) {
        level.SetEnvironmentPath(std::move(env));
        ++count;
    }
    if (count > 0) {
        ctx.MarkDirty();
    }
    return count;
}

int RemapLevelDocument(EditorContext& ctx, LevelDocument& doc, const std::string& oldKey,
                       const std::string& newKey, bool oldIsDir) {
    int count = 0;
    if (RemapPathString(ctx, doc.environmentPath, oldKey, newKey, oldIsDir)) {
        ++count;
    }
    for (LevelActorRecord& actor : doc.actors) {
        if (RemapPathString(ctx, actor.meshPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        if (RemapPathString(ctx, actor.materialPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
        for (std::string& slot : actor.materialPaths) {
            if (RemapPathString(ctx, slot, oldKey, newKey, oldIsDir)) {
                ++count;
            }
        }
        if (RemapPathString(ctx, actor.lightmapPath, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    }
    return count;
}

int RemapPackLevelsOnDisk(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                          bool oldIsDir, const std::string& skipAbsLevelPath) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".llev") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!skipAbsLevelPath.empty() &&
            fs::path(path).lexically_normal() == fs::path(skipAbsLevelPath).lexically_normal()) {
            // Open level is remapped in memory and saved by the user (dirty package).
            continue;
        }
        LevelDocument doc;
        if (!LoadLeonLevelFile(path, doc)) {
            continue;
        }
        const int n = RemapLevelDocument(ctx, doc, oldKey, newKey, oldIsDir);
        if (n <= 0) {
            continue;
        }
        if (!SaveLeonLevelFile(path, doc)) {
            std::cerr << "AssetTools: failed to write fixed-up level " << path << '\n';
            continue;
        }
        count += n;
    }
    return count;
}

/// Remap a path stored relative to `ownerFile` (e.g. `.lchar` / blendspace / `.lskm` refs).
[[nodiscard]] bool RemapRelBesideOwner(EditorContext& ctx, const fs::path& ownerFile,
                                       std::string& relField, const std::string& oldKey,
                                       const std::string& newKey, bool oldIsDir) {
    if (relField.empty()) {
        return false;
    }
    const fs::path abs = (ownerFile.parent_path() / relField).lexically_normal();
    std::string asPack = abs.generic_string();
    if (!RemapPathString(ctx, asPack, oldKey, newKey, oldIsDir)) {
        return false;
    }
    if (asPack.empty()) {
        relField.clear();
        return true;
    }
    const std::string resolved = ResolveContentAssetPath(ctx.projectPath, asPack);
    const fs::path target = resolved.empty() ? fs::path(asPack) : fs::path(resolved);
    std::error_code ec;
    relField = fs::relative(target, ownerFile.parent_path(), ec).generic_string();
    if (ec) {
        relField = asPack;
    }
    return true;
}

int RemapPackCharactersOnDisk(EditorContext& ctx, const std::string& oldKey,
                              const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".lchar") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        CharacterVisualDesc desc;
        if (!LoadCharacterVisualLchar(path, desc)) {
            continue;
        }
        bool changed = false;
        auto remap = [&](std::string& field) {
            if (RemapRelBesideOwner(ctx, entry.path(), field, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
        };
        remap(desc.skeletalMeshRel);
        remap(desc.blendSpaceRel);
        remap(desc.physicsAssetRel);
        remap(desc.animBlueprintRel);
        remap(desc.jumpStartAnimRel);
        remap(desc.fallLoopAnimRel);
        remap(desc.landAnimRel);
        if (!changed) {
            continue;
        }
        if (!SaveCharacterVisualLchar(path, desc)) {
            std::cerr << "AssetTools: failed to write fixed-up character " << path << '\n';
        }
    }
    return count;
}

int RemapPackBlendSpacesOnDisk(EditorContext& ctx, const std::string& oldKey,
                               const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string fname = entry.path().filename().string();
        if (fname.find(".blendspace1d.json") == std::string::npos) {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        BlendSpace1DAssetDesc desc;
        if (!LoadBlendSpace1DJson(path, desc)) {
            continue;
        }
        bool changed = false;
        for (auto& sample : desc.samples) {
            if (RemapRelBesideOwner(ctx, entry.path(), sample.animRelPath, oldKey, newKey,
                                    oldIsDir)) {
                changed = true;
                ++count;
            }
        }
        if (!changed) {
            continue;
        }
        if (!SaveBlendSpace1DJson(path, desc)) {
            std::cerr << "AssetTools: failed to write fixed-up blendspace " << path << '\n';
        }
    }
    return count;
}

int RemapPackAnimAssetsOnDisk(EditorContext& ctx, const std::string& oldKey,
                              const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string ext = ExtLower(entry.path());
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        if (ext == ".lamnt") {
            AnimMontage montage;
            if (!LoadAnimMontageJson(path, montage)) {
                continue;
            }
            if (RemapRelBesideOwner(ctx, entry.path(), montage.animSequenceRel, oldKey, newKey,
                                    oldIsDir)) {
                ++count;
                if (!SaveAnimMontageJson(path, montage)) {
                    std::cerr << "AssetTools: failed to write fixed-up montage " << path << '\n';
                }
            }
        } else if (ext == ".labp") {
            AnimBlueprintLite abp;
            if (!LoadAnimBlueprintJson(path, abp)) {
                continue;
            }
            bool changed = false;
            auto remap = [&](std::string& field) {
                if (RemapRelBesideOwner(ctx, entry.path(), field, oldKey, newKey, oldIsDir)) {
                    changed = true;
                    ++count;
                }
            };
            remap(abp.locomotionBlendSpaceRel);
            remap(abp.jumpStartAnimRel);
            remap(abp.fallLoopAnimRel);
            remap(abp.landAnimRel);
            if (changed && !SaveAnimBlueprintJson(path, abp)) {
                std::cerr << "AssetTools: failed to write fixed-up AnimBlueprint " << path << '\n';
            }
        }
    }
    return count;
}

int RemapPackSkeletalBinariesOnDisk(EditorContext& ctx, const std::string& oldKey,
                                    const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string ext = ExtLower(entry.path());
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        if (ext == ".lskm") {
            std::string assetName;
            std::string skelRel;
            std::string matRel;
            if (!PeekSkeletalMeshRefs(path, assetName, skelRel, matRel)) {
                continue;
            }
            bool changed = false;
            if (RemapRelBesideOwner(ctx, entry.path(), skelRel, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
            if (RemapRelBesideOwner(ctx, entry.path(), matRel, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
            if (changed && !RewriteSkeletalMeshRefs(path, skelRel, matRel)) {
                std::cerr << "AssetTools: failed to rewrite .lskm " << path << '\n';
            }
        } else if (ext == ".lanim") {
            std::string skelRel;
            if (!PeekAnimSequenceSkeletonRel(path, skelRel)) {
                continue;
            }
            if (RemapRelBesideOwner(ctx, entry.path(), skelRel, oldKey, newKey, oldIsDir)) {
                ++count;
                if (!RewriteAnimSequenceSkeletonRel(path, skelRel)) {
                    std::cerr << "AssetTools: failed to rewrite .lanim " << path << '\n';
                }
            }
        }
    }
    return count;
}

void RenameMapBuiltDataSidecar(const std::string& oldLevelAbs, const std::string& newLevelAbs) {
    if (oldLevelAbs.empty() || newLevelAbs.empty()) {
        return;
    }
    if (ExtLower(oldLevelAbs) != ".llev" || ExtLower(newLevelAbs) != ".llev") {
        return;
    }
    const fs::path oldBd = MapBuildData::PathBesideLevel(oldLevelAbs);
    const fs::path newBd = MapBuildData::PathBesideLevel(newLevelAbs);
    std::error_code ec;
    if (!fs::is_regular_file(oldBd, ec) || ec) {
        return;
    }
    if (fs::exists(newBd, ec) && !ec) {
        // Prefer destination name; drop the stale sidecar after overwrite.
        fs::remove(newBd, ec);
    }
    fs::rename(oldBd, newBd, ec);
    if (ec) {
        std::cerr << "AssetTools: failed to rename Map BuiltData " << oldBd.generic_string()
                  << '\n';
    }
}

void DeleteMapBuiltDataSidecar(const std::string& levelAbs) {
    if (levelAbs.empty() || ExtLower(levelAbs) != ".llev") {
        return;
    }
    const fs::path bd = MapBuildData::PathBesideLevel(levelAbs);
    std::error_code ec;
    if (fs::is_regular_file(bd, ec) && !ec) {
        fs::remove(bd, ec);
        if (ec) {
            std::cerr << "AssetTools: failed to delete Map BuiltData " << bd.generic_string()
                      << '\n';
        }
    }
}

int RemapPackBlueprintsOnDisk(EditorContext& ctx, const std::string& oldKey,
                              const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".lbp") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        BlueprintDocument doc;
        if (!LoadBlueprintDocument(path, doc)) {
            continue;
        }
        bool changed = false;
        for (BlueprintComponentDesc& component : doc.components) {
            if (RemapPathString(ctx, component.staticMesh, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
            if (RemapPathString(ctx, component.material, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
        }
        if (!changed) {
            continue;
        }
        doc.version = kLeonBlueprintDocumentVersion;
        if (!SaveBlueprintDocument(path, doc)) {
            std::cerr << "AssetTools: failed to write fixed-up blueprint " << path << '\n';
        }
    }
    return count;
}

int RemapPackMaterialsOnDisk(EditorContext& ctx, const std::string& oldKey,
                             const std::string& newKey, bool oldIsDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    std::error_code ec;
    if (!fs::exists(content, ec)) {
        return 0;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".lmat") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        // Skip rewriting the material file that is itself being moved/renamed.
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        LeonMaterialDocument doc;
        if (!LoadLeonMaterialDocument(path, doc)) {
            continue;
        }
        bool changed = false;
        if (RemapPathString(ctx, doc.baseColorMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.normalMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.emissiveMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.ormMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.opacityMaskMapPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (RemapPathString(ctx, doc.parentPath, oldKey, newKey, oldIsDir)) {
            changed = true;
            ++count;
        }
        if (!changed) {
            continue;
        }
        if (!SaveLeonMaterialDocument(path, doc)) {
            std::cerr << "AssetTools: failed to write fixed-up material " << path << '\n';
            continue;
        }
        if (ctx.resources != nullptr) {
            ctx.resources->InvalidateMaterial(path);
        }
    }
    // Material graphs store Content-relative texture keys (same convention as .lmat).
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        if (ExtLower(entry.path()) != ".lmgraph") {
            continue;
        }
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        LeonMaterialGraphDocument graph;
        if (!LoadLeonMaterialGraphDocument(path, graph)) {
            continue;
        }
        bool changed = false;
        for (MaterialExpression& expr : graph.expressions) {
            if (expr.type != EMaterialExpressionType::TextureSample) {
                continue;
            }
            if (RemapPathString(ctx, expr.texture, oldKey, newKey, oldIsDir)) {
                changed = true;
                ++count;
            }
        }
        if (changed && !SaveLeonMaterialGraphDocument(path, graph)) {
            std::cerr << "AssetTools: failed to write fixed-up Material graph " << path << '\n';
        }
    }
    return count;
}

int RemapSessionPaths(EditorContext& ctx, const std::string& oldKey, const std::string& newKey,
                      bool oldIsDir) {
    int count = 0;
    auto remapOne = [&](std::string& field) {
        if (RemapPathString(ctx, field, oldKey, newKey, oldIsDir)) {
            ++count;
        }
    };
    remapOne(ctx.previewAssetPath);
    remapOne(ctx.requestOpenMaterialPath);
    remapOne(ctx.requestOpenBlueprintPath);
    remapOne(ctx.requestOpenWidgetPath);
    remapOne(ctx.requestOpenMeshPreviewPath);
    remapOne(ctx.requestSaveAssetPath);
    remapOne(ctx.pendingMaterialPickPath);
    remapOne(ctx.pendingMeshPickPath);
    remapOne(ctx.pendingHdrPickPath);
    remapOne(ctx.contentBrowserSelectedPath);
    remapOne(ctx.placeActorsBlueprintPath);

    for (std::string& p : ctx.dirtyMaterialPaths) {
        remapOne(p);
    }
    for (std::string& p : ctx.dirtyBlueprintPaths) {
        remapOne(p);
    }
    for (std::string& p : ctx.dirtyWidgetPaths) {
        remapOne(p);
    }

    if (!ctx.levelPath.empty()) {
        const std::string levelKey = NormalizeKey(ctx, ctx.levelPath);
        if (oldIsDir) {
            if (levelKey == oldKey || (levelKey.size() > oldKey.size() &&
                                       levelKey.compare(0, oldKey.size(), oldKey) == 0 &&
                                       levelKey[oldKey.size()] == '/')) {
                std::string mapped =
                    newKey.empty()
                        ? std::string{}
                        : (levelKey == oldKey ? newKey : newKey + levelKey.substr(oldKey.size()));
                if (!mapped.empty()) {
                    const std::string resolved = ResolveContentAssetPath(ctx.projectPath, mapped);
                    ctx.levelPath = resolved.empty() ? mapped : resolved;
                } else {
                    ctx.levelPath.clear();
                }
                ++count;
            }
        } else if (levelKey == oldKey) {
            if (newKey.empty()) {
                ctx.levelPath.clear();
            } else {
                const std::string resolved = ResolveContentAssetPath(ctx.projectPath, newKey);
                ctx.levelPath = resolved.empty() ? newKey : resolved;
            }
            ++count;
        }
    }
    return count;
}

int CountInString(const EditorContext& ctx, const std::string& field, const std::string& oldKey,
                  bool oldIsDir) {
    if (field.empty() || oldKey.empty()) {
        return 0;
    }
    const std::string key = NormalizeKey(ctx, field);
    if (key.empty()) {
        return 0;
    }
    if (oldIsDir) {
        if (key == oldKey) {
            return 1;
        }
        if (key.size() > oldKey.size() && key.compare(0, oldKey.size(), oldKey) == 0 &&
            key[oldKey.size()] == '/') {
            return 1;
        }
        return 0;
    }
    return key == oldKey ? 1 : 0;
}

[[nodiscard]] bool WriteRedirectorForMove(EditorContext& ctx, const std::string& oldAbs,
                                          const std::string& newAbs, bool oldWasDir);

void FinishOp(EditorContext& ctx, const std::string& oldAbs, const std::string& newAbs,
              bool oldWasDir, AssetToolsResult& result) {
    // Map BuiltData stem follows `.llev` rename/move (Unreal MapName_BuiltData).
    if (!oldWasDir && !newAbs.empty()) {
        RenameMapBuiltDataSidecar(oldAbs, newAbs);
    }

    if (!newAbs.empty()) {
        if (!WriteRedirectorForMove(ctx, oldAbs, newAbs, oldWasDir)) {
            std::cerr << "AssetTools: failed to write redirector for " << oldAbs << '\n';
        }
        result.fixedUpReferences = 0;
    } else {
        result.fixedUpReferences = FixUpReferences(ctx, oldAbs, newAbs, oldWasDir);
    }

    if (ctx.resources != nullptr) {
        if (!oldWasDir && ExtLower(oldAbs) == ".lmat") {
            ctx.resources->InvalidateMaterial(oldAbs);
            if (!newAbs.empty()) {
                ctx.resources->InvalidateMaterial(newAbs);
            }
        }
        // Soft refs may have changed — drop GPU caches so next load sees new paths.
        ctx.resources->clear();
    }

    RebindLevelMaterials(ctx);

    ctx.requestContentRefresh = true;
    ctx.requestMaterialEditorRemapFrom = oldAbs;
    ctx.requestMaterialEditorRemapTo = newAbs;
}

[[nodiscard]] AssetToolsResult Fail(std::string error) {
    AssetToolsResult r;
    r.error = std::move(error);
    return r;
}

[[nodiscard]] bool WriteRedirectorForMove(EditorContext& ctx, const std::string& oldAbs,
                                          const std::string& newAbs, bool oldWasDir) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return false;
    }
    std::string oldKey = NormalizeKey(ctx, oldAbs);
    std::string newKey = NormalizeKey(ctx, newAbs);
    if (oldKey.empty() || newKey.empty()) {
        return false;
    }
    if (oldWasDir) {
        if (oldKey.back() != '/') {
            oldKey.push_back('/');
        }
        if (newKey.back() != '/') {
            newKey.push_back('/');
        }
    }

    const fs::path redirRel = RedirectorRelativePathForKey(oldKey);
    if (redirRel.empty()) {
        return false;
    }

    LeonRedirectorDocument doc;
    doc.source = oldKey;
    doc.target = newKey;
    doc.directory = oldWasDir;

    const fs::path redirAbs = content / redirRel;
    std::error_code ec;
    fs::create_directories(redirAbs.parent_path(), ec);
    return SaveLeonRedirector(redirAbs, doc);
}

} // namespace

void CopyMapBuiltDataBesideLevel(const std::string& oldLevelAbs, const std::string& newLevelAbs) {
    if (oldLevelAbs.empty() || newLevelAbs.empty() || oldLevelAbs == newLevelAbs) {
        return;
    }
    auto extLower = [](const std::string& p) {
        const auto e = fs::path(p).extension().string();
        std::string out = e;
        for (char& c : out) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return out;
    };
    if (extLower(oldLevelAbs) != ".llev" || extLower(newLevelAbs) != ".llev") {
        return;
    }
    const fs::path oldBd = MapBuildData::PathBesideLevel(oldLevelAbs);
    const fs::path newBd = MapBuildData::PathBesideLevel(newLevelAbs);
    std::error_code ec;
    if (!fs::is_regular_file(oldBd, ec) || ec) {
        return;
    }
    if (fs::exists(newBd, ec) && !ec) {
        fs::remove(newBd, ec);
    }
    fs::copy_file(oldBd, newBd, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "AssetTools: failed to copy Map BuiltData " << oldBd.generic_string() << '\n';
    }
}

bool IsEditablePackAsset(const EditorContext& ctx, const std::string& absPath) {
    if (ctx.projectPath.empty() || absPath.empty()) {
        return false;
    }
    if (absPath.rfind("leon:", 0) == 0) {
        return false;
    }
    fs::path relative;
    return detail::IsUnderRoot(absPath, PackContentRoot(ctx), relative);
}

int CountAssetReferences(const EditorContext& ctx, const std::string& absPath) {
    if (absPath.empty()) {
        return 0;
    }
    const std::string oldKey = NormalizeKey(ctx, absPath);
    if (oldKey.empty()) {
        return 0;
    }
    const bool oldIsDir = IsDirectoryAsset(absPath);
    int count = 0;

    if (ctx.level != nullptr) {
        for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
            count += CountInString(ctx, mesh.meshPath, oldKey, oldIsDir);
            count += CountInString(ctx, mesh.materialPath, oldKey, oldIsDir);
            for (const std::string& slot : mesh.materialPaths) {
                count += CountInString(ctx, slot, oldKey, oldIsDir);
            }
        }
        count += CountInString(ctx, ctx.level->EnvironmentPath(), oldKey, oldIsDir);
        for (const BlueprintInstance& bp : ctx.level->BlueprintInstances()) {
            count += CountInString(ctx, bp.blueprintPath, oldKey, oldIsDir);
        }
    }

    const fs::path content = PackContentRoot(ctx);
    std::error_code ec;
    if (content.empty() || !fs::exists(content, ec)) {
        return count;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string ext = ExtLower(entry.path());
        const std::string path = entry.path().generic_string();
        const std::string fname = entry.path().filename().string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        if (ext == ".llev") {
            if (ctx.level != nullptr && !ctx.levelPath.empty() &&
                fs::path(path).lexically_normal() == fs::path(ctx.levelPath).lexically_normal()) {
                continue; // already counted from open level
            }
            LevelDocument doc;
            if (!LoadLeonLevelFile(path, doc)) {
                continue;
            }
            count += CountInString(ctx, doc.environmentPath, oldKey, oldIsDir);
            for (const LevelActorRecord& actor : doc.actors) {
                count += CountInString(ctx, actor.meshPath, oldKey, oldIsDir);
                count += CountInString(ctx, actor.materialPath, oldKey, oldIsDir);
                for (const std::string& slot : actor.materialPaths) {
                    count += CountInString(ctx, slot, oldKey, oldIsDir);
                }
                count += CountInString(ctx, actor.lightmapPath, oldKey, oldIsDir);
            }
        } else if (ext == ".lbp") {
            BlueprintDocument bpDoc;
            if (!LoadBlueprintDocument(path, bpDoc)) {
                continue;
            }
            for (const BlueprintComponentDesc& component : bpDoc.components) {
                count += CountInString(ctx, component.staticMesh, oldKey, oldIsDir);
                count += CountInString(ctx, component.material, oldKey, oldIsDir);
            }
        } else if (ext == ".lmat") {
            LeonMaterialDocument doc;
            if (!LoadLeonMaterialDocument(path, doc)) {
                continue;
            }
            auto countBeside = [&](const std::string& field) {
                count += CountInString(ctx, ResolveMaterialInstanceFieldPath(entry.path(), field),
                                       oldKey, oldIsDir);
            };
            countBeside(doc.baseColorMapPath);
            countBeside(doc.normalMapPath);
            countBeside(doc.emissiveMapPath);
            countBeside(doc.ormMapPath);
            countBeside(doc.opacityMaskMapPath);
            countBeside(doc.parentPath);
        } else if (ext == ".lchar") {
            CharacterVisualDesc desc;
            if (!LoadCharacterVisualLchar(path, desc)) {
                continue;
            }
            auto countRel = [&](const std::string& rel) {
                if (rel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / rel).lexically_normal();
                count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
            };
            countRel(desc.skeletalMeshRel);
            countRel(desc.blendSpaceRel);
            countRel(desc.physicsAssetRel);
            countRel(desc.animBlueprintRel);
            countRel(desc.jumpStartAnimRel);
            countRel(desc.fallLoopAnimRel);
            countRel(desc.landAnimRel);
        } else if (fname.find(".blendspace1d.json") != std::string::npos) {
            BlendSpace1DAssetDesc desc;
            if (!LoadBlendSpace1DJson(path, desc)) {
                continue;
            }
            for (const auto& sample : desc.samples) {
                if (sample.animRelPath.empty()) {
                    continue;
                }
                const fs::path abs =
                    (entry.path().parent_path() / sample.animRelPath).lexically_normal();
                count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
            }
        } else if (ext == ".lskm") {
            std::string assetName;
            std::string skelRel;
            std::string matRel;
            if (!PeekSkeletalMeshRefs(path, assetName, skelRel, matRel)) {
                continue;
            }
            auto countRel = [&](const std::string& rel) {
                if (rel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / rel).lexically_normal();
                count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
            };
            countRel(skelRel);
            countRel(matRel);
        } else if (ext == ".lanim") {
            std::string skelRel;
            if (!PeekAnimSequenceSkeletonRel(path, skelRel) || skelRel.empty()) {
                continue;
            }
            const fs::path abs = (entry.path().parent_path() / skelRel).lexically_normal();
            count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
        } else if (ext == ".lamnt") {
            AnimMontage montage;
            if (!LoadAnimMontageJson(path, montage) || montage.animSequenceRel.empty()) {
                continue;
            }
            const fs::path abs =
                (entry.path().parent_path() / montage.animSequenceRel).lexically_normal();
            count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
        } else if (ext == ".labp") {
            AnimBlueprintLite abp;
            if (!LoadAnimBlueprintJson(path, abp)) {
                continue;
            }
            auto countRel = [&](const std::string& rel) {
                if (rel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / rel).lexically_normal();
                count += CountInString(ctx, abs.generic_string(), oldKey, oldIsDir);
            };
            countRel(abp.locomotionBlendSpaceRel);
            countRel(abp.jumpStartAnimRel);
            countRel(abp.fallLoopAnimRel);
            countRel(abp.landAnimRel);
        } else if (ext == ".lmgraph") {
            LeonMaterialGraphDocument graph;
            if (!LoadLeonMaterialGraphDocument(path, graph)) {
                continue;
            }
            for (const MaterialExpression& expr : graph.expressions) {
                if (expr.type != EMaterialExpressionType::TextureSample || expr.texture.empty()) {
                    continue;
                }
                count += CountInString(ctx, expr.texture, oldKey, oldIsDir);
            }
        }
    }
    return count;
}

std::vector<std::string> CollectAssetReferencers(const EditorContext& ctx,
                                                 const std::string& absPath, int maxResults) {
    std::vector<std::string> referencers;
    if (absPath.empty() || maxResults <= 0) {
        return referencers;
    }
    const std::string oldKey = NormalizeKey(ctx, absPath);
    if (oldKey.empty()) {
        return referencers;
    }
    const bool oldIsDir = IsDirectoryAsset(absPath);
    std::set<std::string> seen;

    auto note = [&](const std::string& field, const std::string& referencerLabel) {
        if (field.empty() || referencerLabel.empty()) {
            return;
        }
        if (CountInString(ctx, field, oldKey, oldIsDir) <= 0) {
            return;
        }
        if (seen.insert(referencerLabel).second) {
            referencers.push_back(referencerLabel);
        }
    };

    if (ctx.level != nullptr) {
        if (!ctx.levelPath.empty()) {
            const std::string label = "Open Level: " + fs::path(ctx.levelPath).filename().string();
            for (const StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
                note(mesh.meshPath, label);
                note(mesh.materialPath, label);
                for (const std::string& slot : mesh.materialPaths) {
                    note(slot, label);
                }
                note(mesh.lightmapPath, label);
            }
            note(ctx.level->EnvironmentPath(), label);
            for (const BlueprintInstance& bp : ctx.level->BlueprintInstances()) {
                note(bp.blueprintPath, label);
            }
        }
    }

    const fs::path content = PackContentRoot(ctx);
    std::error_code ec;
    if (content.empty() || !fs::exists(content, ec)) {
        return referencers;
    }
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(content, ec)) {
        if (referencers.size() >= static_cast<std::size_t>(maxResults)) {
            break;
        }
        if (ec || !entry.is_regular_file(ec)) {
            continue;
        }
        const std::string ext = ExtLower(entry.path());
        const std::string path = entry.path().generic_string();
        if (!oldIsDir && NormalizeKey(ctx, path) == oldKey) {
            continue;
        }
        const std::string packRel = MakePackRelativeAssetPath(ctx, path);
        const std::string rel =
            packRel.empty() ? fs::path(path).filename().generic_string() : packRel;
        const std::string fname = entry.path().filename().string();
        if (ext == ".llev") {
            if (ctx.level != nullptr && !ctx.levelPath.empty() &&
                fs::path(path).lexically_normal() == fs::path(ctx.levelPath).lexically_normal()) {
                continue;
            }
            LevelDocument doc;
            if (!LoadLeonLevelFile(path, doc)) {
                continue;
            }
            note(doc.environmentPath, rel);
            for (const LevelActorRecord& actor : doc.actors) {
                note(actor.meshPath, rel);
                note(actor.materialPath, rel);
                for (const std::string& slot : actor.materialPaths) {
                    note(slot, rel);
                }
                note(actor.lightmapPath, rel);
            }
        } else if (ext == ".lbp") {
            BlueprintDocument bpDoc;
            if (!LoadBlueprintDocument(path, bpDoc)) {
                continue;
            }
            for (const BlueprintComponentDesc& component : bpDoc.components) {
                note(component.staticMesh, rel);
                note(component.material, rel);
            }
        } else if (ext == ".lmat") {
            LeonMaterialDocument doc;
            if (!LoadLeonMaterialDocument(path, doc)) {
                continue;
            }
            auto noteBeside = [&](const std::string& field) {
                note(ResolveMaterialInstanceFieldPath(entry.path(), field), rel);
            };
            noteBeside(doc.baseColorMapPath);
            noteBeside(doc.normalMapPath);
            noteBeside(doc.emissiveMapPath);
            noteBeside(doc.ormMapPath);
            noteBeside(doc.opacityMaskMapPath);
            noteBeside(doc.parentPath);
        } else if (ext == ".lchar") {
            CharacterVisualDesc desc;
            if (!LoadCharacterVisualLchar(path, desc)) {
                continue;
            }
            auto noteRel = [&](const std::string& fieldRel) {
                if (fieldRel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / fieldRel).lexically_normal();
                note(abs.generic_string(), rel);
            };
            noteRel(desc.skeletalMeshRel);
            noteRel(desc.blendSpaceRel);
            noteRel(desc.physicsAssetRel);
            noteRel(desc.animBlueprintRel);
            noteRel(desc.jumpStartAnimRel);
            noteRel(desc.fallLoopAnimRel);
            noteRel(desc.landAnimRel);
        } else if (fname.find(".blendspace1d.json") != std::string::npos) {
            BlendSpace1DAssetDesc desc;
            if (!LoadBlendSpace1DJson(path, desc)) {
                continue;
            }
            for (const auto& sample : desc.samples) {
                if (sample.animRelPath.empty()) {
                    continue;
                }
                const fs::path abs =
                    (entry.path().parent_path() / sample.animRelPath).lexically_normal();
                note(abs.generic_string(), rel);
            }
        } else if (ext == ".lskm") {
            std::string assetName;
            std::string skelRel;
            std::string matRel;
            if (!PeekSkeletalMeshRefs(path, assetName, skelRel, matRel)) {
                continue;
            }
            auto noteRel = [&](const std::string& fieldRel) {
                if (fieldRel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / fieldRel).lexically_normal();
                note(abs.generic_string(), rel);
            };
            noteRel(skelRel);
            noteRel(matRel);
        } else if (ext == ".lanim") {
            std::string skelRel;
            if (!PeekAnimSequenceSkeletonRel(path, skelRel) || skelRel.empty()) {
                continue;
            }
            const fs::path abs = (entry.path().parent_path() / skelRel).lexically_normal();
            note(abs.generic_string(), rel);
        } else if (ext == ".lamnt") {
            AnimMontage montage;
            if (!LoadAnimMontageJson(path, montage) || montage.animSequenceRel.empty()) {
                continue;
            }
            const fs::path abs =
                (entry.path().parent_path() / montage.animSequenceRel).lexically_normal();
            note(abs.generic_string(), rel);
        } else if (ext == ".labp") {
            AnimBlueprintLite abp;
            if (!LoadAnimBlueprintJson(path, abp)) {
                continue;
            }
            auto noteRel = [&](const std::string& fieldRel) {
                if (fieldRel.empty()) {
                    return;
                }
                const fs::path abs = (entry.path().parent_path() / fieldRel).lexically_normal();
                note(abs.generic_string(), rel);
            };
            noteRel(abp.locomotionBlendSpaceRel);
            noteRel(abp.jumpStartAnimRel);
            noteRel(abp.fallLoopAnimRel);
            noteRel(abp.landAnimRel);
        } else if (ext == ".lmgraph") {
            LeonMaterialGraphDocument graph;
            if (!LoadLeonMaterialGraphDocument(path, graph)) {
                continue;
            }
            for (const MaterialExpression& expr : graph.expressions) {
                if (expr.type != EMaterialExpressionType::TextureSample || expr.texture.empty()) {
                    continue;
                }
                note(expr.texture, rel);
            }
        }
    }
    return referencers;
}

int FixUpReferences(EditorContext& ctx, const std::string& oldAbsOrRel,
                    const std::string& newAbsOrRel, bool oldIsDirectory) {
    // Flow: Fix Up References — immediate rewrite (used by Fix Up Redirectors and Force Delete).
    // 1. Normalize old/new content-relative keys
    // 2. Remap open level (mark dirty)
    // 3. Rewrite pack soft refs (.llev / .lmat / .lchar / blendspace / .lskm / .lanim)
    // 4. Update editor session paths
    if (oldAbsOrRel.empty()) {
        return 0;
    }
    const std::string oldKey = NormalizeKey(ctx, oldAbsOrRel);
    if (oldKey.empty()) {
        return 0;
    }
    const std::string newKey = newAbsOrRel.empty() ? std::string{} : NormalizeKey(ctx, newAbsOrRel);

    int total = 0;
    total += RemapOpenLevel(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackLevelsOnDisk(ctx, oldKey, newKey, oldIsDirectory, ctx.levelPath);
    total += RemapPackBlueprintsOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackMaterialsOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackCharactersOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackBlendSpacesOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackSkeletalBinariesOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapPackAnimAssetsOnDisk(ctx, oldKey, newKey, oldIsDirectory);
    total += RemapSessionPaths(ctx, oldKey, newKey, oldIsDirectory);
    return total;
}

int FixUpRedirectors(EditorContext& ctx) {
    const fs::path content = PackContentRoot(ctx);
    if (content.empty()) {
        return 0;
    }

    std::vector<fs::path> redirectors;
    std::error_code ec;
    for (const fs::directory_entry& entry :
         fs::recursive_directory_iterator(content, fs::directory_options::skip_permission_denied,
                                          ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec) || ec) {
            continue;
        }
        if (ExtLower(entry.path().string()) != ".lredirect") {
            continue;
        }
        redirectors.push_back(entry.path().lexically_normal());
    }

    int total = 0;
    for (const fs::path& redirAbs : redirectors) {
        LeonRedirectorDocument doc;
        if (!LoadLeonRedirector(redirAbs, doc)) {
            continue;
        }

        const std::string finalTarget = ResolveRedirectorChain(content, doc.target);
        const fs::path oldAbs = (content / doc.source).lexically_normal();
        const fs::path newAbs = (content / finalTarget).lexically_normal();

        total += FixUpReferences(ctx, oldAbs.generic_string(), newAbs.generic_string(),
                                 doc.directory);
        fs::remove(redirAbs, ec);
    }

    if (ctx.resources != nullptr) {
        ctx.resources->clear();
    }
    RebindLevelMaterials(ctx);
    ctx.requestContentRefresh = true;
    return total;
}

AssetToolsResult RenameAsset(EditorContext& ctx, const std::string& fromAbs,
                             const std::string& newName) {
    // Flow: Content Browser Rename
    // 1. Validate under pack Content
    // 2. filesystem::rename (same parent, new base name, keep extension)
    // 3. FixUpReferences(old → new)
    // 4. Session + Material Editor remap flags + refresh
    if (!IsEditablePackAsset(ctx, fromAbs)) {
        return Fail("Asset is not editable pack Content");
    }
    if (newName.empty()) {
        return Fail("Name cannot be empty");
    }
    for (char c : newName) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|') {
            return Fail("Name contains invalid characters");
        }
    }

    std::error_code ec;
    const fs::path from(fromAbs);
    if (!fs::exists(from, ec) || ec) {
        return Fail("Asset not found");
    }
    const bool isDir = fs::is_directory(from, ec);
    const fs::path dest = isDir ? (from.parent_path() / newName)
                                : (from.parent_path() / (newName + from.extension().string()));
    if (fs::exists(dest, ec)) {
        return Fail("An asset with that name already exists");
    }
    fs::rename(from, dest, ec);
    if (ec) {
        return Fail("Rename failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    FinishOp(ctx, fromAbs, dest.generic_string(), isDir, result);
    return result;
}

AssetToolsResult MoveAsset(EditorContext& ctx, const std::string& fromAbs,
                           const std::string& destFolderAbs) {
    // Flow: Content Browser Move
    // 1. Validate source + dest under pack Content
    // 2. filesystem::rename into dest folder
    // 3. FixUpReferences(old → new)
    // 4. Session + refresh
    if (!IsEditablePackAsset(ctx, fromAbs) || !IsEditablePackAsset(ctx, destFolderAbs)) {
        return Fail("Move must stay inside pack Content");
    }
    std::error_code ec;
    const fs::path from(fromAbs);
    const fs::path destDir(destFolderAbs);
    if (!fs::exists(from, ec) || ec) {
        return Fail("Asset not found");
    }
    if (!fs::is_directory(destDir, ec) || ec) {
        return Fail("Destination is not a folder");
    }
    const fs::path dest = destDir / from.filename();
    if (fs::weakly_canonical(from.parent_path(), ec) == fs::weakly_canonical(destDir, ec)) {
        AssetToolsResult noop;
        noop.ok = true;
        return noop;
    }
    // Reject moving a folder into itself.
    if (fs::is_directory(from, ec)) {
        const fs::path canonFrom = fs::weakly_canonical(from, ec);
        const fs::path canonDest = fs::weakly_canonical(destDir, ec);
        const fs::path rel = canonDest.lexically_relative(canonFrom);
        if (!rel.empty() && rel.generic_string().find("..") == std::string::npos) {
            return Fail("Cannot move a folder into itself");
        }
    }
    if (fs::exists(dest, ec)) {
        return Fail("An asset with that name already exists in the destination");
    }
    const bool isDir = fs::is_directory(from, ec);
    fs::rename(from, dest, ec);
    if (ec) {
        return Fail("Move failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    FinishOp(ctx, fromAbs, dest.generic_string(), isDir, result);
    return result;
}

AssetToolsResult DuplicateAsset(EditorContext& ctx, const std::string& fromAbs) {
    // Flow: Content Browser Duplicate
    // 1. Validate under pack Content
    // 2. copy (file or recursive folder) with unique `_Copy` suffix beside source
    // 3. Copy `.leonimport` sidecar and Map BuiltData for `.llev`
    if (!IsEditablePackAsset(ctx, fromAbs)) {
        return Fail("Asset is not editable pack Content");
    }
    std::error_code ec;
    const fs::path from(fromAbs);
    if (!fs::exists(from, ec) || ec) {
        return Fail("Asset not found");
    }

    const fs::path parent = from.parent_path();
    const bool isDir = fs::is_directory(from, ec);
    const std::string baseName = from.filename().string();
    const std::string stem = isDir ? baseName : from.stem().string();
    const std::string ext = isDir ? std::string{} : from.extension().string();

    fs::path dest;
    for (int attempt = 1; attempt < 1000; ++attempt) {
        const std::string suffix = attempt == 1 ? "_Copy" : "_Copy" + std::to_string(attempt);
        dest = isDir ? (parent / (stem + suffix)) : (parent / (stem + suffix + ext));
        if (!fs::exists(dest, ec)) {
            break;
        }
        if (attempt == 999) {
            return Fail("Could not find a unique duplicate name");
        }
    }

    if (isDir) {
        fs::copy(from, dest, fs::copy_options::recursive, ec);
    } else {
        fs::copy_file(from, dest, fs::copy_options::overwrite_existing, ec);
    }
    if (ec) {
        return Fail("Duplicate failed: " + ec.message());
    }

    if (!isDir) {
        const fs::path sidecarFrom = parent / (stem + ".leonimport");
        if (fs::is_regular_file(sidecarFrom, ec) && !ec) {
            const fs::path sidecarDest = parent / (dest.stem().string() + ".leonimport");
            fs::copy_file(sidecarFrom, sidecarDest, fs::copy_options::overwrite_existing, ec);
        }

        if (ExtLower(fromAbs) == ".llev") {
            CopyMapBuiltDataBesideLevel(fromAbs, dest.generic_string());
        }
    }

    AssetToolsResult result;
    result.ok = true;
    ctx.requestContentRefresh = true;
    return result;
}

AssetToolsResult DeleteAssets(EditorContext& ctx, const std::string& pathAbs) {
    // Flow: Content Browser Delete Assets
    // 1. Validate under pack Content
    // 2. Count was shown in UI; Force Delete clears refs
    // 3. filesystem::remove_all
    // 4. FixUpReferences(old → empty)
    if (!IsEditablePackAsset(ctx, pathAbs)) {
        return Fail("Asset is not editable pack Content");
    }
    std::error_code ec;
    const fs::path path(pathAbs);
    if (!fs::exists(path, ec) || ec) {
        return Fail("Asset not found");
    }
    const bool isDir = fs::is_directory(path, ec);
    // Capture key before delete (path may vanish).
    const std::string oldAbs = path.generic_string();
    if (!isDir && ExtLower(oldAbs) == ".llev") {
        DeleteMapBuiltDataSidecar(oldAbs);
    }
    if (!isDir) {
        const fs::path sidecar = fs::path(oldAbs).replace_extension(".leonimport");
        std::error_code sideEc;
        if (fs::is_regular_file(sidecar, sideEc) && !sideEc) {
            fs::remove(sidecar, sideEc);
        }
        if (ExtLower(oldAbs) == ".lmesh") {
            const fs::path materialsDir = path.parent_path() / "Materials";
            if (fs::is_directory(materialsDir, sideEc) && !sideEc) {
                fs::remove_all(materialsDir, sideEc);
            }
        }
    }

    fs::remove_all(path, ec);
    if (ec) {
        return Fail("Delete failed: " + ec.message());
    }

    AssetToolsResult result;
    result.ok = true;
    result.fixedUpReferences = FixUpReferences(ctx, oldAbs, {}, isDir);
    if (ctx.resources != nullptr) {
        if (isDir) {
            ctx.resources->clear();
        } else {
            const std::string ext = ExtLower(oldAbs);
            if (ext == ".lmat" || ext == ".lmgraph") {
                ctx.resources->InvalidateMaterial(oldAbs);
            } else if (ext == ".lmesh") {
                ctx.resources->InvalidateStaticMesh(oldAbs);
            } else if (ext == ".ltx") {
                ctx.resources->InvalidateTexture(oldAbs);
            }
        }
    }
    ctx.requestContentRefresh = true;
    ctx.requestMaterialEditorRemapFrom = oldAbs;
    ctx.requestMaterialEditorRemapTo.clear();
    if (ctx.contentBrowserSelectedPath == oldAbs) {
        ctx.contentBrowserSelectedPath.clear();
    }
    RebindLevelMaterials(ctx);
    return result;
}

void RebindLevelMaterials(EditorContext& ctx) {
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        return;
    }
    for (StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
        if (!mesh.materialPaths.empty()) {
            mesh.materialOverride = false;
            const std::size_t n = mesh.mesh != nullptr && mesh.mesh->HasMaterials()
                                      ? mesh.mesh->Materials().size()
                                      : mesh.materialPaths.size();
            mesh.materials.resize(n);
            for (std::size_t i = 0; i < n; ++i) {
                if (i < mesh.materialPaths.size() && !mesh.materialPaths[i].empty()) {
                    const std::string resolved = ResolveAssetPath(mesh.materialPaths[i]);
                    mesh.materials[i] = ctx.resources->LoadMaterial(
                        resolved.empty() ? mesh.materialPaths[i] : resolved);
                } else if (mesh.mesh != nullptr && i < mesh.mesh->Materials().size()) {
                    mesh.materials[i] = mesh.mesh->Materials()[i];
                } else {
                    mesh.materials[i] = ctx.resources->DefaultMaterial();
                }
            }
            continue;
        }
        if (mesh.materialPath.empty()) {
            // Force Delete / Fix Up cleared the soft ref — drop stale POD.
            mesh.material = ctx.resources->DefaultMaterial();
            mesh.materialOverride = false;
            mesh.materials.clear();
            continue;
        }
        const std::string resolved = ResolveAssetPath(mesh.materialPath);
        mesh.material =
            ctx.resources->LoadMaterial(resolved.empty() ? mesh.materialPath : resolved);
    }
}

} // namespace leon::editor
