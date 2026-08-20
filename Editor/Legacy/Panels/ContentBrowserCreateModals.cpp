#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/FileIO.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/panels/ContentBrowserHelpers.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/physics/PhysicsAsset.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/LeonMaterialGraph.h>
#include <leon/ui/UserWidgetDocument.h>
#include <set>
#include <string>

namespace leon::editor {
namespace fs = std::filesystem;
using content_browser::ExtLower;
using content_browser::IsAssetPathDirty;
using content_browser::NormalizePath;

namespace {

[[nodiscard]] std::string extLower(const fs::path& p) {
    return ExtLower(p);
}

} // namespace

void ContentBrowserPanel::BeginRenameAsset(EditorContext& ctx, const std::string& path) {
    if (!IsEditablePackAsset(ctx, path)) {
        return;
    }
    modalAssetPath_ = path;
    const fs::path p(path);
    const std::string stem = p.stem().string();
    (void)std::snprintf(renameNameBuf_, sizeof(renameNameBuf_), "%s", stem.c_str());
    openRenameModal_ = true;
}

void ContentBrowserPanel::BeginDeleteAsset(EditorContext& ctx, const std::string& path) {
    BeginDeletePaths(ctx, {path});
}

void ContentBrowserPanel::BeginDeletePaths(EditorContext& ctx,
                                           const std::vector<std::string>& paths) {
    pendingDeletePaths_.clear();
    deleteRefCount_ = 0;
    deleteReferencers_.clear();
    std::set<std::string> referencerSeen;
    for (const std::string& path : paths) {
        if (!IsEditablePackAsset(ctx, path)) {
            continue;
        }
        pendingDeletePaths_.push_back(NormalizePath(path));
        deleteRefCount_ += CountAssetReferences(ctx, path);
        if (static_cast<int>(deleteReferencers_.size()) < 20) {
            const int remaining = 20 - static_cast<int>(deleteReferencers_.size());
            for (const std::string& ref : CollectAssetReferencers(ctx, path, remaining)) {
                if (referencerSeen.insert(ref).second) {
                    deleteReferencers_.push_back(ref);
                }
            }
        }
    }
    if (pendingDeletePaths_.empty()) {
        return;
    }
    modalAssetPath_ = pendingDeletePaths_.size() == 1 ? pendingDeletePaths_.front() : std::string{};
    openDeleteModal_ = true;
}

void ContentBrowserPanel::BeginReferenceViewer(EditorContext& ctx, const std::string& path) {
    if (path.empty()) {
        return;
    }
    modalAssetPath_ = NormalizePath(path);
    referenceViewerRefCount_ = CountAssetReferences(ctx, modalAssetPath_);
    referenceViewerReferencers_ = CollectAssetReferencers(ctx, modalAssetPath_, 64);
    openReferenceViewerModal_ = true;
}

void ContentBrowserPanel::BeginDuplicatePaths(EditorContext& ctx,
                                              const std::vector<std::string>& paths) {
    ctx.requestDuplicateAssetPaths.clear();
    for (const std::string& path : paths) {
        if (!IsEditablePackAsset(ctx, path)) {
            continue;
        }
        std::error_code ec;
        if ((fs::is_regular_file(path, ec) || fs::is_directory(path, ec)) && !ec) {
            ctx.requestDuplicateAssetPaths.push_back(NormalizePath(path));
        }
    }
}

void ContentBrowserPanel::DrawAssetItemContextMenu(EditorContext& ctx, const Entry& entry) {
    if (!ImGui::BeginPopupContextItem("CBAssetItemMenu")) {
        return;
    }
    // RMB on an unselected tile becomes the sole selection; keep multi-select when already in it.
    if (!IsPathSelected(entry.path)) {
        SelectOnly(ctx, entry.path);
    } else {
        SetPrimarySelection(ctx, entry.path);
    }

    const bool dirty = IsAssetPathDirty(ctx, entry);
    const bool canSaveAsset =
        dirty && (entry.kind == Entry::Kind::Level || entry.kind == Entry::Kind::Material ||
                  entry.kind == Entry::Kind::MaterialGraph);
    if (ImGui::MenuItem("Save Asset", "Ctrl+S", false, canSaveAsset)) {
        ctx.requestSaveAssetPath = entry.path;
    }
    if (ImGui::MenuItem("Save All", "Ctrl+Shift+S", false,
                        ctx.dirty || !ctx.dirtyMaterialPaths.empty())) {
        ctx.requestSaveAll = true;
    }
    ImGui::Separator();
    if (IsImportableSourcePath(entry.path)) {
        if (ImGui::MenuItem("Import…")) {
            ctx.pendingImportSourcePath = entry.path;
            ctx.requestOpenImportDialog = true;
        }
        ImGui::Separator();
    }
    const bool canReimport =
        (entry.kind == Entry::Kind::Character || entry.kind == Entry::Kind::StaticMesh ||
         entry.kind == Entry::Kind::SkelMesh || entry.kind == Entry::Kind::Anim ||
         (entry.kind == Entry::Kind::Texture && extLower(entry.path) == ".ltx")) &&
        HasImportSidecar(entry.path);
    if (ImGui::MenuItem("Reimport", nullptr, false, canReimport)) {
        AssetImportResult result;
        if (EditorReimportAsset(ctx, entry.path, result)) {
            ctx.previewAssetPath = result.previewPath;
            ctx.requestPreviewReload = true;
            ctx.requestContentRefresh = true;
            EditorToast(result.message, EEditorToastKind::Success, 3.0f);
        } else {
            EditorToast(result.message.empty() ? "Reimport failed" : result.message,
                        EEditorToastKind::Error, 4.5f);
        }
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (canReimport) {
            ImGui::SetTooltip("Re-cook from the source path stored in .leonimport");
        } else if (entry.kind == Entry::Kind::Character || entry.kind == Entry::Kind::StaticMesh ||
                   entry.kind == Entry::Kind::SkelMesh || entry.kind == Entry::Kind::Anim ||
                   (entry.kind == Entry::Kind::Texture && extLower(entry.path) == ".ltx")) {
            ImGui::SetTooltip("No .leonimport sidecar — import once to enable Reimport");
        }
    }
    if (entry.kind == Entry::Kind::SkelMesh || entry.kind == Entry::Kind::Character) {
        if (ImGui::MenuItem("Create Physics Asset")) {
            std::string outPath;
            std::string err;
            if (CreatePhysicsAssetBesideMesh(entry.path, outPath, &err)) {
                ctx.requestContentRefresh = true;
                EditorToast("Created Physics Asset: " + fs::path(outPath).filename().string(),
                            EEditorToastKind::Success, 3.0f);
            } else {
                EditorToast(err.empty() ? "Create Physics Asset failed" : err,
                            EEditorToastKind::Error, 4.5f);
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Auto box bodies from skeleton bones → <Mesh>.lphys beside mesh");
        }
    }
    if (entry.kind == Entry::Kind::Material || entry.kind == Entry::Kind::MaterialGraph) {
        if (ImGui::MenuItem("Create Material Instance")) {
            createInstanceParentPath_ = entry.path;
            const std::string suggested =
                SuggestMaterialInstanceName(fs::path(entry.path).stem().string());
            (void)std::snprintf(createInstanceNameBuf_, sizeof(createInstanceNameBuf_), "%s",
                                suggested.c_str());
            openCreateInstanceFromModal_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Create a Material Instance (.lmat) with Parent set to this asset");
        }
    }
    const bool isFavorite = ctx.IsContentFavorite(entry.path);
    if (ImGui::MenuItem(isFavorite ? "Remove from Favorites" : "Add to Favorites", nullptr, false,
                        !entry.isDirectory)) {
        ctx.ToggleContentFavorite(entry.path);
    }
    ImGui::Separator();
    const bool editable = IsEditablePackAsset(ctx, entry.path);
    const bool multiDelete = selectedPaths_.size() > 1 && IsPathSelected(entry.path);
    const bool multiSelect = selectedPaths_.size() > 1 && IsPathSelected(entry.path);
    if (ImGui::MenuItem("Reference Viewer", nullptr, false, !entry.isDirectory)) {
        BeginReferenceViewer(ctx, entry.path);
    }
    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editable)) {
        if (multiSelect) {
            BeginDuplicatePaths(ctx, selectedPaths_);
        } else {
            BeginDuplicatePaths(ctx, {entry.path});
        }
    }
    if (ImGui::MenuItem("Rename", "F2", false, editable && !multiDelete)) {
        BeginRenameAsset(ctx, entry.path);
    }
    if (ImGui::MenuItem(multiDelete ? "Delete Selection" : "Delete", "Del", false, editable)) {
        if (multiDelete) {
            BeginDeletePaths(ctx, selectedPaths_);
        } else {
            BeginDeleteAsset(ctx, entry.path);
        }
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawRenameAssetModal(EditorContext& ctx) {
    if (openRenameModal_) {
        ImGui::OpenPopup("Rename Asset");
        openRenameModal_ = false;
    }
    if (!ui::BeginModal("Rename Asset", nullptr)) {
        return;
    }
    const fs::path path(modalAssetPath_);
    ImGui::TextUnformatted(path.filename().generic_string().c_str());
    (void)ui::InputText("##rename_name", "Name", renameNameBuf_, sizeof(renameNameBuf_));
    if (!path.extension().empty()) {
        ImGui::TextDisabled("Extension: %s", path.extension().generic_string().c_str());
    }
    if (ui::DialogButton("##rename", "Rename", ui::EUiVariant::Primary)) {
        ctx.requestRenameAssetPath = modalAssetPath_;
        ctx.pendingRenameAssetName = renameNameBuf_;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawDeleteAssetsModal(EditorContext& ctx) {
    if (openDeleteModal_) {
        ImGui::OpenPopup("Delete Assets");
        openDeleteModal_ = false;
    }
    if (!ui::BeginModal("Delete Assets", nullptr)) {
        return;
    }
    if (pendingDeletePaths_.size() <= 1) {
        ImGui::TextWrapped("%s", pendingDeletePaths_.empty() ? modalAssetPath_.c_str()
                                                             : pendingDeletePaths_.front().c_str());
    } else {
        ImGui::TextWrapped("Delete %zu selected assets?", pendingDeletePaths_.size());
        const std::size_t show = std::min<std::size_t>(pendingDeletePaths_.size(), 8);
        for (std::size_t i = 0; i < show; ++i) {
            ImGui::BulletText("%s",
                              fs::path(pendingDeletePaths_[i]).filename().generic_string().c_str());
        }
        if (pendingDeletePaths_.size() > show) {
            ImGui::TextDisabled("…and %zu more", pendingDeletePaths_.size() - show);
        }
    }
    if (deleteRefCount_ > 0) {
        ImGui::TextWrapped("Referenced by %d locations.", deleteRefCount_);
        if (ui::CollapsingSection("Referencers")) {
            const std::size_t show =
                std::min<std::size_t>(deleteReferencers_.size(), static_cast<std::size_t>(20));
            for (std::size_t i = 0; i < show; ++i) {
                ImGui::BulletText("%s", deleteReferencers_[i].c_str());
            }
            if (deleteReferencers_.size() > show) {
                ImGui::TextDisabled("…");
            }
        }
        ImGui::TextDisabled("Force Delete clears soft references (Fix Up References).");
    } else {
        ImGui::TextUnformatted("No references found in other packages.");
    }

    auto confirmDelete = [&]() {
        ctx.requestDeleteAssetPaths = pendingDeletePaths_;
        pendingDeletePaths_.clear();
        ClearBrowserSelection(ctx);
        ImGui::CloseCurrentPopup();
    };

    if (deleteRefCount_ > 0) {
        if (ui::DialogButton("##force_delete", "Force Delete", ui::EUiVariant::Destructive,
                             140.0f)) {
            confirmDelete();
        }
        ImGui::SameLine();
        {
            ui::UiButtonDesc blockedDelete;
            blockedDelete.label = "Delete";
            blockedDelete.variant = ui::EUiVariant::Secondary;
            blockedDelete.disabled = true;
            blockedDelete.width = 140.0f;
            (void)ui::Button("##delete_blocked", blockedDelete);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip(
                    "Asset is referenced. Use Force Delete to clear references first.");
            }
        }
    } else if (ui::DialogButton("##delete_confirm", "Delete", ui::EUiVariant::Destructive,
                                140.0f)) {
        confirmDelete();
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        pendingDeletePaths_.clear();
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawReferenceViewerModal(EditorContext& ctx) {
    (void)ctx;
    if (openReferenceViewerModal_) {
        ImGui::OpenPopup("Reference Viewer");
        openReferenceViewerModal_ = false;
    }
    if (!ui::BeginModal("Reference Viewer", nullptr)) {
        return;
    }
    ImGui::TextWrapped("%s", modalAssetPath_.c_str());
    if (referenceViewerRefCount_ > 0) {
        ImGui::TextWrapped("Referenced by %d locations.", referenceViewerRefCount_);
        ImGui::Separator();
        for (const std::string& ref : referenceViewerReferencers_) {
            ImGui::BulletText("%s", ref.c_str());
        }
        if (referenceViewerReferencers_.size() <
            static_cast<std::size_t>(referenceViewerRefCount_)) {
            ImGui::TextDisabled("…and more referencers not shown");
        }
    } else {
        ImGui::TextUnformatted("No references found in the open level or pack assets.");
    }
    if (ui::DialogButton("##close_refs", "Close", ui::EUiVariant::Primary)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawCreateContextMenu(EditorContext& ctx) {
    if (!ImGui::BeginPopupContextWindow("ContentBrowserCreateMenu",
                                        ImGuiPopupFlags_MouseButtonRight |
                                            ImGuiPopupFlags_NoOpenOverExistingPopup)) {
        return;
    }
    const bool canSaveAll = ctx.dirty || !ctx.dirtyMaterialPaths.empty() ||
                            !ctx.dirtyBlueprintPaths.empty() || !ctx.dirtyWidgetPaths.empty();
    if (ImGui::MenuItem("Save All", "Ctrl+Shift+S", false, canSaveAll)) {
        ctx.requestSaveAll = true;
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Create")) {
        if (ImGui::MenuItem("Blueprint Class…", nullptr, false, !contentRoot_.empty())) {
            (void)std::snprintf(newBlueprintNameBuf_, sizeof(newBlueprintNameBuf_), "BP_New");
            openNewBlueprintModal_ = true;
        }
        if (ImGui::MenuItem("Widget Blueprint…", nullptr, false, !contentRoot_.empty())) {
            (void)std::snprintf(newWidgetNameBuf_, sizeof(newWidgetNameBuf_), "WBP_New");
            openNewWidgetModal_ = true;
        }
        if (ImGui::MenuItem("Material Instance…", nullptr, false, !contentRoot_.empty())) {
            openNewMaterialModal_ = true;
        }
        if (ImGui::MenuItem("Material…", nullptr, false, !contentRoot_.empty())) {
            openNewMaterialGraphModal_ = true;
        }
        if (ImGui::MenuItem("Folder…", nullptr, false, !contentRoot_.empty())) {
            openNewFolderModal_ = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndPopup();
}

void ContentBrowserPanel::DrawNewWidgetModal(EditorContext& ctx) {
    if (openNewWidgetModal_) {
        ImGui::OpenPopup("Create Widget Blueprint");
        openNewWidgetModal_ = false;
    }
    if (!ui::BeginModal("Create Widget Blueprint", nullptr)) {
        return;
    }
    (void)ui::InputText("##new_widget_name", "Name", newWidgetNameBuf_, sizeof(newWidgetNameBuf_));
    ImGui::TextDisabled("Writes WBP_*.luw into UI/ (or the current folder).");
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = newWidgetNameBuf_;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            if (name.size() < 4 || name.compare(0, 4, "WBP_") != 0) {
                name = "WBP_" + name;
            }
            fs::path dir;
            if (!currentFolder_.empty()) {
                const std::string folderName = fs::path(currentFolder_).filename().string();
                dir = (folderName == "UI") ? fs::path(currentFolder_)
                                           : (fs::path(currentFolder_) / "UI");
            } else if (!contentRoot_.empty()) {
                dir = fs::path(contentRoot_) / "UI";
            }
            if (!dir.empty()) {
                std::error_code ec;
                fs::create_directories(dir, ec);
                const fs::path outPath = dir / (name + ".luw");
                UserWidgetDocument doc = MakeDefaultUserWidgetDocument(name);
                doc.root.widgetClass = EWidgetClass::Canvas;
                doc.root.id = "Root";
                WidgetNodeDesc box;
                box.widgetClass = EWidgetClass::VerticalBox;
                box.id = "MenuRoot";
                box.text = name;
                box.hint = "Edit in Widget Designer";
                doc.root.children.push_back(std::move(box));
                if (SaveUserWidgetDocument(outPath.generic_string(), doc)) {
                    scanned_ = false;
                    Refresh(ctx);
                    ctx.requestOpenWidgetPath = outPath.generic_string();
                    ctx.showWidgetDesigner = true;
                    EditorToast("Created " + name + ".luw", EEditorToastKind::Success);
                } else {
                    EditorToast("Failed to write " + outPath.generic_string(),
                                EEditorToastKind::Error);
                }
            }
        }
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawNewBlueprintModal(EditorContext& ctx) {
    if (openNewBlueprintModal_) {
        ImGui::OpenPopup("Create Blueprint Class");
        openNewBlueprintModal_ = false;
    }
    if (!ui::BeginModal("Create Blueprint Class", nullptr)) {
        return;
    }
    (void)ui::InputText("##new_bp_name", "Name", newBlueprintNameBuf_, sizeof(newBlueprintNameBuf_));
    ImGui::TextDisabled("Writes BP_*.lbp into Blueprints/ (or the current folder).");
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = newBlueprintNameBuf_;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            if (name.size() < 3 || name.compare(0, 3, "BP_") != 0) {
                name = "BP_" + name;
            }
            fs::path dir;
            if (!currentFolder_.empty()) {
                const std::string folderName = fs::path(currentFolder_).filename().string();
                dir = (folderName == "Blueprints") ? fs::path(currentFolder_)
                                                   : (fs::path(currentFolder_) / "Blueprints");
            } else if (!contentRoot_.empty()) {
                dir = fs::path(contentRoot_) / "Blueprints";
            }
            if (!dir.empty()) {
                std::error_code ec;
                fs::create_directories(dir, ec);
                const fs::path outPath = dir / (name + ".lbp");
                const BlueprintDocument doc = MakeDefaultBlueprintDocument(name);
                if (SaveBlueprintDocument(outPath.generic_string(), doc)) {
                    scanned_ = false;
                    Refresh(ctx);
                    ctx.requestContentRefresh = true;
                    ctx.contentBrowserSelectedPath = outPath.generic_string();
                    ctx.requestOpenBlueprintPath = outPath.generic_string();
                    ctx.showBlueprintEditor = true;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawNewMaterialModal(EditorContext& ctx) {
    if (openNewMaterialModal_) {
        ImGui::OpenPopup("Create Material Instance");
        openNewMaterialModal_ = false;
    }
    if (!ui::BeginModal("Create Material Instance", nullptr)) {
        return;
    }
    static char matName[128] = "MI_New";
    static float baseColor[3] = {0.7f, 0.7f, 0.72f};
    static float metallic = 0.0f;
    static float roughness = 0.6f;
    (void)ui::InputText("##new_mat_name", "Name", matName, sizeof(matName));
    (void)ui::ColorEdit3("##new_mat_bc", "Base Color", baseColor);
    (void)ui::SliderFloat("##new_mat_met", "Metallic", &metallic, 0.0f, 1.0f);
    (void)ui::SliderFloat("##new_mat_rgh", "Roughness", &roughness, 0.04f, 1.0f);
    ui::Hint("Writes Material Instance .lmat (MI_ prefix) into Materials/ or current folder");
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = matName;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            if (name.size() < 3 || name[0] != 'M' || name[1] != 'I' || name[2] != '_') {
                if (name.size() >= 2 && name[0] == 'M' && name[1] == '_') {
                    name = "MI_" + name.substr(2);
                } else {
                    name = "MI_" + name;
                }
            }
            fs::path dir;
            if (!currentFolder_.empty()) {
                dir = fs::path(currentFolder_);
            } else if (!contentRoot_.empty()) {
                dir = fs::path(contentRoot_) / "Materials";
            } else {
                dir = fs::path(ResolveAssetPath("assets/Materials"));
            }
            std::error_code ec;
            fs::create_directories(dir, ec);
            const fs::path outPath = dir / (name + ".lmat");
            Material mat{};
            mat.albedo = {baseColor[0], baseColor[1], baseColor[2]};
            mat.metallic = metallic;
            mat.roughness = roughness;
            if (SaveLeonMaterialFile(outPath.generic_string(), name, mat)) {
                scanned_ = false;
                Refresh(ctx);
                ctx.requestContentRefresh = true;
                ctx.requestOpenMaterialPath = outPath.generic_string();
                EditorToast("Created " + name + ".lmat", EEditorToastKind::Success);
                ImGui::CloseCurrentPopup();
            } else {
                EditorToast("Failed to write " + outPath.generic_string(), EEditorToastKind::Error);
            }
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawCreateInstanceFromModal(EditorContext& ctx) {
    if (openCreateInstanceFromModal_) {
        ImGui::OpenPopup("Create Material Instance From");
        openCreateInstanceFromModal_ = false;
    }
    if (!ui::BeginModal("Create Material Instance From", nullptr)) {
        return;
    }
    ImGui::TextWrapped("Parent: %s", createInstanceParentPath_.c_str());
    (void)ui::InputText("##mi_name", "Name", createInstanceNameBuf_, sizeof(createInstanceNameBuf_));
    ImGui::TextDisabled("Writes sparse .lmat with Parent= set (Unreal Create Material Instance)");
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = createInstanceNameBuf_;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty() && !createInstanceParentPath_.empty()) {
            if (name.size() < 3 || name[0] != 'M' || name[1] != 'I' || name[2] != '_') {
                name = SuggestMaterialInstanceName(name);
            }
            const fs::path parentFs(createInstanceParentPath_);
            const fs::path outPath = parentFs.parent_path() / (name + ".lmat");
            std::string parentRel = createInstanceParentPath_;
            const std::string packRel = MakePackRelativeAssetPath(ctx, createInstanceParentPath_);
            if (!packRel.empty()) {
                parentRel = packRel;
            }
            const std::string text = MakeMaterialInstanceFromParentText(name, parentRel);
            if (WriteTextFileAtomic(outPath.generic_string(), text)) {
                scanned_ = false;
                Refresh(ctx);
                ctx.requestContentRefresh = true;
                ctx.requestOpenMaterialPath = outPath.generic_string();
                EditorToast("Created " + name + ".lmat", EEditorToastKind::Success);
                ImGui::CloseCurrentPopup();
            } else {
                EditorToast("Failed to write " + outPath.generic_string(), EEditorToastKind::Error);
            }
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawNewMaterialGraphModal(EditorContext& ctx) {
    if (openNewMaterialGraphModal_) {
        ImGui::OpenPopup("Create Material");
        openNewMaterialGraphModal_ = false;
    }
    if (!ui::BeginModal("Create Material", nullptr)) {
        return;
    }
    static char graphName[128] = "M_New";
    static float baseColor[3] = {0.7f, 0.7f, 0.72f};
    static float roughness = 0.6f;
    (void)ui::InputText("##graph_name", "Name", graphName, sizeof(graphName));
    (void)ui::ColorEdit3("##new_graph_bc", "Base Color", baseColor);
    (void)ui::SliderFloat("##new_graph_rgh", "Roughness", &roughness, 0.04f, 1.0f);
    ui::Hint("Writes parent Material .lmgraph (M_ prefix)");
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = graphName;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        if (!name.empty()) {
            if (name.size() < 2 || name[0] != 'M' || name[1] != '_') {
                name = "M_" + name;
            }
            fs::path dir;
            if (!currentFolder_.empty()) {
                dir = fs::path(currentFolder_);
            } else if (!contentRoot_.empty()) {
                dir = fs::path(contentRoot_) / "Materials";
            } else {
                dir = fs::path(ResolveAssetPath("assets/Materials"));
            }
            std::error_code ec;
            fs::create_directories(dir, ec);
            const fs::path outPath = dir / (name + ".lmgraph");
            const LeonMaterialGraphDocument graph = MakeDefaultLeonMaterialGraph(
                name, {baseColor[0], baseColor[1], baseColor[2]}, roughness);
            if (SaveLeonMaterialGraphDocument(outPath.generic_string(), graph)) {
                scanned_ = false;
                Refresh(ctx);
                ctx.requestContentRefresh = true;
                ctx.requestOpenMaterialPath = outPath.generic_string();
                ctx.showMaterialEditor = true;
                EditorToast("Created " + name + ".lmgraph", EEditorToastKind::Success);
                ImGui::CloseCurrentPopup();
            } else {
                EditorToast("Failed to write " + outPath.generic_string(), EEditorToastKind::Error);
            }
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

void ContentBrowserPanel::DrawNewFolderModal(EditorContext& ctx) {
    if (openNewFolderModal_) {
        ImGui::OpenPopup("New Content Folder");
        openNewFolderModal_ = false;
    }
    if (!ui::BeginModal("New Content Folder", nullptr)) {
        return;
    }
    static char folderName[128] = "New folder";
    (void)ui::InputText("##folder_name", "Name", folderName, sizeof(folderName));
    if (ui::DialogButton("##create", "Create", ui::EUiVariant::Primary)) {
        std::string name = folderName;
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
            name.pop_back();
        }
        const std::string parent = !currentFolder_.empty() ? currentFolder_ : contentRoot_;
        if (!name.empty() && name.find_first_of("\\/:*?\"<>|") == std::string::npos &&
            !parent.empty()) {
            std::error_code ec;
            const fs::path dir = fs::path(parent) / name;
            if (fs::create_directories(dir, ec) || fs::is_directory(dir, ec)) {
                scanned_ = false;
                Refresh(ctx);
                ImGui::CloseCurrentPopup();
            } else {
                std::cerr << "Content Browser: failed to create folder " << dir << '\n';
            }
        }
    }
    ImGui::SameLine();
    if (ui::DialogButton("##cancel", "Cancel", ui::EUiVariant::Ghost)) {
        ImGui::CloseCurrentPopup();
    }
    ui::EndModal();
}

} // namespace leon::editor
