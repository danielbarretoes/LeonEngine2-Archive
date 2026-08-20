#include <glm/vec3.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/content/LeonBlueprint.h>
#include <leon/core/Camera.h>
#include <leon/core/FileIO.h>
#include <leon/core/LeonProjectConfig.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/AssetTools.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EngineContent.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/ContentBrowserHelpers.h>
#include <leon/editor/panels/ContentBrowserPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/Engine.h>
#include <leon/physics/PhysicsAsset.h>
#include <leon/render/LeonMaterialFormat.h>
#include <leon/render/LeonMaterialGraph.h>
#include <leon/render/Renderer.h>
#include <leon/render/ResourceCache.h>
#include <leon/ui/UserWidgetDocument.h>
#include <string>

namespace leon::editor {
namespace fs = std::filesystem;
using content_browser::ExtLower;
using content_browser::IsAssetPathDirty;
using content_browser::NormalizePath;
using content_browser::PathsEqualNormalized;

namespace {

// Local alias used throughout this TU (historical name).
[[nodiscard]] std::string extLower(const fs::path& p) {
    return ExtLower(p);
}

[[nodiscard]] bool CbToolbarIconButton(ELucideIcon icon, const char* id, const char* label,
                                       const char* tooltip = nullptr, bool disabled = false) {
    ui::UiButtonDesc desc;
    desc.icon = ui::UiIcon(icon);
    desc.label = label;
    desc.tooltip = tooltip;
    desc.variant = ui::EUiVariant::Ghost;
    desc.size = ui::EUiSize::Sm;
    desc.disabled = disabled;
    return ui::Button(id, desc);
}

[[nodiscard]] std::string ResolvePackRootFromLevelPath(const std::string& levelPath) {
    if (levelPath.empty()) {
        return {};
    }
    std::error_code ec;
    fs::path dir = fs::path(levelPath).parent_path();
    while (!dir.empty()) {
        if (IsLeonProjectDirectory(dir)) {
            return dir.generic_string();
        }
        const fs::path parent = dir.parent_path();
        if (parent == dir) {
            break;
        }
        dir = parent;
    }
    return {};
}

void DrawFolderIcon(ImDrawList* draw, const ImVec2& min, const ImVec2& max) {
    if (draw == nullptr) {
        return;
    }
    draw->AddRectFilled(min, max, IM_COL32(42, 44, 48, 255), 3.0f);
    DrawLucideIcon(draw, min, max, ELucideIcon::Folder, IM_COL32(232, 188, 88, 255));
}

[[nodiscard]] ImVec4 KindTint(ContentBrowserPanel::Entry::Kind kind) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    switch (kind) {
    case Kind::Folder:
        return ImVec4(0.35f, 0.35f, 0.38f, 1.0f);
    case Kind::Level:
        return ImVec4(0.20f, 0.45f, 0.75f, 1.0f);
    case Kind::Material:
        return ImVec4(0.55f, 0.28f, 0.70f, 1.0f);
    case Kind::MaterialGraph:
        return ImVec4(0.45f, 0.22f, 0.80f, 1.0f);
    case Kind::StaticMesh:
        return ImVec4(0.25f, 0.55f, 0.40f, 1.0f);
    case Kind::Texture:
        return ImVec4(0.55f, 0.35f, 0.55f, 1.0f);
    case Kind::Blueprint:
        return ImVec4(0.25f, 0.45f, 0.85f, 1.0f);
    case Kind::UserWidget:
        return ImVec4(0.55f, 0.30f, 0.65f, 1.0f);
    case Kind::Character:
    case Kind::SkelMesh:
        return ImVec4(0.70f, 0.45f, 0.20f, 1.0f);
    case Kind::Hdr:
        return ImVec4(0.15f, 0.55f, 0.70f, 1.0f);
    case Kind::Lightmap:
        return ImVec4(0.85f, 0.70f, 0.25f, 1.0f);
    case Kind::Anim:
    case Kind::AnimMontage:
    case Kind::AnimBlueprint:
        return ImVec4(0.65f, 0.55f, 0.20f, 1.0f);
    case Kind::PhysicsAsset:
        return ImVec4(0.35f, 0.60f, 0.70f, 1.0f);
    default:
        return ImVec4(0.40f, 0.40f, 0.42f, 1.0f);
    }
}

[[nodiscard]] bool ShouldSkipDirectoryName(const std::string& name) {
    // Build / ship / CMake binary dirs — never Content Browser roots (see Docs/NAMING.md).
    return name == "build" || name == "build-linux" || name == "build-fast" ||
           name == "build-ninja" || name == "build-tools" || name == "Shipping" || name == "Dist" ||
           name == "dist" || name == ".git" || name == "src" || name == "_deps" ||
           (name.size() >= 6 && name.compare(0, 6, "_leon_") == 0);
}

[[nodiscard]] bool RectsOverlap(float aMinX, float aMinY, float aMaxX, float aMaxY, float bMinX,
                                float bMinY, float bMaxX, float bMaxY) {
    return aMinX <= bMaxX && aMaxX >= bMinX && aMinY <= bMaxY && aMaxY >= bMinY;
}

} // namespace

bool ContentBrowserPanel::IsPathSelected(const std::string& path) const {
    for (const std::string& p : selectedPaths_) {
        if (PathsEqualNormalized(p, path)) {
            return true;
        }
    }
    return false;
}

void ContentBrowserPanel::ClearBrowserSelection(EditorContext& ctx) {
    selectedPaths_.clear();
    selectionAnchorPath_.clear();
    ctx.contentBrowserSelectedPath.clear();
}

void ContentBrowserPanel::SetPrimarySelection(EditorContext& ctx, const std::string& path) {
    ctx.contentBrowserSelectedPath = NormalizePath(path);
}

void ContentBrowserPanel::SelectOnly(EditorContext& ctx, const std::string& path) {
    const std::string norm = NormalizePath(path);
    selectedPaths_.clear();
    selectedPaths_.push_back(norm);
    selectionAnchorPath_ = norm;
    SetPrimarySelection(ctx, norm);
}

void ContentBrowserPanel::ToggleSelect(EditorContext& ctx, const std::string& path) {
    const std::string norm = NormalizePath(path);
    for (auto it = selectedPaths_.begin(); it != selectedPaths_.end(); ++it) {
        if (PathsEqualNormalized(*it, norm)) {
            selectedPaths_.erase(it);
            if (PathsEqualNormalized(ctx.contentBrowserSelectedPath, norm)) {
                ctx.contentBrowserSelectedPath =
                    selectedPaths_.empty() ? std::string{} : selectedPaths_.back();
            }
            if (selectedPaths_.empty()) {
                selectionAnchorPath_.clear();
            }
            return;
        }
    }
    selectedPaths_.push_back(norm);
    selectionAnchorPath_ = norm;
    SetPrimarySelection(ctx, norm);
}

