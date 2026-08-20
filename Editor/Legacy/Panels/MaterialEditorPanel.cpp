#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <leon/core/FileIO.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorDisplayNames.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorFileDialog.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/MaterialEditorPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/level/Level.h>
#include <leon/render/LeonMaterialGraph.h>
#include <leon/render/PostProcess.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <memory>
#include <sstream>

namespace leon::editor {
namespace {

[[nodiscard]] const char* ExpressionTypeLabel(EMaterialExpressionType t) {
    return MaterialExpressionTypeLabel(t);
}

[[nodiscard]] int NextExpressionId(const LeonMaterialGraphDocument& g) {
    int maxId = 0;
    for (const MaterialExpression& e : g.expressions) {
        maxId = std::max(maxId, e.id);
    }
    return maxId + 1;
}

[[nodiscard]] bool LooksLikeTexturePath(const std::string& path) {
    namespace fs = std::filesystem;
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".ltx" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
           ext == ".bmp" || ext == ".exr";
}

} // namespace

void MaterialEditorPanel::RefreshTextureList(EditorContext& ctx) {
    if (ctx.requestContentRefresh) {
        texturesProjectKey_.clear();
    }
    const std::string key = ctx.projectPath;
    if (!texturesProjectKey_.empty() && texturesProjectKey_ == key && !textures_.empty()) {
        return;
    }
    texturesProjectKey_ = key;
    textures_ = CollectEditorTextures(ctx.projectPath);
}

MaterialEditorPanel::Doc* MaterialEditorPanel::FindDoc(const std::string& path) {
    for (Doc& d : docs_) {
        if (PathsEqualNormalized(d.path, path)) {
            return &d;
        }
    }
    return nullptr;
}

const MaterialEditorPanel::Doc* MaterialEditorPanel::FindDoc(const std::string& path) const {
    for (const Doc& d : docs_) {
        if (PathsEqualNormalized(d.path, path)) {
            return &d;
        }
    }
    return nullptr;
}

bool MaterialEditorPanel::HasDirtyDocs() const {
    for (const Doc& d : docs_) {
        if (d.open && d.dirty) {
            return true;
        }
    }
    return false;
}

bool MaterialEditorPanel::IsPathDirty(const std::string& path) const {
    const Doc* doc = FindDoc(path);
    return doc != nullptr && doc->open && doc->dirty;
}

bool MaterialEditorPanel::HasFocusedDirtyDoc() const {
    if (focusedPath_.empty()) {
        return false;
    }
    return IsPathDirty(focusedPath_);
}

bool MaterialEditorPanel::SaveFocused(EditorContext& ctx) {
    if (focusedPath_.empty()) {
        return false;
    }
    return SavePath(ctx, focusedPath_);
}

bool MaterialEditorPanel::SavePath(EditorContext& ctx, const std::string& path) {
    Doc* doc = FindDoc(path);
    if (doc == nullptr || !doc->open) {
        return false;
    }
    if (!doc->dirty) {
        return true;
    }
    return SaveDoc(ctx, *doc);
}

bool MaterialEditorPanel::SaveAll(EditorContext& ctx) {
    bool ok = true;
    bool any = false;
    for (Doc& doc : docs_) {
        if (!doc.open || !doc.dirty) {
            continue;
        }
        any = true;
        ok = SaveDoc(ctx, doc) && ok;
    }
    return any ? ok : true;
}

void MaterialEditorPanel::SyncDirtyPaths(EditorContext& ctx) const {
    ctx.dirtyMaterialPaths.clear();
    for (const Doc& d : docs_) {
        if (d.open && d.dirty) {
            ctx.dirtyMaterialPaths.push_back(d.path);
        }
    }
}

void MaterialEditorPanel::RemapAssetPath(EditorContext& ctx, const std::string& fromAbs,
                                         const std::string& toAbs) {
    if (fromAbs.empty()) {
        return;
    }

    auto pathMatchesFrom = [&](const std::string& candidate) {
        if (candidate.empty()) {
            return false;
        }
        if (PathsEqualNormalized(candidate, fromAbs)) {
            return true;
        }
        const std::string resolved = ResolveAssetPath(candidate);
        return !resolved.empty() && PathsEqualNormalized(resolved, fromAbs);
    };

    const std::string toRel = toAbs.empty() ? std::string{} : MakePackRelativeAssetPath(ctx, toAbs);

    for (Doc& doc : docs_) {
        if (PathsEqualNormalized(doc.path, fromAbs)) {
            if (toAbs.empty()) {
                doc.open = false;
                if (PathsEqualNormalized(focusedPath_, doc.path)) {
                    focusedPath_.clear();
                }
            } else {
                doc.path = NormalizeAssetPathAbs(toAbs);
                if (PathsEqualNormalized(focusedPath_, fromAbs) ||
                    PathsEqualNormalized(focusedPath_, doc.path)) {
                    focusedPath_ = doc.path;
                }
            }
        }

        if (doc.mode != EMode::Instance) {
            continue;
        }
        if (!pathMatchesFrom(doc.instance.parentPath)) {
            continue;
        }
        if (toAbs.empty()) {
            doc.instance.parentPath.clear();
        } else {
            doc.instance.parentPath = toRel.empty() ? toAbs : toRel;
        }
        (void)std::snprintf(doc.parentBuf, sizeof(doc.parentBuf), "%s",
                            doc.instance.parentPath.c_str());
        doc.resolvedDirty = true;
        doc.dirty = true;
    }
}

void MaterialEditorPanel::SyncBuffersFromDoc(Doc& doc) {
    if (doc.mode == EMode::Graph) {
        (void)std::snprintf(doc.nameBuf, sizeof(doc.nameBuf), "%s", doc.graph.name.c_str());
        return;
    }
    (void)std::snprintf(doc.nameBuf, sizeof(doc.nameBuf), "%s", doc.instance.name.c_str());
    (void)std::snprintf(doc.parentBuf, sizeof(doc.parentBuf), "%s",
                        doc.instance.parentPath.c_str());
    const LeonMaterialDocument& src = doc.instance.parentPath.empty() ? doc.instance : doc.resolved;
    (void)std::snprintf(
        doc.baseMapBuf, sizeof(doc.baseMapBuf), "%s",
        (doc.instance.overrideBaseColorMap ? doc.instance.baseColorMapPath : src.baseColorMapPath)
            .c_str());
    (void)std::snprintf(
        doc.normalMapBuf, sizeof(doc.normalMapBuf), "%s",
        (doc.instance.overrideNormalMap ? doc.instance.normalMapPath : src.normalMapPath).c_str());
    (void)std::snprintf(
        doc.emissiveMapBuf, sizeof(doc.emissiveMapBuf), "%s",
        (doc.instance.overrideEmissiveMap ? doc.instance.emissiveMapPath : src.emissiveMapPath)
            .c_str());
    (void)std::snprintf(
        doc.ormMapBuf, sizeof(doc.ormMapBuf), "%s",
        (doc.instance.overrideOrmMap ? doc.instance.ormMapPath : src.ormMapPath).c_str());
    (void)std::snprintf(doc.opacityMaskBuf, sizeof(doc.opacityMaskBuf), "%s",
                        (doc.instance.overrideOpacityMaskMap ? doc.instance.opacityMaskMapPath
                                                             : src.opacityMaskMapPath)
                            .c_str());
}

void MaterialEditorPanel::SyncDocFromBuffers(Doc& doc) {
    if (doc.mode == EMode::Graph) {
        doc.graph.name = doc.nameBuf;
        return;
    }
    doc.instance.name = doc.nameBuf;
    doc.instance.parentPath = doc.parentBuf;
    if (doc.instance.overrideBaseColorMap) {
        doc.instance.baseColorMapPath = doc.baseMapBuf;
    }
    if (doc.instance.overrideNormalMap) {
        doc.instance.normalMapPath = doc.normalMapBuf;
    }
    if (doc.instance.overrideEmissiveMap) {
        doc.instance.emissiveMapPath = doc.emissiveMapBuf;
    }
    if (doc.instance.overrideOrmMap) {
        doc.instance.ormMapPath = doc.ormMapBuf;
    }
    if (doc.instance.overrideOpacityMaskMap) {
        doc.instance.opacityMaskMapPath = doc.opacityMaskBuf;
    }
}

void MaterialEditorPanel::RefreshResolved(Doc& doc) {
    if (doc.mode != EMode::Instance) {
        return;
    }
    SyncDocFromBuffers(doc);
    if (!ResolveLeonMaterialDocumentInMemory(doc.path, doc.instance, doc.resolved)) {
        doc.resolved = doc.instance;
    }
    doc.resolvedDirty = false;
}

void MaterialEditorPanel::OpenMaterial(const std::string& path) {
    const std::string norm = NormalizeAssetPathAbs(path);
    if (norm.empty() || !IsLeonMaterialAssetPath(norm)) {
        EditorLogWarn("Material Editor: not a .lmat / .lmgraph — " + path);
        return;
    }
    if (Doc* existing = FindDoc(norm)) {
        existing->open = true;
        existing->focusOnce = true;
        return;
    }

    Doc doc;
    doc.path = norm;
    doc.dirty = false;
    doc.open = true;
    doc.focusOnce = true;

    if (IsLeonMaterialGraphPath(norm)) {
        doc.mode = EMode::Graph;
        if (!LoadLeonMaterialGraphDocument(norm, doc.graph)) {
            EditorLogError("Material Editor: failed to load Material " + norm);
            return;
        }
    } else {
        doc.mode = EMode::Instance;
        if (!LoadLeonMaterialDocument(norm, doc.instance)) {
            EditorLogError("Material Editor: failed to load Material Instance " + norm);
            return;
        }
        // Fill parentBuf BEFORE RefreshResolved — otherwise SyncDocFromBuffers copies an empty
        // parentBuf over instance.parentPath and strips Parent= on every open.
        SyncBuffersFromDoc(doc);
        doc.resolvedDirty = true;
        RefreshResolved(doc);
    }
    SyncBuffersFromDoc(doc);
    docs_.push_back(std::move(doc));
    EditorLogInfo("Material Editor: opened " + norm);
}

void MaterialEditorPanel::CloseDoc(const std::string& path) {
    if (Doc* doc = FindDoc(path)) {
        doc->open = false;
        if (PathsEqualNormalized(focusedPath_, path)) {
            focusedPath_.clear();
        }
    }
}

bool MaterialEditorPanel::SaveDoc(EditorContext& ctx, Doc& doc) {
    SyncDocFromBuffers(doc);
    if (doc.mode == EMode::Instance && doc.instance.parentPath.empty()) {
        EditorLogWarn("Material Instance has empty Parent — save will not inherit a Material "
                      "graph. Set Parent to a .lmgraph (e.g. Materials/M_Grass.lmgraph).");
        EditorToast("Parent is empty — instance will not inherit the Material.",
                    EEditorToastKind::Warning, 4.0f);
    }
    bool ok = false;
    if (doc.mode == EMode::Graph) {
        ok = SaveLeonMaterialGraphDocument(doc.path, doc.graph);
    } else {
        ok = SaveLeonMaterialDocument(doc.path, doc.instance);
    }
    if (!ok) {
        EditorLogError("Material Editor: save failed " + doc.path);
        return false;
    }
    doc.dirty = false;
    if (ctx.resources != nullptr) {
        ctx.resources->InvalidateMaterial(doc.path);
        // Parent edits: drop all MI caches so Parent recompile is picked up.
        if (doc.mode == EMode::Graph || !doc.instance.parentPath.empty()) {
            ctx.resources->InvalidateAllMaterials();
        }
    }
    RebindLevelMaterials(ctx);
    ctx.requestContentRefresh = true;
    EditorLogInfo("Material Editor: saved " + doc.path);
    return true;
}

void MaterialEditorPanel::ApplyToSelection(EditorContext& ctx, Doc& doc) {
    if (ctx.level == nullptr || ctx.resources == nullptr) {
        return;
    }
    if (doc.mode != EMode::Instance) {
        EditorLogWarn("Material Editor: Apply to Selection requires a Material Instance (.lmat)");
        return;
    }
    if (ctx.selection.kind != EEditorSelectionKind::StaticMesh ||
        ctx.selection.index >= ctx.level->StaticMeshes().size()) {
        EditorLogWarn("Material Editor: select a StaticMesh to apply");
        return;
    }
    if (doc.dirty) {
        (void)SaveDoc(ctx, doc);
    }
    StaticMeshComponent& mesh = ctx.level->StaticMeshes()[ctx.selection.index];
    const std::string rel = MakePackRelativeAssetPath(ctx, doc.path);
    const std::string loadPath = ResolveAssetPath(rel.empty() ? doc.path : rel);
    mesh.material = ctx.resources->LoadMaterial(loadPath.empty() ? doc.path : loadPath);
    mesh.materialOverride = true;
    mesh.materialPath = rel.empty() ? doc.path : rel;
    ctx.MarkDirty();
    EditorLogInfo("Applied material to selection");
}

void MaterialEditorPanel::CreateInstanceFromDoc(EditorContext& ctx, Doc& doc) {
    namespace fs = std::filesystem;
    const fs::path parentPath(doc.path);
    const std::string stem = parentPath.stem().string();
    const std::string suggested = SuggestMaterialInstanceName(stem);
    const fs::path outPath = parentPath.parent_path() / (suggested + ".lmat");

    std::string parentRel = doc.path;
    if (!ctx.projectPath.empty()) {
        const std::string packRel = MakePackRelativeAssetPath(ctx, doc.path);
        if (!packRel.empty()) {
            parentRel = packRel;
        }
    }

    const std::string text = MakeMaterialInstanceFromParentText(suggested, parentRel);
    if (!WriteTextFileAtomic(outPath.generic_string(), text)) {
        EditorLogError("Failed to create Material Instance: " + outPath.generic_string());
        return;
    }
    ctx.requestContentRefresh = true;
    ctx.requestOpenMaterialPath = outPath.generic_string();
    EditorLogInfo("Created Material Instance " + outPath.generic_string());
}

void MaterialEditorPanel::DrawInstanceDoc(EditorContext& ctx, Doc& doc) {
    bool refreshPreview = false;
    if (doc.resolvedDirty) {
        RefreshResolved(doc);
        SyncBuffersFromDoc(doc);
        refreshPreview = true;
    }

    ui::TextTitle("Material Instance");
    ui::TextMuted("Overrides Parent parameters (sparse .lmat)");
    if (doc.instance.parentPath.empty() && doc.parentBuf[0] == '\0') {
        ui::TextError("No Parent — set Parent to a .lmgraph or this is a standalone .lmat.");
    }

    {
        ui::UiButtonDesc save;
        save.label = "Save Asset";
        save.icon = ui::UiIcon(ELucideIcon::Save);
        save.variant = ui::EUiVariant::Primary;
        save.size = ui::EUiSize::Sm;
        if (ui::Button("##mi_save", save)) {
            (void)SaveDoc(ctx, doc);
        }
    }
    ImGui::SameLine();
    if (ui::Button("##mi_apply", "Apply to Selection", ui::EUiVariant::Secondary,
                   ui::EUiSize::Sm)) {
        ApplyToSelection(ctx, doc);
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc revert;
        revert.label = "Revert";
        revert.variant = ui::EUiVariant::Ghost;
        revert.size = ui::EUiSize::Sm;
        revert.disabled = !doc.dirty;
        if (ui::Button("##mi_revert", revert) && doc.dirty) {
            LeonMaterialDocument data;
            if (LoadLeonMaterialDocument(doc.path, data)) {
                doc.instance = std::move(data);
                doc.resolvedDirty = true;
                RefreshResolved(doc);
                SyncBuffersFromDoc(doc);
                doc.dirty = false;
            }
        }
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc resetAll;
        resetAll.label = "Reset All Overrides";
        resetAll.variant = ui::EUiVariant::Warning;
        resetAll.size = ui::EUiSize::Sm;
        resetAll.disabled = doc.instance.parentPath.empty();
        if (ui::Button("##mi_reset_all", resetAll) && !doc.instance.parentPath.empty()) {
            LeonMaterialDocument cleared;
            cleared.name = doc.instance.name;
            cleared.parentPath = doc.instance.parentPath;
            doc.instance = std::move(cleared);
            doc.resolvedDirty = true;
            RefreshResolved(doc);
            SyncBuffersFromDoc(doc);
            doc.dirty = true;
        }
    }

    if (ui::InputText("##mat_name", "Name", doc.nameBuf, sizeof(doc.nameBuf))) {
        doc.dirty = true;
    }

    if (ui::InputText("##mat_parent", "Parent", doc.parentBuf, sizeof(doc.parentBuf))) {
        doc.dirty = true;
        doc.resolvedDirty = true;
        RefreshResolved(doc);
        refreshPreview = true;
    }
    ImGui::SameLine();
    if (ui::Button("##parent_pick", "…", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
        const std::string picked =
            EditorPickOpenFile("Material\0*.lmgraph;*.lmat\0Material Graph\0*.lmgraph\0Material "
                               "Instance\0*.lmat\0All\0*.*\0",
                               "Parent Material");
        if (!picked.empty()) {
            std::string rel = picked;
            if (!ctx.projectPath.empty()) {
                const std::string packRel = MakePackRelativeAssetPath(ctx, picked);
                if (!packRel.empty()) {
                    rel = packRel;
                }
            }
            (void)std::snprintf(doc.parentBuf, sizeof(doc.parentBuf), "%s", rel.c_str());
            doc.dirty = true;
            doc.resolvedDirty = true;
        }
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc browseParent;
        browseParent.label = "Browse";
        browseParent.icon = ui::UiIcon(ELucideIcon::FolderOpen);
        browseParent.variant = ui::EUiVariant::Ghost;
        browseParent.size = ui::EUiSize::Sm;
        browseParent.disabled = doc.parentBuf[0] == '\0';
        if (ui::Button("##parent_browse", browseParent) && doc.parentBuf[0] != '\0') {
            std::string abs = ResolveAssetPath(doc.parentBuf);
            if (abs.empty()) {
                abs = doc.parentBuf;
            }
            ctx.RevealInContentBrowser(abs);
        }
    }

    if (ctx.resources != nullptr && ctx.renderer != nullptr) {
        ImGui::Separator();
        ImGui::TextUnformatted("Preview");
        if (!doc.preview) {
            doc.preview = std::make_unique<MaterialSpherePreview>();
            refreshPreview = true;
        }
        if (refreshPreview) {
            Material previewMat = doc.resolved.material;
            ResolveLeonMaterialTextures(*ctx.resources, previewMat, doc.path,
                                        doc.resolved.baseColorMapPath, doc.resolved.normalMapPath,
                                        doc.resolved.emissiveMapPath, doc.resolved.ormMapPath,
                                        doc.resolved.opacityMaskMapPath);
            doc.preview->SetMaterial(*ctx.resources, previewMat);
        }
        doc.preview->Draw(*ctx.renderer, 220.0f, 220.0f);
    }

    auto markDirty = [&]() {
        doc.dirty = true;
        doc.resolvedDirty = true;
    };

    Material& disp = doc.resolved.material;
    Material& auth = doc.instance.material;

    // When enabling an override pin, seed authoring from the currently displayed (resolved)
    // value so Save does not write Material{} defaults that were never shown.
    auto overridePin = [&](const char* id, bool& flag, auto&& onEnable) -> bool {
        const bool was = flag;
        const bool changed = ui::Checkbox(id, "", &flag, ui::EUiSize::Sm);
        if (changed) {
            if (flag && !was) {
                onEnable();
            }
            markDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Override Parent");
        }
        return changed;
    };

    auto resetBtn = [&](const char* id, bool& flag) {
        ImGui::SameLine();
        ui::UiButtonDesc reset;
        reset.label = "Reset";
        reset.variant = ui::EUiVariant::Ghost;
        reset.size = ui::EUiSize::Sm;
        reset.disabled = !flag;
        reset.tooltip =
            doc.instance.parentPath.empty() ? "No Parent to reset to" : "Reset to Parent";
        if (ui::Button(id, reset) && flag) {
            flag = false;
            markDirty();
            // Same-frame resolve so UV / scalars snap back to Parent immediately.
            RefreshResolved(doc);
            refreshPreview = true;
        }
    };

    ImGui::Separator();
    if (ui::CollapsingSection("General")) {
        overridePin("##ovShading", doc.instance.overrideShading,
                    [&] { auth.shading = disp.shading; });
        ImGui::SameLine();
        int shading = disp.shading == EShadingModel::Unlit ? 1 : 0;
        {
            const bool disabled = !doc.instance.overrideShading && !doc.instance.parentPath.empty();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Shading Model");
            ImGui::SameLine();
            ui::UiSelectDesc shadingDesc;
            shadingDesc.preview = shading == 1 ? "Unlit" : "Default lit";
            shadingDesc.size = ui::EUiSize::Sm;
            shadingDesc.width = 160.0f;
            shadingDesc.disabled = disabled;
            if (ui::BeginSelect("##ShadingModel", shadingDesc)) {
                int next = shading;
                if (ui::SelectItem("Default lit", shading == 0)) {
                    next = 0;
                }
                if (ui::SelectItem("Unlit", shading == 1)) {
                    next = 1;
                }
                if (next != shading) {
                    auth.shading = next == 1 ? EShadingModel::Unlit : EShadingModel::BlinnPhong;
                    disp.shading = auth.shading;
                    doc.instance.overrideShading = true;
                    markDirty();
                }
                ui::EndSelect();
            }
        }
        resetBtn("Reset##sh", doc.instance.overrideShading);

        overridePin("##ovBlend", doc.instance.overrideBlendMode,
                    [&] { auth.blendMode = disp.blendMode; });
        ImGui::SameLine();
        int blend = static_cast<int>(disp.blendMode);
        {
            const bool disabled =
                !doc.instance.overrideBlendMode && !doc.instance.parentPath.empty();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Blend Mode");
            ImGui::SameLine();
            ui::UiSelectDesc blendDesc;
            const char* blendPreview = "Opaque";
            if (blend == 1) {
                blendPreview = "Masked";
            } else if (blend == 2) {
                blendPreview = "Translucent";
            }
            blendDesc.preview = blendPreview;
            blendDesc.size = ui::EUiSize::Sm;
            blendDesc.width = 160.0f;
            blendDesc.disabled = disabled;
            if (ui::BeginSelect("##BlendMode", blendDesc)) {
                int next = blend;
                if (ui::SelectItem("Opaque", blend == 0)) {
                    next = 0;
                }
                if (ui::SelectItem("Masked", blend == 1)) {
                    next = 1;
                }
                if (ui::SelectItem("Translucent", blend == 2)) {
                    next = 2;
                }
                if (next != blend) {
                    auth.blendMode = static_cast<EBlendMode>(next);
                    disp.blendMode = auth.blendMode;
                    doc.instance.overrideBlendMode = true;
                    markDirty();
                }
                ui::EndSelect();
            }
        }
        resetBtn("Reset##bl", doc.instance.overrideBlendMode);

        overridePin("##ovTwo", doc.instance.overrideTwoSided,
                    [&] { auth.twoSided = disp.twoSided; });
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.instance.overrideTwoSided && !doc.instance.parentPath.empty());
        if (ui::Checkbox("##two_sided", "Two Sided", &disp.twoSided)) {
            auth.twoSided = disp.twoSided;
            doc.instance.overrideTwoSided = true;
            markDirty();
        }
        ImGui::EndDisabled();
        resetBtn("Reset##two", doc.instance.overrideTwoSided);

        overridePin("##ovCast", doc.instance.overrideCastsShadows,
                    [&] { auth.castsShadows = disp.castsShadows; });
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.instance.overrideCastsShadows &&
                             !doc.instance.parentPath.empty());
        if (ui::Checkbox("##cast_shadow", "Cast Shadow", &disp.castsShadows)) {
            auth.castsShadows = disp.castsShadows;
            doc.instance.overrideCastsShadows = true;
            markDirty();
        }
        ImGui::EndDisabled();
        resetBtn("Reset##cast", doc.instance.overrideCastsShadows);
        ui::Hint("Material default. Per-actor override: Details → Lighting → Cast Shadow.");

        const bool mirrorPresetReady =
            ctx.renderer != nullptr &&
            ctx.renderer->GetPostProcessSettings().quality >= EPostProcessQuality::Medium;
        overridePin("##ovMirror", doc.instance.overridePlanarMirror,
                    [&] { auth.planarMirror = disp.planarMirror; });
        ImGui::SameLine();
        ImGui::BeginDisabled(
            (!doc.instance.overridePlanarMirror && !doc.instance.parentPath.empty()) ||
            !mirrorPresetReady);
        if (ui::Checkbox("##planar_mirror", "Planar Mirror", &disp.planarMirror)) {
            auth.planarMirror = disp.planarMirror;
            doc.instance.overridePlanarMirror = true;
            markDirty();
        }
        ImGui::EndDisabled();
        resetBtn("Reset##mir", doc.instance.overridePlanarMirror);
        if (!mirrorPresetReady) {
            ui::Hint("Requires + Post graphics preset (World Settings).");
        }

        overridePin("##ovClip", doc.instance.overrideOpacityMaskClipValue,
                    [&] { auth.opacityMaskClipValue = disp.opacityMaskClipValue; });
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.instance.overrideOpacityMaskClipValue &&
                             !doc.instance.parentPath.empty());
        if (ui::SliderFloat("##opacity_clip", "Opacity Mask Clip Value", &disp.opacityMaskClipValue,
                            0.0f, 1.0f)) {
            auth.opacityMaskClipValue = disp.opacityMaskClipValue;
            doc.instance.overrideOpacityMaskClipValue = true;
            markDirty();
        }
        ImGui::EndDisabled();
        resetBtn("Reset##clip", doc.instance.overrideOpacityMaskClipValue);
    }

    if (ui::CollapsingSection("Parameters")) {
        auto colorRow = [&](const char* label, bool& ov, glm::vec3& authV, glm::vec3& dispV,
                            const char* resetId) {
            overridePin((std::string("##ov") + label).c_str(), ov, [&] { authV = dispV; });
            ImGui::SameLine();
            ImGui::BeginDisabled(!ov && !doc.instance.parentPath.empty());
            if (ui::ColorEdit3((std::string("##col_") + label).c_str(), label, &dispV.x)) {
                authV = dispV;
                ov = true;
                markDirty();
            }
            ImGui::EndDisabled();
            resetBtn(resetId, ov);
        };
        colorRow("Base Color", doc.instance.overrideBaseColor, auth.albedo, disp.albedo,
                 "Reset##bc");
        colorRow("Emissive", doc.instance.overrideEmissive, auth.emissive, disp.emissive,
                 "Reset##em");
        colorRow("Specular", doc.instance.overrideSpecular, auth.specular, disp.specular,
                 "Reset##sp");

        auto floatRow = [&](const char* label, bool& ov, float& authV, float& dispV, float lo,
                            float hi, const char* resetId) {
            overridePin((std::string("##ov") + label).c_str(), ov, [&] { authV = dispV; });
            ImGui::SameLine();
            ImGui::BeginDisabled(!ov && !doc.instance.parentPath.empty());
            if (ui::SliderFloat((std::string("##flt_") + label).c_str(), label, &dispV, lo, hi)) {
                authV = dispV;
                ov = true;
                markDirty();
            }
            ImGui::EndDisabled();
            resetBtn(resetId, ov);
        };
        floatRow("Metallic", doc.instance.overrideMetallic, auth.metallic, disp.metallic, 0.0f,
                 1.0f, "Reset##met");
        floatRow("Roughness", doc.instance.overrideRoughness, auth.roughness, disp.roughness, 0.04f,
                 1.0f, "Reset##rg");
        if (doc.instance.overrideRoughness) {
            auth.syncShininessFromRoughness();
            disp.syncShininessFromRoughness();
        }
        floatRow("Opacity", doc.instance.overrideOpacity, auth.alpha, disp.alpha, 0.0f, 1.0f,
                 "Reset##op");

        overridePin("##ovUv", doc.instance.overrideUvScale, [&] { auth.uvScale = disp.uvScale; });
        ImGui::SameLine();
        ImGui::BeginDisabled(!doc.instance.overrideUvScale && !doc.instance.parentPath.empty());
        if (ui::DragFloat2("##uv_scale", "UV Scale", &disp.uvScale.x, 0.05f)) {
            auth.uvScale = disp.uvScale;
            doc.instance.overrideUvScale = true;
            markDirty();
        }
        ImGui::EndDisabled();
        resetBtn("Reset##uv", doc.instance.overrideUvScale);
    }

    if (ui::CollapsingSection("Textures")) {
        RefreshTextureList(ctx);
        auto applyTexturePath = [&](char* buf, size_t bufSize, std::string& authPath, bool& ov,
                                    const std::string& chosen) {
            if (chosen.empty()) {
                return;
            }
            const std::string relative = MakePackRelativeAssetPath(ctx, chosen);
            const std::string store = relative.empty() ? chosen : relative;
            (void)std::snprintf(buf, bufSize, "%s", store.c_str());
            authPath = store;
            ov = true;
            markDirty();
            texturesProjectKey_.clear();
        };
        auto texRow = [&](const char* label, bool& ov, char* buf, size_t bufSize,
                          std::string& authPath, const char* pickId) {
            overridePin((std::string("##ovT") + pickId).c_str(), ov, [&] { authPath = buf; });
            ImGui::SameLine();
            {
                const bool disabled = !ov && !doc.instance.parentPath.empty();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(label);
                ImGui::SameLine();
                ui::UiSelectDesc texDesc;
                texDesc.preview = (buf[0] != '\0') ? buf : "(None)";
                texDesc.previewIcon = ui::UiIcon(ELucideIcon::Image);
                texDesc.size = ui::EUiSize::Sm;
                texDesc.width = 220.0f;
                texDesc.disabled = disabled;
                if (ui::BeginSelect(pickId, texDesc)) {
                    if (ui::SelectItem("(None)", buf[0] == '\0')) {
                        buf[0] = '\0';
                        authPath.clear();
                        ov = true;
                        markDirty();
                    }
                    for (const EditorTextureEntry& entry : textures_) {
                        const bool selected = entry.authoringPath == authPath ||
                                              entry.authoringPath == buf ||
                                              entry.absolutePath == buf;
                        if (ui::SelectItem(entry.displayName.c_str(), selected,
                                           ui::UiIcon(ELucideIcon::Image))) {
                            applyTexturePath(buf, bufSize, authPath, ov, entry.authoringPath);
                        }
                    }
                    if (textures_.empty()) {
                        ui::Hint("No textures in Content — Import a PNG/JPG…");
                    }
                    ui::EndSelect();
                }
            }
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
                    const char* from = static_cast<const char*>(payload->Data);
                    if (from != nullptr && from[0] != '\0' && LooksLikeTexturePath(from)) {
                        applyTexturePath(buf, bufSize, authPath, ov, from);
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            const std::string btn = std::string("…##") + pickId;
            if (ui::Button(btn.c_str(), "…", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
                const std::string picked = EditorPickOpenFile(
                    "Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.exr;*.ltx\0All\0*.*\0", label);
                if (!picked.empty()) {
                    applyTexturePath(buf, bufSize, authPath, ov, picked);
                }
            }
            resetBtn((std::string("Reset##t") + pickId).c_str(), ov);
        };
        texRow("Base Color", doc.instance.overrideBaseColorMap, doc.baseMapBuf,
               sizeof(doc.baseMapBuf), doc.instance.baseColorMapPath, "base");
        texRow("Normal", doc.instance.overrideNormalMap, doc.normalMapBuf, sizeof(doc.normalMapBuf),
               doc.instance.normalMapPath, "norm");
        texRow("Emissive", doc.instance.overrideEmissiveMap, doc.emissiveMapBuf,
               sizeof(doc.emissiveMapBuf), doc.instance.emissiveMapPath, "emis");
        texRow("ORM", doc.instance.overrideOrmMap, doc.ormMapBuf, sizeof(doc.ormMapBuf),
               doc.instance.ormMapPath, "orm");
        texRow("Opacity Mask", doc.instance.overrideOpacityMaskMap, doc.opacityMaskBuf,
               sizeof(doc.opacityMaskBuf), doc.instance.opacityMaskMapPath, "mask");
        ui::Hint("ORM = Occlusion(R) Roughness(G) Metallic(B). Drop a texture from "
                 "Content Browser or pick from the list.");
    }

    if (!doc.resolved.parentGraphParameters.empty() &&
        ui::CollapsingSection("Material Property Overrides")) {
        for (const MaterialGraphParameter& p : doc.resolved.parentGraphParameters) {
            ImGui::PushID(p.name.c_str());
            if (p.type == EMaterialParameterType::Scalar) {
                const bool overr = doc.instance.scalarParameterOverrides.count(p.name) > 0;
                bool flag = overr;
                float v = overr ? doc.instance.scalarParameterOverrides[p.name] : p.defaultValue.x;
                if (doc.resolved.scalarParameterOverrides.count(p.name)) {
                    v = doc.resolved.scalarParameterOverrides[p.name];
                }
                if (ui::Checkbox("##ov", "", &flag, ui::EUiSize::Sm)) {
                    if (flag && !overr) {
                        doc.instance.scalarParameterOverrides[p.name] = v;
                    } else if (!flag) {
                        doc.instance.scalarParameterOverrides.erase(p.name);
                    }
                    markDirty();
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(!flag && !doc.instance.parentPath.empty());
                if (ui::SliderFloat((std::string("##scalar_") + p.name).c_str(), p.name.c_str(), &v,
                                    0.0f, 1.0f)) {
                    doc.instance.scalarParameterOverrides[p.name] = v;
                    markDirty();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                {
                    ui::UiButtonDesc reset;
                    reset.label = "Reset";
                    reset.variant = ui::EUiVariant::Ghost;
                    reset.size = ui::EUiSize::Sm;
                    reset.disabled = !overr;
                    if (ui::Button("##reset_scalar", reset) && overr) {
                        doc.instance.scalarParameterOverrides.erase(p.name);
                        markDirty();
                    }
                }
            } else {
                const bool overr = doc.instance.vectorParameterOverrides.count(p.name) > 0;
                bool flag = overr;
                glm::vec3 v =
                    overr ? doc.instance.vectorParameterOverrides[p.name] : p.defaultValue;
                if (doc.resolved.vectorParameterOverrides.count(p.name)) {
                    v = doc.resolved.vectorParameterOverrides[p.name];
                }
                if (ui::Checkbox("##ov", "", &flag, ui::EUiSize::Sm)) {
                    if (flag && !overr) {
                        doc.instance.vectorParameterOverrides[p.name] = v;
                    } else if (!flag) {
                        doc.instance.vectorParameterOverrides.erase(p.name);
                    }
                    markDirty();
                }
                ImGui::SameLine();
                ImGui::BeginDisabled(!flag && !doc.instance.parentPath.empty());
                if (ui::ColorEdit3((std::string("##vec_") + p.name).c_str(), p.name.c_str(),
                                   &v.x)) {
                    doc.instance.vectorParameterOverrides[p.name] = v;
                    markDirty();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                {
                    ui::UiButtonDesc reset;
                    reset.label = "Reset";
                    reset.variant = ui::EUiVariant::Ghost;
                    reset.size = ui::EUiSize::Sm;
                    reset.disabled = !overr;
                    if (ui::Button("##reset_vector", reset) && overr) {
                        doc.instance.vectorParameterOverrides.erase(p.name);
                        markDirty();
                    }
                }
            }
            ImGui::PopID();
        }
    }
}

void MaterialEditorPanel::DrawGraphDoc(EditorContext& ctx, Doc& doc) {
    ui::TextTitle("Material");
    ui::TextMuted("Parent graph — expressions lite → outputs (no HLSL emit)");

    {
        ui::UiButtonDesc save;
        save.label = "Save Asset";
        save.icon = ui::UiIcon(ELucideIcon::Save);
        save.variant = ui::EUiVariant::Primary;
        save.size = ui::EUiSize::Sm;
        if (ui::Button("##mg_save", save)) {
            (void)SaveDoc(ctx, doc);
        }
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc createMi;
        createMi.label = "Create Material Instance";
        createMi.icon = ui::UiIcon(ELucideIcon::Plus);
        createMi.variant = ui::EUiVariant::Secondary;
        createMi.size = ui::EUiSize::Sm;
        if (ui::Button("##mg_create_mi", createMi)) {
            CreateInstanceFromDoc(ctx, doc);
        }
    }
    ImGui::SameLine();
    {
        ui::UiButtonDesc revert;
        revert.label = "Revert";
        revert.variant = ui::EUiVariant::Ghost;
        revert.size = ui::EUiSize::Sm;
        revert.disabled = !doc.dirty;
        if (ui::Button("##mg_revert", revert) && doc.dirty) {
            LeonMaterialGraphDocument data;
            if (LoadLeonMaterialGraphDocument(doc.path, data)) {
                doc.graph = std::move(data);
                SyncBuffersFromDoc(doc);
                doc.dirty = false;
            }
        }
    }

    if (ui::InputText("##mg_name", "Name", doc.nameBuf, sizeof(doc.nameBuf))) {
        doc.dirty = true;
    }

    ImGui::Separator();
    if (ui::CollapsingSection("Details")) {
        ui::Hint("Outputs wire expression IDs into surface pins.");
    }

    if (ui::CollapsingSection("Graph")) {
        ui::UiGraphCanvasDesc canvasDesc;
        canvasDesc.size = ImVec2(0.0f, 180.0f);
        canvasDesc.emptyHint = "Add expressions below to build the graph.";
        if (ui::BeginGraphCanvas("##mg_canvas", canvasDesc)) {
            ui::Hint("Lite graph — wire expression IDs into material outputs.");
            ui::EndGraphCanvas();
        }

        ui::GraphSectionHeader("Outputs", "Expression ID per surface pin");
        auto outPin = [&](const char* id, const char* label, int& pin) {
            if (ui::GraphPinInt(id, label, &pin)) {
                doc.dirty = true;
            }
        };
        outPin("##pin_bc", MaterialPinLabel("BaseColor"), doc.graph.baseColor);
        outPin("##pin_n", "Normal", doc.graph.normal);
        outPin("##pin_m", "Metallic", doc.graph.metallic);
        outPin("##pin_r", "Roughness", doc.graph.roughness);
        outPin("##pin_e", "Emissive", doc.graph.emissive);
        outPin("##pin_o", MaterialPinLabel("OpacityMask"), doc.graph.opacityMask);
        outPin("##pin_orm", MaterialPinLabel("ORM"), doc.graph.orm);

        ui::GraphSectionHeader("Expressions");
        ui::SectionLabel("Add");
        ui::UiSelectDesc addExpr;
        addExpr.preview = "Add expression…";
        addExpr.previewIcon = ui::UiIcon(ELucideIcon::Plus);
        addExpr.size = ui::EUiSize::Sm;
        addExpr.width = 220.0f;
        if (ui::BeginSelect("##AddExpr", addExpr)) {
            const EMaterialExpressionType types[] = {EMaterialExpressionType::Constant3Vector,
                                                     EMaterialExpressionType::TextureSample,
                                                     EMaterialExpressionType::Multiply,
                                                     EMaterialExpressionType::Lerp,
                                                     EMaterialExpressionType::TextureCoordinate,
                                                     EMaterialExpressionType::ScalarParameter,
                                                     EMaterialExpressionType::VectorParameter};
            for (EMaterialExpressionType t : types) {
                if (ui::SelectItem(ExpressionTypeLabel(t), false)) {
                    MaterialExpression e{};
                    e.id = NextExpressionId(doc.graph);
                    e.type = t;
                    if (t == EMaterialExpressionType::Constant3Vector) {
                        e.value = {0.7f, 0.7f, 0.72f};
                    } else if (t == EMaterialExpressionType::ScalarParameter) {
                        e.parameterName = "Scalar";
                        e.value = {0.5f, 0.5f, 0.5f};
                    } else if (t == EMaterialExpressionType::VectorParameter) {
                        e.parameterName = "Vector";
                        e.value = {1.0f, 1.0f, 1.0f};
                    }
                    doc.graph.expressions.push_back(e);
                    doc.dirty = true;
                }
            }
            ui::EndSelect();
        }

        int removeIndex = -1;
        for (std::size_t i = 0; i < doc.graph.expressions.size(); ++i) {
            MaterialExpression& e = doc.graph.expressions[i];
            ImGui::PushID(static_cast<int>(i));
            char nodeTitle[96];
            (void)std::snprintf(nodeTitle, sizeof(nodeTitle), "%s  ·  ID %d",
                                ExpressionTypeLabel(e.type), e.id);
            if (ui::BeginGraphNode("##expr_node", nodeTitle)) {
                if (ui::Button("##rm_expr", "Remove", ui::EUiVariant::Destructive,
                               ui::EUiSize::Sm)) {
                    removeIndex = static_cast<int>(i);
                }
                if (ui::InputInt("##expr_id", "ID", &e.id, 1, 100, 0, 0.0f, ui::EUiSize::Sm)) {
                    doc.dirty = true;
                }
                switch (e.type) {
                case EMaterialExpressionType::Constant3Vector:
                    if (ui::ColorEdit3("##expr_value", "Value", &e.value.x)) {
                        doc.dirty = true;
                    }
                    break;
                case EMaterialExpressionType::ScalarParameter: {
                    char nameBuf[128]{};
                    (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", e.parameterName.c_str());
                    if (ui::InputText("##param_name", "Parameter Name", nameBuf, sizeof(nameBuf))) {
                        e.parameterName = nameBuf;
                        doc.dirty = true;
                    }
                    if (ui::SliderFloat("##expr_default_scalar", "Default", &e.value.x, 0.0f,
                                        1.0f)) {
                        e.value.y = e.value.x;
                        e.value.z = e.value.x;
                        doc.dirty = true;
                    }
                    break;
                }
                case EMaterialExpressionType::VectorParameter: {
                    char nameBuf[128]{};
                    (void)std::snprintf(nameBuf, sizeof(nameBuf), "%s", e.parameterName.c_str());
                    if (ui::InputText("##param_name", "Parameter Name", nameBuf, sizeof(nameBuf))) {
                        e.parameterName = nameBuf;
                        doc.dirty = true;
                    }
                    if (ui::ColorEdit3("##expr_default_vector", "Default", &e.value.x)) {
                        doc.dirty = true;
                    }
                    break;
                }
                case EMaterialExpressionType::TextureSample: {
                    RefreshTextureList(ctx);
                    char texBuf[260]{};
                    (void)std::snprintf(texBuf, sizeof(texBuf), "%s", e.texture.c_str());
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Texture");
                    ImGui::SameLine();
                    ui::UiSelectDesc texDesc;
                    texDesc.preview = e.texture.empty() ? "(None)" : e.texture.c_str();
                    texDesc.previewIcon = ui::UiIcon(ELucideIcon::Image);
                    texDesc.size = ui::EUiSize::Sm;
                    texDesc.width = 220.0f;
                    if (ui::BeginSelect("##TexSample", texDesc)) {
                        if (ui::SelectItem("(None)", e.texture.empty())) {
                            e.texture.clear();
                            doc.dirty = true;
                        }
                        for (const EditorTextureEntry& entry : textures_) {
                            const bool selected = entry.authoringPath == e.texture;
                            if (ui::SelectItem(entry.displayName.c_str(), selected,
                                               ui::UiIcon(ELucideIcon::Image))) {
                                e.texture = entry.authoringPath;
                                doc.dirty = true;
                            }
                        }
                        ui::EndSelect();
                    }
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload =
                                ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
                            const char* from = static_cast<const char*>(payload->Data);
                            if (from != nullptr && LooksLikeTexturePath(from)) {
                                const std::string relative = MakePackRelativeAssetPath(ctx, from);
                                e.texture = relative.empty() ? from : relative;
                                doc.dirty = true;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if (ui::InputText("##tex_path", "Texture Path", texBuf, sizeof(texBuf))) {
                        e.texture = texBuf;
                        doc.dirty = true;
                    }
                    if (ui::InputInt("##uv_id", "UV Expr Id", &e.uv, 1, 100, 0, 0.0f,
                                     ui::EUiSize::Sm)) {
                        doc.dirty = true;
                    }
                    break;
                }
                case EMaterialExpressionType::Multiply:
                    if (ui::InputInt("##mul_a", "A", &e.a, 1, 100, 0, 0.0f, ui::EUiSize::Sm) ||
                        ui::InputInt("##mul_b", "B", &e.b, 1, 100, 0, 0.0f, ui::EUiSize::Sm)) {
                        doc.dirty = true;
                    }
                    break;
                case EMaterialExpressionType::Lerp:
                    if (ui::InputInt("##lerp_a", "A", &e.a, 1, 100, 0, 0.0f, ui::EUiSize::Sm) ||
                        ui::InputInt("##lerp_b", "B", &e.b, 1, 100, 0, 0.0f, ui::EUiSize::Sm) ||
                        ui::InputInt("##lerp_alpha", "Alpha", &e.alpha, 1, 100, 0, 0.0f,
                                     ui::EUiSize::Sm)) {
                        doc.dirty = true;
                    }
                    break;
                case EMaterialExpressionType::TextureCoordinate:
                    if (ui::InputInt("##uv_idx", "Coordinate Index", &e.coordinateIndex, 1, 100, 0,
                                     0.0f, ui::EUiSize::Sm)) {
                        doc.dirty = true;
                    }
                    break;
                }
            }
            ui::EndGraphNode();
            ImGui::PopID();
        }
        if (removeIndex >= 0) {
            doc.graph.expressions.erase(doc.graph.expressions.begin() + removeIndex);
            doc.dirty = true;
        }
    }

    if (ctx.resources != nullptr && ctx.renderer != nullptr) {
        ImGui::Separator();
        ImGui::TextUnformatted("Preview");
        if (!doc.preview) {
            doc.preview = std::make_unique<MaterialSpherePreview>();
            doc.graphPreviewReady = false;
        }
        // Live recompile while dirty; once after open/save when not yet valid.
        if (doc.dirty || !doc.graphPreviewReady) {
            MaterialGraphBindings bindings;
            if (CompileLeonMaterialGraph(doc.graph, bindings)) {
                Material previewMat = bindings.material;
                ResolveLeonMaterialTextures(*ctx.resources, previewMat, doc.path,
                                            bindings.baseColorMapPath, bindings.normalMapPath,
                                            bindings.emissiveMapPath, bindings.ormMapPath,
                                            bindings.opacityMaskMapPath);
                doc.preview->SetMaterial(*ctx.resources, previewMat);
            }
            doc.graphPreviewReady = !doc.dirty;
        }
        doc.preview->Draw(*ctx.renderer, 220.0f, 220.0f);
    }
}

void MaterialEditorPanel::Draw(EditorContext& ctx) {
    if (!ctx.requestOpenMaterialPath.empty()) {
        OpenMaterial(ctx.requestOpenMaterialPath);
        ctx.requestOpenMaterialPath.clear();
        ctx.showMaterialEditor = true;
        ctx.requestFocusMaterialEditor = true;
    }

    // Do not Begin an empty shell at startup — show* defaults false; Window menu can open it.
    if (!ctx.showMaterialEditor && docs_.empty()) {
        SyncDirtyPaths(ctx);
        return;
    }

    if (ctx.showMaterialEditor && docs_.empty()) {
        if (ctx.requestFocusMaterialEditor) {
            ImGui::SetNextWindowFocus();
            ctx.requestFocusMaterialEditor = false;
        }
        if (ui::BeginPanel("Material Editor", {.pOpen = &ctx.showMaterialEditor})) {
            ui::Hint(
                "Double-click a .lmat (Material Instance) or .lmgraph (Material) in the Content "
                "Browser.\n"
                "Use Create Material Instance on a Material to author Parent overrides.");
            ui::EndPanel();
        }
        SyncDirtyPaths(ctx);
        return;
    }

    if (ctx.requestFocusMaterialEditor) {
        ImGui::SetNextWindowFocus();
        ctx.requestFocusMaterialEditor = false;
    }

    if (!ui::BeginPanel("Material Editor", {.pOpen = &ctx.showMaterialEditor})) {
        ui::EndPanel();
        SyncDirtyPaths(ctx);
        return;
    }

    if (ImGui::BeginTabBar("##MaterialEditorTabs",
                           ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (Doc& doc : docs_) {
            if (!doc.open) {
                continue;
            }
            const std::string displayName =
                doc.mode == EMode::Graph ? doc.graph.name : doc.instance.name;
            std::string tabLabel = displayName;
            if (doc.dirty) {
                tabLabel += " *";
            }
            tabLabel += "###MatTab" + doc.path;
            ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
            if (doc.focusOnce) {
                flags |= ImGuiTabItemFlags_SetSelected;
                doc.focusOnce = false;
            }
            bool open = true;
            if (ImGui::BeginTabItem(tabLabel.c_str(), &open, flags)) {
                focusedPath_ = doc.path;
                ui::TextPath(doc.path.c_str());
                if (doc.dirty) {
                    ImGui::SameLine();
                    ui::TextCaption("*");
                }
                if (doc.mode == EMode::Graph) {
                    DrawGraphDoc(ctx, doc);
                } else {
                    DrawInstanceDoc(ctx, doc);
                }
                ImGui::EndTabItem();
            }
            if (!open) {
                if (doc.dirty) {
                    open = true;
                    ctx.pendingCloseAssetTabPath = doc.path;
                    ctx.pendingCloseAssetTabKind = EAssetEditorTabKind::Material;
                    ImGui::OpenPopup("Close Asset Tab");
                } else {
                    doc.open = false;
                }
            }
        }
        ImGui::EndTabBar();
    }

    ui::EndPanel();

    docs_.erase(std::remove_if(docs_.begin(), docs_.end(), [](const Doc& d) { return !d.open; }),
                docs_.end());
    if (docs_.empty()) {
        ctx.showMaterialEditor = false;
        focusedPath_.clear();
    } else if (FindDoc(focusedPath_) == nullptr) {
        for (const Doc& d : docs_) {
            if (d.open) {
                focusedPath_ = d.path;
                break;
            }
        }
    }

    SyncDirtyPaths(ctx);
}

} // namespace leon::editor