void ContentBrowserPanel::SelectRange(EditorContext& ctx, const std::vector<Entry>& children,
                                      const std::string& toPath) {
    if (children.empty()) {
        return;
    }
    int anchorIdx = -1;
    int toIdx = -1;
    for (int i = 0; i < static_cast<int>(children.size()); ++i) {
        if (anchorIdx < 0 && PathsEqualNormalized(children[static_cast<std::size_t>(i)].path,
                                                  selectionAnchorPath_)) {
            anchorIdx = i;
        }
        if (toIdx < 0 && PathsEqualNormalized(children[static_cast<std::size_t>(i)].path, toPath)) {
            toIdx = i;
        }
    }
    if (toIdx < 0) {
        SelectOnly(ctx, toPath);
        return;
    }
    if (anchorIdx < 0) {
        selectionAnchorPath_ = NormalizePath(toPath);
        SelectOnly(ctx, toPath);
        return;
    }
    const int lo = std::min(anchorIdx, toIdx);
    const int hi = std::max(anchorIdx, toIdx);
    selectedPaths_.clear();
    selectedPaths_.reserve(static_cast<std::size_t>(hi - lo + 1));
    for (int i = lo; i <= hi; ++i) {
        selectedPaths_.push_back(NormalizePath(children[static_cast<std::size_t>(i)].path));
    }
    SetPrimarySelection(ctx, toPath);
}

void ContentBrowserPanel::SelectAllVisible(EditorContext& ctx, const std::vector<Entry>& children) {
    selectedPaths_.clear();
    selectedPaths_.reserve(children.size());
    for (const Entry& e : children) {
        selectedPaths_.push_back(NormalizePath(e.path));
    }
    if (!selectedPaths_.empty()) {
        if (selectionAnchorPath_.empty() || !IsPathSelected(selectionAnchorPath_)) {
            selectionAnchorPath_ = selectedPaths_.front();
        }
        SetPrimarySelection(ctx, selectedPaths_.back());
    } else {
        ctx.contentBrowserSelectedPath.clear();
    }
}

void ContentBrowserPanel::ApplyMarqueeSelection(EditorContext& ctx,
                                                const std::vector<TileHit>& tiles, bool additive) {
    const float minX = std::min(marqueeStartX_, marqueeEndX_);
    const float minY = std::min(marqueeStartY_, marqueeEndY_);
    const float maxX = std::max(marqueeStartX_, marqueeEndX_);
    const float maxY = std::max(marqueeStartY_, marqueeEndY_);

    if (!additive) {
        selectedPaths_.clear();
    }
    for (const TileHit& tile : tiles) {
        if (!RectsOverlap(minX, minY, maxX, maxY, tile.minX, tile.minY, tile.maxX, tile.maxY)) {
            continue;
        }
        if (!IsPathSelected(tile.path)) {
            selectedPaths_.push_back(NormalizePath(tile.path));
        }
    }
    if (!selectedPaths_.empty()) {
        SetPrimarySelection(ctx, selectedPaths_.back());
        if (selectionAnchorPath_.empty()) {
            selectionAnchorPath_ = selectedPaths_.front();
        }
    } else if (!additive) {
        ctx.contentBrowserSelectedPath.clear();
    }
}

void ContentBrowserPanel::HandleMarquee(EditorContext& ctx, const std::vector<TileHit>& tiles) {
    const ImGuiIO& io = ImGui::GetIO();
    constexpr float kMarqueeSlop = 4.0f;

    if (!marqueeActive_) {
        // Empty-space drag starts a selection rectangle (Unreal-like).
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() &&
            !ImGui::IsAnyItemHovered()) {
            marqueeActive_ = true;
            marqueeAdditive_ = io.KeyCtrl;
            marqueeStartX_ = io.MousePos.x;
            marqueeStartY_ = io.MousePos.y;
            marqueeEndX_ = io.MousePos.x;
            marqueeEndY_ = io.MousePos.y;
            if (!marqueeAdditive_) {
                ClearBrowserSelection(ctx);
            }
        }
        return;
    }

    marqueeEndX_ = io.MousePos.x;
    marqueeEndY_ = io.MousePos.y;
    const float dx = marqueeEndX_ - marqueeStartX_;
    const float dy = marqueeEndY_ - marqueeStartY_;
    const bool dragged = (dx * dx + dy * dy) >= (kMarqueeSlop * kMarqueeSlop);

    if (dragged) {
        ApplyMarqueeSelection(ctx, tiles, marqueeAdditive_);
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        const ImVec2 a(std::min(marqueeStartX_, marqueeEndX_),
                       std::min(marqueeStartY_, marqueeEndY_));
        const ImVec2 b(std::max(marqueeStartX_, marqueeEndX_),
                       std::max(marqueeStartY_, marqueeEndY_));
        draw->AddRectFilled(a, b, IM_COL32(80, 140, 220, 45));
        draw->AddRect(a, b, IM_COL32(120, 180, 255, 220), 0.0f, 0, 1.5f);
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        marqueeActive_ = false;
    }
}

void ContentBrowserPanel::SyncSelectionFolder(EditorContext& ctx) {
    if (selectionFolder_ != currentFolder_) {
        ClearBrowserSelection(ctx);
        selectionFolder_ = currentFolder_;
        marqueeActive_ = false;
    }
}

void ContentBrowserPanel::ApplyTileClick(EditorContext& ctx, const Entry& entry,
                                         const std::vector<Entry>& children) {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyShift) {
        SelectRange(ctx, children, entry.path);
    } else if (io.KeyCtrl) {
        ToggleSelect(ctx, entry.path);
    } else {
        SelectOnly(ctx, entry.path);
    }

    if (!entry.isDirectory && IsPathSelected(entry.path) &&
        PathsEqualNormalized(ctx.contentBrowserSelectedPath, entry.path)) {
        if (entry.kind == Entry::Kind::Material && ctx.requestPickMaterial) {
            ctx.pendingMaterialPickPath = entry.path;
            ctx.requestPickMaterial = false;
        } else if (entry.kind == Entry::Kind::StaticMesh && ctx.requestPickMesh &&
                   extLower(entry.path) == ".lmesh") {
            ctx.pendingMeshPickPath = entry.path;
            ctx.requestPickMesh = false;
        } else if (entry.kind != Entry::Kind::Hdr && entry.kind != Entry::Kind::Skeleton &&
                   entry.kind != Entry::Kind::Level && entry.kind != Entry::Kind::Lightmap &&
                   !IsImportableSourcePath(entry.path)) {
            ctx.previewAssetPath = entry.path;
            ctx.requestPreviewReload = true;
        }
    }
}

ContentBrowserPanel::Entry::Kind ContentBrowserPanel::ClassifyFile(const std::string& pathStr) {
    const fs::path path(pathStr);
    const std::string ext = extLower(path);
    const std::string fname = path.filename().string();

    if (ext == ".obj" || ext == ".lmesh" || ext == ".gltf" || ext == ".glb") {
        return Entry::Kind::StaticMesh;
    }
    if (ext == ".lmat") {
        return Entry::Kind::Material;
    }
    if (ext == ".lmgraph") {
        return Entry::Kind::MaterialGraph;
    }
    if (ext == ".ltx" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".bmp" || ext == ".exr") {
        return Entry::Kind::Texture;
    }
    if (ext == ".lbp") {
        return Entry::Kind::Blueprint;
    }
    if (ext == ".luw") {
        return Entry::Kind::UserWidget;
    }
    if (ext == ".lskm") {
        return Entry::Kind::SkelMesh;
    }
    if (ext == ".lskel") {
        return Entry::Kind::Skeleton;
    }
    if (ext == ".lanim") {
        return Entry::Kind::Anim;
    }
    if (ext == ".lamnt") {
        return Entry::Kind::AnimMontage;
    }
    if (ext == ".labp") {
        return Entry::Kind::AnimBlueprint;
    }
    if (ext == ".lphys") {
        return Entry::Kind::PhysicsAsset;
    }
    if (ext == ".fbx") {
        return Entry::Kind::FbxSource;
    }
    if (ext == ".leonimport") {
        return Entry::Kind::Other; // sidecar — hidden from browser grid
    }
    if (ext == ".lredirect") {
        return Entry::Kind::Redirector;
    }
    if (ext == ".hdr") {
        return Entry::Kind::Hdr;
    }
    if (ext == ".lmb") {
        return Entry::Kind::Lightmap; // Map BuiltData
    }
    if (fname.size() > 6 && fname.compare(fname.size() - 6, 6, ".lchar") == 0) {
        return Entry::Kind::Character;
    }
    if (ext == ".llev") {
        return Entry::Kind::Level;
    }
    return Entry::Kind::Other;
}

void ContentBrowserPanel::EnsureMaterialSwatch(Entry& entry) const {
    if (entry.hasSwatch) {
        return;
    }
    if (entry.kind == Entry::Kind::MaterialGraph) {
        LeonMaterialGraphDocument graph;
        MaterialGraphBindings bindings;
        if (LoadLeonMaterialGraphDocument(entry.path, graph) &&
            CompileLeonMaterialGraph(graph, bindings)) {
            entry.swatch[0] = bindings.material.albedo.x;
            entry.swatch[1] = bindings.material.albedo.y;
            entry.swatch[2] = bindings.material.albedo.z;
            entry.hasSwatch = true;
        }
        return;
    }
    if (entry.kind != Entry::Kind::Material) {
        return;
    }
    LeonMaterialDocument doc;
    if (LoadLeonMaterialDocument(entry.path, doc)) {
        entry.swatch[0] = doc.material.albedo.x;
        entry.swatch[1] = doc.material.albedo.y;
        entry.swatch[2] = doc.material.albedo.z;
        entry.hasSwatch = true;
    }
}

void ContentBrowserPanel::SyncRoot(EditorContext& ctx) {
    const std::string syncKey = ctx.projectPath + "|" + ctx.levelPath;
    if (syncKey == lastLevelPath_ && !contentRoot_.empty()) {
        return;
    }
    lastLevelPath_ = syncKey;
    std::string projectRoot = ctx.projectPath;
    if (projectRoot.empty()) {
        projectRoot = ResolvePackRootFromLevelPath(ctx.levelPath);
    }
    if (projectRoot.empty()) {
        projectRoot = ResolveProjectsDirectory();
    }
    // Unreal-like: browse `<project>/Content` (not CMakeLists / src at project root).
    fs::path content = ProjectContentDirectory(projectRoot);
    std::error_code ec;
    if (!content.empty() && !fs::is_directory(content, ec)) {
        fs::create_directories(content, ec);
        fs::create_directories(content / "Levels", ec);
        fs::create_directories(content / "Materials", ec);
    }
    contentRoot_ = content.empty() ? projectRoot : content.generic_string();
    currentFolder_ = contentRoot_;
    scanned_ = false;
}

ContentBrowserPanel::Entry ContentBrowserPanel::BuildTree(const std::string& absoluteDir) const {
    Entry folder;
    folder.isDirectory = true;
    folder.kind = Entry::Kind::Folder;
    folder.path = absoluteDir;
    folder.name = fs::path(absoluteDir).filename().string();
    if (folder.name.empty()) {
        folder.name = absoluteDir;
    }

    std::error_code ec;
    if (!fs::is_directory(absoluteDir, ec)) {
        return folder;
    }

    std::vector<Entry> dirs;
    std::vector<Entry> files;
    for (const auto& it : fs::directory_iterator(absoluteDir, ec)) {
        Entry e;
        e.name = it.path().filename().string();
        e.path = it.path().generic_string();
        if (it.is_directory()) {
            if (ShouldSkipDirectoryName(e.name)) {
                continue;
            }
            e = BuildTree(e.path);
            dirs.push_back(std::move(e));
            continue;
        }
        if (!it.is_regular_file()) {
            continue;
        }
        e.kind = ClassifyFile(e.path);
        if (e.kind == Entry::Kind::Other) {
            continue;
        }
        if (e.kind == Entry::Kind::Material || e.kind == Entry::Kind::MaterialGraph) {
            EnsureMaterialSwatch(e);
        }
        files.push_back(std::move(e));
    }

    std::sort(dirs.begin(), dirs.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    std::sort(files.begin(), files.end(),
              [](const Entry& a, const Entry& b) { return a.name < b.name; });
    folder.children.reserve(dirs.size() + files.size());
    for (Entry& d : dirs) {
        folder.children.push_back(std::move(d));
    }
    for (Entry& f : files) {
        folder.children.push_back(std::move(f));
    }
    return folder;
}

void ContentBrowserPanel::Refresh(EditorContext& ctx) {
    SyncRoot(ctx);
    scanned_ = true;
    InvalidateLiveFolderListing();
    materialThumbs_.Clear();
    meshThumbs_.Clear();
    textureThumbs_.Clear();
    skeletalThumbs_.Clear();
    root_ = Entry{};
    if (contentRoot_.empty()) {
        currentFolder_.clear();
        return;
    }
    root_ = BuildTree(contentRoot_);
    root_.name = fs::path(contentRoot_).filename().string();
    if (root_.name.empty() || root_.name == "." || root_.name == "..") {
        root_.name = "Content";
    }
    if (currentFolder_.empty()) {
        currentFolder_ = contentRoot_;
    } else {
        std::error_code ec;
        if (!fs::is_directory(currentFolder_, ec)) {
            currentFolder_ = contentRoot_;
        }
    }
}

const ContentBrowserPanel::Entry*
ContentBrowserPanel::FindEntryByPath(const Entry& node, const std::string& path) const {
    if (PathsEqualNormalized(node.path, path)) {
        return &node;
    }
    for (const Entry& child : node.children) {
        if (const Entry* found = FindEntryByPath(child, path)) {
            return found;
        }
    }
    return nullptr;
}

void ContentBrowserPanel::InvalidateLiveFolderListing() const {
    liveFolderPath_.clear();
    liveFolderChildren_.clear();
}

std::vector<ContentBrowserPanel::Entry> ContentBrowserPanel::CurrentFolderChildren() const {
    if (contentRoot_.empty() || currentFolder_.empty()) {
        return {};
    }
    std::error_code ec;
    if (!fs::is_directory(currentFolder_, ec) || ec) {
        return {};
    }
    if (!PathsEqualNormalized(liveFolderPath_, currentFolder_)) {
        liveFolderChildren_ = BuildTree(currentFolder_).children;
        liveFolderPath_ = NormalizePath(currentFolder_);
    }
    return liveFolderChildren_;
}

void ContentBrowserPanel::ActivateEntry(EditorContext& ctx, const Entry& entry, bool openFolder) {
    if (entry.isDirectory) {
        if (openFolder || viewMode_ == EViewMode::Contents) {
            currentFolder_ = entry.path;
        }
        return;
    }

    // DCC sources: open Import dialog with format auto-detected from the file.
    if (IsImportableSourcePath(entry.path)) {
        ctx.pendingImportSourcePath = entry.path;
        ctx.requestOpenImportDialog = true;
        return;
    }

    if (entry.kind == Entry::Kind::Level && ctx.engine != nullptr) {
        ctx.pendingOpenPath = entry.path;
        return;
    }
    if (entry.kind == Entry::Kind::Hdr) {
        if (ctx.requestPickHdr) {
            ctx.pendingHdrPickPath = MakePackRelativeAssetPath(ctx, entry.path);
        } else if (ctx.level != nullptr && ctx.resources != nullptr) {
            const std::string rel = MakePackRelativeAssetPath(ctx, entry.path);
            auto env = ctx.resources->LoadEnvMap(ResolveAssetPath(rel));
            if (!env) {
                env = ctx.resources->LoadEnvMap(entry.path);
            }
            if (env) {
                ctx.level->SetEnvironment(std::move(env));
                ctx.level->SetEnvironmentPath(rel);
                ctx.MarkDirty();
            }
        }
        return;
    }
    if (entry.kind == Entry::Kind::Material || entry.kind == Entry::Kind::MaterialGraph) {
        if (entry.kind == Entry::Kind::Material && ctx.requestPickMaterial) {
            ctx.pendingMaterialPickPath = entry.path;
            ctx.requestPickMaterial = false;
        } else {
            ctx.requestOpenMaterialPath = entry.path;
            ctx.showMaterialEditor = true;
        }
        ctx.previewAssetPath = entry.path;
        ctx.showAssetPreview = true;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::StaticMesh && ctx.requestPickMesh &&
        extLower(entry.path) == ".lmesh") {
        ctx.pendingMeshPickPath = entry.path;
        ctx.requestPickMesh = false;
        ctx.previewAssetPath = entry.path;
        ctx.showAssetPreview = true;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::StaticMesh) {
        // Double-click cooked mesh → Static Mesh Editor (Unreal-like dockable window).
        if (extLower(entry.path) == ".lmesh") {
            ctx.requestOpenMeshPreviewPath = entry.path;
            ctx.showStaticMeshEditor = true;
            return;
        }
        ctx.previewAssetPath = entry.path;
        ctx.showAssetPreview = true;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::Blueprint) {
        ctx.requestOpenBlueprintPath = entry.path;
        ctx.showBlueprintEditor = true;
        ctx.previewAssetPath = entry.path;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::UserWidget) {
        ctx.requestOpenWidgetPath = entry.path;
        ctx.showWidgetDesigner = true;
        ctx.previewAssetPath = entry.path;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::AnimMontage || entry.kind == Entry::Kind::AnimBlueprint) {
        EditorToast("No dedicated editor — preview in Asset Preview", EEditorToastKind::Info, 3.5f);
        ctx.previewAssetPath = entry.path;
        ctx.showAssetPreview = true;
        ctx.requestPreviewReload = true;
        return;
    }
    if (entry.kind == Entry::Kind::Skeleton) {
        ctx.previewAssetPath = entry.path;
        ctx.showAssetPreview = true;
        ctx.requestPreviewReload = true;
        return;
    }
    ctx.previewAssetPath = entry.path;
    ctx.showAssetPreview = true;
    ctx.requestPreviewReload = true;
}

void ContentBrowserPanel::DrawEntry(EditorContext& ctx, const Entry& entry) {
    ImGui::PushID(entry.path.c_str());
    const bool selected = PathsEqualNormalized(ctx.contentBrowserSelectedPath, entry.path);
    const float iconSz = ImGui::GetTextLineHeight();
    const float pad = 4.0f;

    if (entry.isDirectory) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding;
        if (selected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        // Leading space → FontSize label height (bare "##id" collapses and hides children).
        const bool open = ImGui::TreeNodeEx(" ##dir", flags);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
        const float yIcon = min.y + (max.y - min.y - iconSz) * 0.5f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        DrawLucideIcon(draw, ImVec2(x, yIcon), ImVec2(x + iconSz, yIcon + iconSz),
                       open ? ELucideIcon::FolderOpen : ELucideIcon::Folder,
                       IM_COL32(232, 188, 88, 255));
        const float yText = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f;
        draw->AddText(ImVec2(x + iconSz + pad, yText), ImGui::GetColorU32(ImGuiCol_Text),
                      entry.name.c_str());

        if (ImGui::IsItemClicked()) {
            ctx.contentBrowserSelectedPath = entry.path;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                currentFolder_ = entry.path;
                viewMode_ = EViewMode::Contents;
            }
        }
        AcceptFolderDropTarget(ctx, entry);
        DrawAssetItemContextMenu(ctx, entry);
        // Without IsItemActive, BeginDragDropSource steals the click and folders never expand.
        if (ImGui::IsItemActive() && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }
        if (open) {
            for (const Entry& child : entry.children) {
                DrawEntry(ctx, child);
            }
            ImGui::TreePop();
        }
    } else {
        const bool dirty = IsAssetPathDirty(ctx, entry);
        ImGuiTreeNodeFlags leafFlags =
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        if (selected) {
            leafFlags |= ImGuiTreeNodeFlags_Selected;
        }
        ImGui::TreeNodeEx(" ##leaf", leafFlags);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
        const float yIcon = min.y + (max.y - min.y - iconSz) * 0.5f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec4 tint = KindTint(entry.kind);
        DrawLucideIcon(draw, ImVec2(x, yIcon), ImVec2(x + iconSz, yIcon + iconSz),
                       LucideIconForContentKind(static_cast<int>(entry.kind)),
                       ImGui::ColorConvertFloat4ToU32(tint));
        const float yText = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f;
        const std::string label = entry.name + (dirty ? " *" : "");
        draw->AddText(ImVec2(x + iconSz + pad, yText), ImGui::GetColorU32(ImGuiCol_Text),
                      label.c_str());

        if (ImGui::IsItemClicked()) {
            ctx.contentBrowserSelectedPath = entry.path;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ActivateEntry(ctx, entry, false);
            } else if (entry.kind != Entry::Kind::Hdr && entry.kind != Entry::Kind::Skeleton &&
                       entry.kind != Entry::Kind::Level && entry.kind != Entry::Kind::Lightmap) {
                ctx.previewAssetPath = entry.path;
                ctx.requestPreviewReload = true;
            }
        }
        DrawAssetItemContextMenu(ctx, entry);
        if (ImGui::IsItemActive() && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }
    }
    ImGui::PopID();
}

void ContentBrowserPanel::DrawEngineLibrary(EditorContext& ctx) {
    constexpr ImGuiTreeNodeFlags kFolderFlags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
        ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding;
    if (!ImGui::TreeNodeEx(" ##engine_lib", kFolderFlags)) {
        return;
    }
    {
        const float iconSz = ImGui::GetTextLineHeight();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
        const float y = min.y + (max.y - min.y - iconSz) * 0.5f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        DrawLucideIcon(draw, ImVec2(x, y), ImVec2(x + iconSz, y + iconSz), ELucideIcon::Cpu,
                       IM_COL32(140, 180, 230, 255));
        draw->AddText(
            ImVec2(x + iconSz + 4.0f, min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text), "Engine");
    }

    const auto& items = EngineContentCatalog();
    auto drawLeaf = [&](const EngineContentItem& item) {
        ImGui::PushID(item.path.c_str());
        ELucideIcon icon = ELucideIcon::Box;
        ImU32 color = IM_COL32(180, 180, 190, 255);
        switch (item.kind) {
        case EngineContentItem::Kind::Primitive:
            icon = ELucideIcon::Box;
            color = IM_COL32(100, 210, 150, 255);
            break;
        case EngineContentItem::Kind::Material:
            icon = ELucideIcon::Layers;
            color = IM_COL32(180, 120, 220, 255);
            break;
        case EngineContentItem::Kind::Sky:
            icon = ELucideIcon::Sun;
            color = IM_COL32(80, 190, 220, 255);
            break;
        case EngineContentItem::Kind::Folder:
            icon = ELucideIcon::Folder;
            color = IM_COL32(232, 188, 88, 255);
            break;
        default:
            break;
        }
        constexpr ImGuiTreeNodeFlags kLeaf =
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
        ImGui::TreeNodeEx(" ##eng_leaf", kLeaf);
        const float iconSz = ImGui::GetTextLineHeight();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
        const float y = min.y + (max.y - min.y - iconSz) * 0.5f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        DrawLucideIcon(draw, ImVec2(x, y), ImVec2(x + iconSz, y + iconSz), icon, color);
        draw->AddText(
            ImVec2(x + iconSz + 4.0f, min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f),
            ImGui::GetColorU32(ImGuiCol_Text), item.name.c_str());
        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (item.kind == EngineContentItem::Kind::Material) {
                const char* matRel = item.path.ends_with("M_WorldGrid")
                                         ? "Materials/M_WorldGrid.lmat"
                                         : "Materials/M_Default.lmat";
                ctx.requestOpenMaterialPath = ResolveAssetPath(matRel);
                ctx.showMaterialEditor = true;
                ctx.previewAssetPath = ResolveAssetPath(matRel);
                ctx.requestPreviewReload = true;
            } else {
                std::string err;
                glm::vec3 pos = ctx.camera != nullptr ? ctx.camera->Target() : glm::vec3{0, 0, 0};
                if (ctx.camera != nullptr && ctx.camera->Mode() == ECameraMode::FreeLook) {
                    pos = ctx.camera->EyeLocation() + ctx.camera->ForwardVector() * 5.0f;
                    pos.y = std::max(pos.y, 0.0f);
                }
                if (!ApplyEngineContent(ctx, item.path, pos, err) && !err.empty()) {
                    std::cerr << "Engine content: " << err << '\n';
                }
            }
        }
        if (ImGui::IsItemActive() && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", item.path.c_str(), item.path.size() + 1);
            ImGui::TextUnformatted(item.name.c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
    };

    auto drawFolder = [&](const char* folderName, const char* pathPrefix) {
        ImGui::PushID(pathPrefix);
        if (!ImGui::TreeNodeEx(" ##eng_folder", kFolderFlags)) {
            ImGui::PopID();
            return;
        }
        {
            const float iconSz = ImGui::GetTextLineHeight();
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float x = min.x + ImGui::GetTreeNodeToLabelSpacing();
            const float y = min.y + (max.y - min.y - iconSz) * 0.5f;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            DrawLucideIcon(draw, ImVec2(x, y), ImVec2(x + iconSz, y + iconSz), ELucideIcon::Folder,
                           IM_COL32(232, 188, 88, 255));
            draw->AddText(ImVec2(x + iconSz + 4.0f,
                                 min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f),
                          ImGui::GetColorU32(ImGuiCol_Text), folderName);
        }
        for (const EngineContentItem& item : items) {
            if (item.kind == EngineContentItem::Kind::Folder) {
                continue;
            }
            if (item.path.starts_with(pathPrefix) && item.path != pathPrefix) {
                const std::string rest = item.path.substr(std::string(pathPrefix).size());
                if (rest.find('/') == std::string::npos) {
                    drawLeaf(item);
                }
            }
        }
        ImGui::TreePop();
        ImGui::PopID();
    };

    drawFolder("Basic Shapes", "leon:Engine/BasicShapes/");
    drawFolder("Materials", "leon:Engine/Materials/");
    drawFolder("HDR", "leon:Engine/Hdr/");

    ImGui::TreePop();
}

void ContentBrowserPanel::DrawBreadcrumbs(EditorContext& ctx) {
    (void)ctx;
    if (contentRoot_.empty()) {
        ImGui::TextDisabled("No project root");
        return;
    }

    if (ui::IconButton("##cb_root", ELucideIcon::Folder, ui::EUiVariant::Ghost, ui::EUiSize::Sm,
                       "Content root")) {
        currentFolder_ = contentRoot_;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (ui::Button("##cb_root_lbl", "Root", ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
        currentFolder_ = contentRoot_;
    }

    std::error_code ec;
    const fs::path rootPath = fs::path(contentRoot_).lexically_normal();
    const fs::path curPath = fs::path(currentFolder_).lexically_normal();
    fs::path relative = fs::relative(curPath, rootPath, ec);
    if (ec || relative.empty() || relative == ".") {
        return;
    }

    fs::path accum = rootPath;
    for (const fs::path& part : relative) {
        const std::string name = part.generic_string();
        if (name.empty() || name == ".") {
            continue;
        }
        accum /= part;
        ImGui::SameLine();
        ImGui::TextUnformatted("/");
        ImGui::SameLine();
        ImGui::PushID(accum.generic_string().c_str());
        if (ui::Button("##crumb", name.c_str(), ui::EUiVariant::Ghost, ui::EUiSize::Sm)) {
            currentFolder_ = accum.generic_string();
        }
        ImGui::PopID();
    }
}

namespace {

[[nodiscard]] bool EntryPassesKindFilter(ContentBrowserPanel::Entry::Kind kind,
                                         EContentBrowserKindFilter filter) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    switch (filter) {
    case EContentBrowserKindFilter::All:
        return true;
    case EContentBrowserKindFilter::StaticMeshes:
        return kind == Kind::StaticMesh || kind == Kind::FbxSource || kind == Kind::Folder;
    case EContentBrowserKindFilter::Materials:
        return kind == Kind::Material || kind == Kind::MaterialGraph || kind == Kind::Folder;
    case EContentBrowserKindFilter::Textures:
        return kind == Kind::Texture || kind == Kind::Folder;
    case EContentBrowserKindFilter::Blueprints:
        // Blueprints + User Widgets (UI) share this chip.
        return kind == Kind::Blueprint || kind == Kind::UserWidget || kind == Kind::Folder;
    case EContentBrowserKindFilter::Characters:
        return kind == Kind::Character || kind == Kind::SkelMesh || kind == Kind::Skeleton ||
               kind == Kind::Anim || kind == Kind::AnimMontage || kind == Kind::AnimBlueprint ||
               kind == Kind::PhysicsAsset || kind == Kind::Folder;
    case EContentBrowserKindFilter::Misc:
        return kind == Kind::Folder || kind == Kind::Other || kind == Kind::Level ||
               kind == Kind::FbxSource || kind == Kind::Hdr || kind == Kind::Lightmap ||
               kind == Kind::EnginePrimitive || kind == Kind::UserWidget;
    }
    return true;
}

[[nodiscard]] bool EntryHasVisibleDescendant(const ContentBrowserPanel::Entry& entry,
                                             EContentBrowserKindFilter filter) {
    using Kind = ContentBrowserPanel::Entry::Kind;
    if (filter == EContentBrowserKindFilter::All) {
        return true;
    }
    if (!entry.isDirectory && entry.kind != Kind::Folder) {
        return EntryPassesKindFilter(entry.kind, filter);
    }
    for (const ContentBrowserPanel::Entry& child : entry.children) {
        if (child.isDirectory || child.kind == Kind::Folder) {
            if (EntryHasVisibleDescendant(child, filter)) {
                return true;
            }
        } else if (EntryPassesKindFilter(child.kind, filter)) {
            return true;
        }
    }
    return false;
}

} // namespace

void ContentBrowserPanel::DrawContentsGrid(EditorContext& ctx) {
    SyncSelectionFolder(ctx);
    DrawBreadcrumbs(ctx);

    // Favorites (starred absolute paths) — above folder contents; respect kind + text filters.
    if (!ctx.contentFavoritePaths.empty()) {
        bool anyFavoriteVisible = false;
        for (std::size_t i = 0; i < ctx.contentFavoritePaths.size(); ++i) {
            const std::string& fav = ctx.contentFavoritePaths[i];
            const Entry::Kind favKind = ClassifyFile(fav);
            if (!EntryPassesKindFilter(favKind, ctx.contentKindFilter)) {
                continue;
            }
            const std::string label = fs::path(fav).filename().string();
            if (!ctx.contentFilter.empty() && label.find(ctx.contentFilter) == std::string::npos &&
                fav.find(ctx.contentFilter) == std::string::npos) {
                continue;
            }
            if (!anyFavoriteVisible) {
                ImGui::TextUnformatted("Favorites");
                anyFavoriteVisible = true;
            }
            ImGui::PushID(static_cast<int>(i) + 9000);
            if (ImGui::Selectable(label.c_str(), IsPathSelected(fav))) {
                SelectOnly(ctx, fav);
                currentFolder_ = fs::path(fav).parent_path().generic_string();
            }
            if (ImGui::BeginPopupContextItem("##fav_ctx")) {
                if (ImGui::MenuItem("Remove from Favorites")) {
                    ctx.ToggleContentFavorite(fav);
                }
                if (ImGui::MenuItem("Browse to Asset")) {
                    ctx.RevealInContentBrowser(fav);
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (anyFavoriteVisible) {
            ImGui::Separator();
        }
    }

    std::vector<Entry> children = CurrentFolderChildren();
    if (!ctx.contentFilter.empty()) {
        children.erase(
            std::remove_if(children.begin(), children.end(),
                           [&](const Entry& e) {
                               return e.name.find(ctx.contentFilter) == std::string::npos &&
                                      e.path.find(ctx.contentFilter) == std::string::npos;
                           }),
            children.end());
    }
    if (ctx.contentKindFilter != EContentBrowserKindFilter::All) {
        children.erase(
            std::remove_if(children.begin(), children.end(),
                           [&](const Entry& e) {
                               if (e.isDirectory || e.kind == Entry::Kind::Folder) {
                                   return !EntryHasVisibleDescendant(e, ctx.contentKindFilter);
                               }
                               return !EntryPassesKindFilter(e.kind, ctx.contentKindFilter);
                           }),
            children.end());
    }

    ImGui::SameLine();
    ImGui::TextDisabled("·");
    ImGui::SameLine();
    if (selectedPaths_.empty()) {
        ImGui::TextDisabled("%zu items", children.size());
    } else {
        ImGui::TextDisabled("%zu items  ·  %zu selected", children.size(), selectedPaths_.size());
    }
    ImGui::Separator();

    if (children.empty()) {
        if (ctx.contentKindFilter != EContentBrowserKindFilter::All) {
            ImGui::TextDisabled("(no assets match filter — click All)");
        } else {
            ImGui::TextDisabled("(empty folder — right-click to create)");
        }
        HandleMarquee(ctx, {});
        return;
    }

    constexpr float kTile = 96.0f;
    constexpr float kPad = 8.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int columns = std::max(1, static_cast<int>((avail + kPad) / (kTile + kPad)));
    int col = 0;
    // Spread thumb loads/renders across frames so opening a folder stays responsive.
    int thumbBudget = 4;
    std::vector<TileHit> tileHits;
    tileHits.reserve(children.size());

    for (Entry& entry : children) {
        if (entry.kind == Entry::Kind::Material || entry.kind == Entry::Kind::MaterialGraph) {
            EnsureMaterialSwatch(entry);
        }

        ImGui::PushID(entry.path.c_str());
        ImGui::BeginGroup();

        const ImVec2 tileSize(kTile, kTile);
        unsigned int thumbTex = 0;
        if (ctx.resources != nullptr) {
            if (entry.kind == Entry::Kind::Texture) {
                thumbTex = textureThumbs_.Ensure(*ctx.resources, entry.path, thumbBudget);
            } else if (ctx.renderer != nullptr) {
                if (entry.kind == Entry::Kind::Material ||
                    entry.kind == Entry::Kind::MaterialGraph) {
                    thumbTex = materialThumbs_.Ensure(*ctx.renderer, *ctx.resources, entry.path,
                                                      static_cast<int>(kTile), thumbBudget);
                } else if (entry.kind == Entry::Kind::StaticMesh) {
                    thumbTex = meshThumbs_.Ensure(*ctx.renderer, *ctx.resources, entry.path,
                                                  static_cast<int>(kTile), thumbBudget);
                } else if (entry.kind == Entry::Kind::SkelMesh ||
                           entry.kind == Entry::Kind::Character) {
                    thumbTex = skeletalThumbs_.Ensure(*ctx.renderer, *ctx.resources, entry.path,
                                                      static_cast<int>(kTile), thumbBudget);
                }
            }
        }

        // Click = select (Shift range / Ctrl toggle); double click = open.
        if (ImGui::InvisibleButton("##tile", tileSize)) {
            ApplyTileClick(ctx, entry, children);
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            SelectOnly(ctx, entry.path);
            ActivateEntry(ctx, entry, true);
        }
        DrawAssetItemContextMenu(ctx, entry);
        if (entry.isDirectory) {
            AcceptFolderDropTarget(ctx, entry);
        }
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("LEON_ASSET_PATH", entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        tileHits.push_back(TileHit{entry.path, min.x, min.y, max.x, max.y});

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const bool selected = IsPathSelected(entry.path);
        const ImU32 border =
            selected ? ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.75f, 0.25f, 1.0f))
                     : ImGui::ColorConvertFloat4ToU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        if (entry.isDirectory || entry.kind == Entry::Kind::Folder) {
            DrawFolderIcon(draw, min, max);
        } else if (thumbTex != 0) {
            draw->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(thumbTex)), min, max,
                           ImVec2(0, 1), ImVec2(1, 0));
        } else {
            ImVec4 tint = KindTint(entry.kind);
            if ((entry.kind == Entry::Kind::Material || entry.kind == Entry::Kind::MaterialGraph) &&
                entry.hasSwatch) {
                tint = ImVec4(entry.swatch[0], entry.swatch[1], entry.swatch[2], 1.0f);
            }
            // Dim plate + Lucide stroke glyph (Unreal Content Browser-style type icon).
            const ImVec4 plate{tint.x * 0.35f, tint.y * 0.35f, tint.z * 0.35f, 1.0f};
            draw->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(plate), 3.0f);
            const ImU32 stroke = ImGui::ColorConvertFloat4ToU32(ImVec4(
                std::min(1.0f, tint.x * 1.35f + 0.25f), std::min(1.0f, tint.y * 1.35f + 0.25f),
                std::min(1.0f, tint.z * 1.35f + 0.25f), 1.0f));
            DrawLucideIcon(draw, min, max, LucideIconForContentKind(static_cast<int>(entry.kind)),
                           stroke);
        }
        if (selected) {
            draw->AddRectFilled(min, max, IM_COL32(255, 200, 60, 40), 3.0f);
            draw->AddRect(min, max, border, 3.0f, 0, 2.0f);
        } else {
            draw->AddRect(min, max, border, 3.0f);
        }

        const bool dirty = IsAssetPathDirty(ctx, entry);
        const std::string caption = entry.name + (dirty ? " *" : "");
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + kTile);
        ImGui::TextUnformatted(caption.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
        ImGui::PopID();

        ++col;
        if (col < columns) {
            ImGui::SameLine(0.0f, kPad);
        } else {
            col = 0;
        }
    }

    HandleMarquee(ctx, tileHits);
}

void ContentBrowserPanel::DrawStatusFooter(const EditorContext& ctx) const {
    if (contentRoot_.empty()) {
        ui::Hint("No project open");
        return;
    }

    std::vector<Entry> children = CurrentFolderChildren();
    const int totalInFolder = static_cast<int>(children.size());
    if (!ctx.contentFilter.empty()) {
        children.erase(
            std::remove_if(children.begin(), children.end(),
                           [&](const Entry& e) {
                               return e.name.find(ctx.contentFilter) == std::string::npos &&
                                      e.path.find(ctx.contentFilter) == std::string::npos;
                           }),
            children.end());
    }

    int folders = 0;
    int assets = 0;
    for (const Entry& e : children) {
        if (e.isDirectory || e.kind == Entry::Kind::Folder) {
            ++folders;
        } else {
            ++assets;
        }
    }
    const int visible = static_cast<int>(children.size());

    const fs::path folderPath =
        currentFolder_.empty() ? fs::path(contentRoot_) : fs::path(currentFolder_);
    const std::string folderName = folderPath.filename().string();
    ImGui::TextDisabled("%s", folderName.empty() ? "Content" : folderName.c_str());
    ImGui::SameLine();
    if (!ctx.contentFilter.empty() && visible != totalInFolder) {
        ImGui::TextDisabled("· %d of %d items (%d folders, %d assets)", visible, totalInFolder,
                            folders, assets);
    } else {
        ImGui::TextDisabled("· %d items (%d folders, %d assets)", visible, folders, assets);
    }

    const std::size_t selectedCount = selectedPaths_.empty()
                                          ? (ctx.contentBrowserSelectedPath.empty() ? 0 : 1)
                                          : selectedPaths_.size();
    if (selectedCount > 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("· %zu selected", selectedCount);
    }
}

void ContentBrowserPanel::AcceptFolderDropTarget(EditorContext& ctx, const Entry& folderEntry) {
    if (!folderEntry.isDirectory || !IsEditablePackAsset(ctx, folderEntry.path)) {
        return;
    }
    if (!ImGui::BeginDragDropTarget()) {
        return;
    }
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
        const char* from = static_cast<const char*>(payload->Data);
        if (from != nullptr && from[0] != '\0' && IsEditablePackAsset(ctx, from)) {
            ctx.requestMoveAssetPath = from;
            ctx.requestMoveAssetDestFolder = folderEntry.path;
        }
    }
    ImGui::EndDragDropTarget();
}

void ContentBrowserPanel::Draw(EditorContext& ctx) {
    // Always keep the open-folder destination fresh (even if the panel is hidden).
    SyncRoot(ctx);
    if (ctx.requestContentRefresh || !scanned_) {
        Refresh(ctx);
        ctx.requestContentRefresh = false;
    }
    // Browse to Asset / RevealInContentBrowser
    if (!ctx.requestRevealContentPath.empty()) {
        std::error_code ec;
        fs::path reveal(ctx.requestRevealContentPath);
        if (!reveal.is_absolute()) {
            const std::string abs = ResolveAssetPath(ctx.requestRevealContentPath);
            if (!abs.empty()) {
                reveal = abs;
            }
        }
        if (fs::is_regular_file(reveal, ec) && !ec) {
            currentFolder_ = reveal.parent_path().generic_string();
            SelectOnly(ctx, reveal.generic_string());
            viewMode_ = EViewMode::Contents;
        } else if (fs::is_directory(reveal, ec) && !ec) {
            currentFolder_ = reveal.generic_string();
            viewMode_ = EViewMode::Contents;
        }
        ctx.requestRevealContentPath.clear();
        ctx.showContentBrowser = true;
    }
    ctx.contentBrowserFolder = currentFolder_.empty() ? contentRoot_ : currentFolder_;

    if (!ctx.showContentBrowser) {
        return;
    }
    if (!ui::BeginPanel("Content Browser", {.pOpen = &ctx.showContentBrowser})) {
        ctx.contentBrowserFocused = false;
        ui::EndPanel();
        return;
    }

    if (ctx.IsAssetPickActive()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ui::Tokens().accentHover);
        const char* pickHint = ctx.requestPickMaterial ? "Pick mode: click a Material (.lmat)"
                               : ctx.requestPickMesh   ? "Pick mode: click a Static Mesh"
                               : ctx.requestPickHdr    ? "Pick mode: click an HDR (.hdr)"
                                                       : "Pick mode: select an asset";
        ImGui::TextUnformatted(pickHint);
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ui::Button("##cancel_pick", "Cancel", ui::EUiVariant::Secondary, ui::EUiSize::Sm)) {
            ctx.ClearAssetPickMode();
        }
    }

    if (CbToolbarIconButton(ELucideIcon::Package, "##cb_import", "Import…",
                            "Import asset into the current Content folder")) {
        ctx.requestOpenImportDialog = true;
    }
    ImGui::SameLine();
    if (ui::IconButton("##cb_refresh", ELucideIcon::RefreshCw, ui::EUiVariant::Ghost,
                       ui::EUiSize::Sm, "Refresh")) {
        scanned_ = false;
        Refresh(ctx);
    }
    ImGui::SameLine();
    {
        const bool canSave = ctx.dirty || !ctx.dirtyMaterialPaths.empty() ||
                             !ctx.dirtyBlueprintPaths.empty() || !ctx.dirtyWidgetPaths.empty();
        if (CbToolbarIconButton(ELucideIcon::Save, "##cb_save", "Save",
                                "Save Asset: focused dirty Material / Blueprint / Widget, else "
                                "level (Ctrl+S)",
                                !canSave)) {
            ctx.requestSaveCurrent = true;
        }
        ImGui::SameLine();
        if (CbToolbarIconButton(ELucideIcon::Save, "##cb_save_all", "Save All",
                                "Save all dirty Materials, Blueprints, Widgets, and the level "
                                "(Ctrl+Shift+S)",
                                !canSave)) {
            ctx.requestSaveAll = true;
        }
    }
    ImGui::SameLine();
    {
        const bool hierarchy = viewMode_ == EViewMode::Hierarchy;
        if (CbToolbarIconButton(hierarchy ? ELucideIcon::Boxes : ELucideIcon::LayoutGrid,
                                "##cb_view", hierarchy ? "Hierarchy" : "Contents",
                                "Toggle folder tree vs flat contents with path navigation")) {
            viewMode_ =
                viewMode_ == EViewMode::Hierarchy ? EViewMode::Contents : EViewMode::Hierarchy;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", contentRoot_.empty() ? "(project)" : root_.name.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Right-click: Save All / Create. Asset RMB: Save Asset / Rename / Delete");
    }

    {
        char filterBuf[128];
        (void)std::snprintf(filterBuf, sizeof(filterBuf), "%s", ctx.contentFilter.c_str());
        if (ui::SearchField("##content_filter", filterBuf, sizeof(filterBuf), "Filter…")) {
            ctx.contentFilter = filterBuf;
        }
    }

    // Unreal-like Content Browser shortcuts when this window is focused.
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive()) {
        if (ImGui::IsKeyPressed(ImGuiKey_A) && ImGui::GetIO().KeyCtrl &&
            viewMode_ == EViewMode::Contents) {
            std::vector<Entry> children = CurrentFolderChildren();
            if (!ctx.contentFilter.empty()) {
                children.erase(
                    std::remove_if(children.begin(), children.end(),
                                   [&](const Entry& e) {
                                       return e.name.find(ctx.contentFilter) == std::string::npos &&
                                              e.path.find(ctx.contentFilter) == std::string::npos;
                                   }),
                    children.end());
            }
            SelectAllVisible(ctx, children);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F2) && selectedPaths_.size() == 1) {
            BeginRenameAsset(ctx, selectedPaths_.front());
        } else if (ImGui::IsKeyPressed(ImGuiKey_F2) && !ctx.contentBrowserSelectedPath.empty() &&
                   selectedPaths_.empty()) {
            BeginRenameAsset(ctx, ctx.contentBrowserSelectedPath);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_D) && ImGui::GetIO().KeyCtrl) {
            if (!selectedPaths_.empty()) {
                BeginDuplicatePaths(ctx, selectedPaths_);
            } else if (!ctx.contentBrowserSelectedPath.empty()) {
                BeginDuplicatePaths(ctx, {ctx.contentBrowserSelectedPath});
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (!selectedPaths_.empty()) {
                BeginDeletePaths(ctx, selectedPaths_);
            } else if (!ctx.contentBrowserSelectedPath.empty()) {
                BeginDeleteAsset(ctx, ctx.contentBrowserSelectedPath);
            }
        }
    }

    ImGui::Separator();
    const float footerH = ImGui::GetFrameHeight() + ui::Layout().panelPadding.y;
    if (ImGui::BeginChild("ContentBody", ImVec2(0, -footerH), false, ImGuiWindowFlags_NoMove)) {
        DrawCreateContextMenu(ctx);
        // Drop onto current folder (Contents view background).
        if (viewMode_ == EViewMode::Contents && !currentFolder_.empty() &&
            IsEditablePackAsset(ctx, currentFolder_)) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LEON_ASSET_PATH")) {
                    const char* from = static_cast<const char*>(payload->Data);
                    if (from != nullptr && from[0] != '\0' && IsEditablePackAsset(ctx, from)) {
                        ctx.requestMoveAssetPath = from;
                        ctx.requestMoveAssetDestFolder = currentFolder_;
                    }
                }
                ImGui::EndDragDropTarget();
            }
        }
        if (viewMode_ == EViewMode::Contents) {
            DrawContentsGrid(ctx);
        } else {
            DrawEngineLibrary(ctx);
            ImGui::Separator();
            ImGui::TextDisabled("Project");
            if (contentRoot_.empty()) {
                ImGui::TextDisabled("Open a project to browse pack content");
            } else {
                for (const Entry& child : root_.children) {
                    if (!ctx.contentFilter.empty()) {
                        const std::string needle = ctx.contentFilter;
                        auto matches = [&](auto&& self, const Entry& e) -> bool {
                            if (e.name.find(needle) != std::string::npos ||
                                e.path.find(needle) != std::string::npos) {
                                return true;
                            }
                            for (const Entry& c : e.children) {
                                if (self(self, c)) {
                                    return true;
                                }
                            }
                            return false;
                        };
                        if (!matches(matches, child)) {
                            continue;
                        }
                    }
                    DrawEntry(ctx, child);
                }
                if (root_.children.empty()) {
                    ImGui::TextDisabled("(empty pack — right-click to create)");
                }
            }
        }
    }
    ImGui::EndChild();

    DrawStatusFooter(ctx);

    DrawNewMaterialModal(ctx);
    DrawCreateInstanceFromModal(ctx);
    DrawNewMaterialGraphModal(ctx);
    DrawNewBlueprintModal(ctx);
    DrawNewWidgetModal(ctx);
    DrawNewFolderModal(ctx);
    DrawRenameAssetModal(ctx);
    DrawDeleteAssetsModal(ctx);
    DrawReferenceViewerModal(ctx);

    ctx.contentBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ui::EndPanel();
}

} // namespace leon::editor
