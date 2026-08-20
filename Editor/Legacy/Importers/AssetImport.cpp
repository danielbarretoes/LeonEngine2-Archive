#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <leon/content/CookedSkeletal.h>
#include <leon/core/Ascii.h>
#include <leon/core/Paths.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorAssetPaths.h>
#include <leon/editor/EditorContext.h>
#include <leon/import/ImportNaming.h>
#include <leon/import/StaticMeshCook.h>
#include <leon/import/TextureMaterialCook.h>
#include <leon/level/Level.h>
#include <leon/render/ResourceCache.h>
#include <leon/render/Texture2DFormat.h>
#include <string>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

std::string stemFromPath(const std::string& path) {
    return fs::path(path).stem().string();
}

std::string sanitizeAssetName(std::string name) {
    if (name.empty()) {
        name = "Imported";
    }
    for (char& c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    return name;
}

[[nodiscard]] std::string extLower(const fs::path& path) {
    return AsciiToLower(path.extension().string());
}

/// Strip trailing _Idle / _Run / _Walk (and spaces) so Bot_Run → Bot.
[[nodiscard]] std::string baseCharacterName(const std::string& stem) {
    std::string lower = AsciiToLower(stem);
    const char* suffixes[] = {"_idle", "_run",  "_walk", "_jump", "_fall",
                              "_land", " idle", " run",  " walk"};
    for (const char* suf : suffixes) {
        const std::size_t n = std::char_traits<char>::length(suf);
        if (lower.size() > n && lower.compare(lower.size() - n, n, suf) == 0) {
            return stem.substr(0, stem.size() - n);
        }
    }
    return stem;
}

[[nodiscard]] std::string findFbxSibling(const fs::path& dir, const std::string& baseName,
                                         const char* suffix) {
    std::error_code ec;
    const fs::path candidate = dir / (baseName + suffix + ".fbx");
    if (fs::is_regular_file(candidate, ec) && !ec) {
        return candidate.generic_string();
    }
    const std::string want = AsciiToLower(baseName + suffix);
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (extLower(entry.path()) != ".fbx") {
            continue;
        }
        if (AsciiToLower(entry.path().stem().string()) == want) {
            return entry.path().generic_string();
        }
    }
    return {};
}

[[nodiscard]] bool hasFbxSibling(const fs::path& dir, const std::string& baseName,
                                 const char* suffix) {
    return !findFbxSibling(dir, baseName, suffix).empty();
}

/// Prefer `Base_Suffix.fbx`, then bare Mixamo-style stems (Running, JumpingUp, …).
[[nodiscard]] std::string findCharacterClipFbx(const fs::path& dir, const std::string& baseName,
                                               const char* baseSuffix,
                                               std::initializer_list<const char*> bareStems) {
    if (baseSuffix != nullptr && baseSuffix[0] != '\0') {
        const std::string fromSuffix = findFbxSibling(dir, baseName, baseSuffix);
        if (!fromSuffix.empty()) {
            return fromSuffix;
        }
    }
    std::error_code ec;
    for (const char* stem : bareStems) {
        const fs::path candidate = dir / (std::string(stem) + ".fbx");
        if (fs::is_regular_file(candidate, ec) && !ec) {
            return candidate.generic_string();
        }
    }
    // Case-insensitive fallback (single directory pass).
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec || !entry.is_regular_file() || extLower(entry.path()) != ".fbx") {
            continue;
        }
        const std::string stemLower = AsciiToLower(entry.path().stem().string());
        for (const char* stem : bareStems) {
            if (stemLower == AsciiToLower(stem)) {
                return entry.path().generic_string();
            }
        }
    }
    return {};
}

/// Pack Content root (Unreal-style). Fallback: staged Engine/Assets beside the exe.
[[nodiscard]] fs::path ImportContentRoot(const EditorContext& ctx) {
    if (!ctx.projectPath.empty()) {
        return ProjectContentDirectory(ctx.projectPath);
    }
    return fs::path(ResolveAssetPath("assets"));
}

/// Folder open in the Content Browser; `Raw/` → parent (character pack root).
[[nodiscard]] fs::path ImportDestinationFolder(const EditorContext& ctx) {
    std::error_code ec;
    fs::path folder;
    if (!ctx.contentBrowserFolder.empty() && fs::is_directory(ctx.contentBrowserFolder, ec) &&
        !ec) {
        folder = fs::path(ctx.contentBrowserFolder);
    } else {
        folder = ImportContentRoot(ctx);
    }
    if (!folder.empty() && AsciiToLower(folder.filename().string()) == "raw") {
        const fs::path parent = folder.parent_path();
        if (!parent.empty()) {
            return parent;
        }
    }
    return folder;
}

/// Cook into `<dest>/<Name>/`, or in-place when the open folder already is `<Name>`.
[[nodiscard]] fs::path ImportAssetDirectory(const EditorContext& ctx, const std::string& name,
                                            const std::string& destinationOverride) {
    if (!destinationOverride.empty()) {
        std::error_code ec;
        const fs::path overrideDir(destinationOverride);
        if (fs::is_directory(overrideDir, ec) && !ec) {
            return overrideDir;
        }
    }
    const fs::path parent = ImportDestinationFolder(ctx);
    if (!parent.empty() && AsciiToLower(parent.filename().string()) == AsciiToLower(name)) {
        return parent;
    }
    return parent / name;
}

[[nodiscard]] const char* ModeToString(EAssetImportMode mode) {
    switch (mode) {
    case EAssetImportMode::StaticObj:
        return "StaticObj";
    case EAssetImportMode::CharacterFbx:
        return "CharacterFbx";
    case EAssetImportMode::AnimFbx:
        return "AnimFbx";
    case EAssetImportMode::StaticFbx:
        return "StaticFbx";
    case EAssetImportMode::StaticGltf:
        return "StaticGltf";
    case EAssetImportMode::Texture2D:
        return "Texture2D";
    }
    return "StaticObj";
}

[[nodiscard]] bool ModeFromString(const std::string& text, EAssetImportMode& out) {
    const std::string key = AsciiToLower(text);
    if (key == "staticobj") {
        out = EAssetImportMode::StaticObj;
        return true;
    }
    if (key == "characterfbx") {
        out = EAssetImportMode::CharacterFbx;
        return true;
    }
    if (key == "animfbx") {
        out = EAssetImportMode::AnimFbx;
        return true;
    }
    if (key == "staticfbx") {
        out = EAssetImportMode::StaticFbx;
        return true;
    }
    if (key == "staticgltf") {
        out = EAssetImportMode::StaticGltf;
        return true;
    }
    if (key == "texture2d") {
        out = EAssetImportMode::Texture2D;
        return true;
    }
    return false;
}

[[nodiscard]] std::string ResolveStoredImportPath(const EditorContext& ctx,
                                                  const std::string& stored,
                                                  const std::string& besideDirectory = {}) {
    if (stored.empty()) {
        return {};
    }
    std::error_code ec;
    const fs::path p(stored);
    if (p.is_absolute() && fs::is_regular_file(p, ec) && !ec) {
        return p.generic_string();
    }
    // Prefer paths relative to the cooked asset folder (template sidecars / short names).
    if (!besideDirectory.empty()) {
        const fs::path beside = fs::path(besideDirectory) / p.filename();
        if (fs::is_regular_file(beside, ec) && !ec) {
            return beside.lexically_normal().generic_string();
        }
        const fs::path besideRel = fs::path(besideDirectory) / p;
        if (fs::is_regular_file(besideRel, ec) && !ec) {
            return besideRel.lexically_normal().generic_string();
        }
    }
    if (!ctx.projectPath.empty()) {
        const fs::path underContent = ProjectContentDirectory(ctx.projectPath) / p;
        if (fs::is_regular_file(underContent, ec) && !ec) {
            return underContent.lexically_normal().generic_string();
        }
        const fs::path underProject = fs::path(ctx.projectPath) / p;
        if (fs::is_regular_file(underProject, ec) && !ec) {
            return underProject.lexically_normal().generic_string();
        }
    }
    const std::string resolved = ResolveAssetPath(stored);
    if (!resolved.empty() && fs::is_regular_file(resolved, ec) && !ec) {
        return resolved;
    }
    return stored;
}

bool copyFileRequired(const fs::path& from, const fs::path& to, std::error_code& ec,
                      std::string& err) {
    if (!fs::is_regular_file(from, ec)) {
        err = "Source not found: " + from.generic_string();
        return false;
    }
    std::error_code sameEc;
    if (fs::equivalent(from, to, sameEc) && !sameEc) {
        return true;
    }
    fs::create_directories(to.parent_path(), ec);
    if (ec) {
        err = "Failed to create " + to.parent_path().generic_string() + ": " + ec.message();
        return false;
    }
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        err = "Failed to copy " + from.generic_string() + ": " + ec.message();
        return false;
    }
    return true;
}

/// Best-effort sibling copy; returns false when the source exists but copy fails.
bool copyFileOptional(const fs::path& from, const fs::path& to, std::error_code& ec,
                      int& warnCount) {
    if (!fs::is_regular_file(from, ec)) {
        return true;
    }
    fs::create_directories(to.parent_path(), ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        ++warnCount;
        std::cerr << "EditorImport: failed to copy dependency " << from.generic_string() << ": "
                  << ec.message() << '\n';
        return false;
    }
    return true;
}

void appendWarnSuffix(std::string& message, int warnCount) {
    if (warnCount > 0) {
        message += " (" + std::to_string(warnCount) + " dependency copy warning(s))";
    }
}

} // namespace

bool IsImportableSourcePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    if (!fs::is_regular_file(path, ec) || ec) {
        return false;
    }
    const std::string ext = extLower(fs::path(path));
    return ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".png" ||
           ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".exr";
}

EAssetImportMode GuessImportMode(const std::string& sourcePath) {
    const fs::path src(sourcePath);
    const std::string ext = extLower(src);
    if (ext == ".obj") {
        return EAssetImportMode::StaticObj;
    }
    if (ext == ".gltf" || ext == ".glb") {
        return EAssetImportMode::StaticGltf;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" ||
        ext == ".exr") {
        return EAssetImportMode::Texture2D;
    }
    if (ext != ".fbx") {
        return EAssetImportMode::StaticFbx;
    }

    const std::string stem = src.stem().string();
    const std::string base = baseCharacterName(stem);
    const fs::path dir = src.parent_path();
    const std::string lowerStem = AsciiToLower(stem);

    // Clip-named FBX next to a mesh → Animation (needs skeleton later).
    if (lowerStem.find("_idle") != std::string::npos ||
        lowerStem.find("_run") != std::string::npos ||
        lowerStem.find("_walk") != std::string::npos ||
        lowerStem.find("_jump") != std::string::npos ||
        lowerStem.find("_fall") != std::string::npos ||
        lowerStem.find("_land") != std::string::npos) {
        // Prefer Character when this looks like the pack mesh name is sibling-less clip set
        // but user double-clicked Idle: still Character if base mesh exists.
        const fs::path meshCandidate = dir / (base + ".fbx");
        std::error_code ec;
        if (fs::is_regular_file(meshCandidate, ec) && !ec) {
            return EAssetImportMode::CharacterFbx;
        }
        return EAssetImportMode::AnimFbx;
    }

    if (hasFbxSibling(dir, base, "_Idle") || hasFbxSibling(dir, base, "_Run") ||
        hasFbxSibling(dir, base, "_Walk")) {
        return EAssetImportMode::CharacterFbx;
    }
    return EAssetImportMode::StaticFbx;
}

void AutofillImportFromSource(const std::string& sourcePath, EAssetImportMode mode,
                              std::string& outAssetName, std::string& outSecondaryFbx,
                              std::string& outJumpStartFbx, std::string& outFallLoopFbx,
                              std::string& outLandFbx) {
    const fs::path src(sourcePath);
    const std::string stem = src.stem().string();
    outSecondaryFbx.clear();
    outJumpStartFbx.clear();
    outFallLoopFbx.clear();
    outLandFbx.clear();

    if (mode != EAssetImportMode::CharacterFbx) {
        outAssetName = sanitizeAssetName(stem);
        return;
    }

    const std::string base = sanitizeAssetName(baseCharacterName(stem));
    outAssetName = base;
    const fs::path dir = src.parent_path();
    outSecondaryFbx = findCharacterClipFbx(dir, base, "_Run", {"Running", "Run"});
    if (outSecondaryFbx.empty()) {
        outSecondaryFbx = findCharacterClipFbx(dir, base, "_Walk", {"Walking", "Walk"});
    }
    outJumpStartFbx = findCharacterClipFbx(dir, base, "_Jump", {"JumpingUp", "Jump"});
    outFallLoopFbx = findCharacterClipFbx(dir, base, "_Fall", {"FallingIdle", "Fall"});
    outLandFbx = findCharacterClipFbx(dir, base, "_Land", {"FallingToLanding", "Land", "Landing"});
}

std::string ImportSidecarPathForAsset(const std::string& cookedAssetPath) {
    if (cookedAssetPath.empty()) {
        return {};
    }
    const fs::path cooked(cookedAssetPath);
    return (cooked.parent_path() / (cooked.stem().string() + ".leonimport")).generic_string();
}

bool LoadImportSidecar(const std::string& sidecarPath, AssetImportSidecar& out) {
    out = {};
    std::ifstream in(sidecarPath);
    if (!in) {
        return false;
    }
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        if (const auto hash = line.find('#'); hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.front()))) {
            line.erase(line.begin());
        }
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = AsciiToLower(line.substr(1, line.size() - 2));
            continue;
        }
        if (section != "import") {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = AsciiToLower(line.substr(0, eq));
        std::string value = line.substr(eq + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) {
            key.pop_back();
        }
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
        if (key == "mode") {
            (void)ModeFromString(value, out.mode);
        } else if (key == "sourcepath") {
            out.sourcePath = value;
        } else if (key == "secondarypath") {
            out.secondaryFbxPath = value;
        } else if (key == "jumppath" || key == "jumpstartpath") {
            out.jumpStartFbxPath = value;
        } else if (key == "fallpath" || key == "falllooppath") {
            out.fallLoopFbxPath = value;
        } else if (key == "landpath") {
            out.landFbxPath = value;
        } else if (key == "skeletonpath") {
            out.skeletonJsonPath = value;
        } else if (key == "assetname") {
            out.assetName = value;
        } else if (key == "animlooping") {
            out.animLooping = (AsciiToLower(value) != "false" && value != "0");
        } else if (key == "uniformscale") {
            try {
                out.uniformScale = std::stof(value);
            } catch (...) {
                out.uniformScale = 1.0f;
            }
        } else if (key == "generatecollision") {
            out.generateCollision = (AsciiToLower(value) == "true" || value == "1");
        }
    }
    return !out.sourcePath.empty();
}

bool SaveImportSidecar(const EditorContext& ctx, const std::string& cookedAssetPath,
                       const AssetImportRequest& request) {
    const std::string sidecarPath = ImportSidecarPathForAsset(cookedAssetPath);
    if (sidecarPath.empty()) {
        return false;
    }
    std::ofstream out(sidecarPath, std::ios::trunc);
    if (!out) {
        return false;
    }
    const std::string sourceRel = MakePackRelativeAssetPath(ctx, request.sourcePath);
    const std::string secondaryRel = request.secondaryFbxPath.empty()
                                         ? std::string{}
                                         : MakePackRelativeAssetPath(ctx, request.secondaryFbxPath);
    const std::string jumpRel = request.jumpStartFbxPath.empty()
                                    ? std::string{}
                                    : MakePackRelativeAssetPath(ctx, request.jumpStartFbxPath);
    const std::string fallRel = request.fallLoopFbxPath.empty()
                                    ? std::string{}
                                    : MakePackRelativeAssetPath(ctx, request.fallLoopFbxPath);
    const std::string landRel = request.landFbxPath.empty()
                                    ? std::string{}
                                    : MakePackRelativeAssetPath(ctx, request.landFbxPath);
    const std::string skeletonRel = request.skeletonJsonPath.empty()
                                        ? std::string{}
                                        : MakePackRelativeAssetPath(ctx, request.skeletonJsonPath);
    out << "# Leon Import sidecar (.leonimport) — Unreal-like reimport source\n\n";
    out << "[Import]\n";
    out << "Mode=" << ModeToString(request.mode) << "\n";
    out << "SourcePath=" << (sourceRel.empty() ? request.sourcePath : sourceRel) << "\n";
    if (!secondaryRel.empty() || !request.secondaryFbxPath.empty()) {
        out << "SecondaryPath=" << (secondaryRel.empty() ? request.secondaryFbxPath : secondaryRel)
            << "\n";
    }
    if (!jumpRel.empty() || !request.jumpStartFbxPath.empty()) {
        out << "JumpPath=" << (jumpRel.empty() ? request.jumpStartFbxPath : jumpRel) << "\n";
    }
    if (!fallRel.empty() || !request.fallLoopFbxPath.empty()) {
        out << "FallPath=" << (fallRel.empty() ? request.fallLoopFbxPath : fallRel) << "\n";
    }
    if (!landRel.empty() || !request.landFbxPath.empty()) {
        out << "LandPath=" << (landRel.empty() ? request.landFbxPath : landRel) << "\n";
    }
    if (!skeletonRel.empty() || !request.skeletonJsonPath.empty()) {
        out << "SkeletonPath=" << (skeletonRel.empty() ? request.skeletonJsonPath : skeletonRel)
            << "\n";
    }
    if (!request.assetName.empty()) {
        out << "AssetName=" << request.assetName << "\n";
    }
    out << "AnimLooping=" << (request.animLooping ? "true" : "false") << "\n";
    out << "UniformScale=" << request.uniformScale << "\n";
    out << "GenerateCollision=" << (request.generateCollision ? "true" : "false") << "\n";
    return static_cast<bool>(out);
}

bool HasImportSidecar(const std::string& cookedAssetPath) {
    std::error_code ec;
    const std::string path = ImportSidecarPathForAsset(cookedAssetPath);
    return !path.empty() && fs::is_regular_file(path, ec) && !ec;
}

bool EditorReimportAsset(EditorContext& ctx, const std::string& cookedAssetPath,
                         AssetImportResult& out) {
    out = {};
    AssetImportSidecar side;
    const std::string sidecarPath = ImportSidecarPathForAsset(cookedAssetPath);
    if (!LoadImportSidecar(sidecarPath, side)) {
        out.message = "No .leonimport sidecar for " + cookedAssetPath;
        return false;
    }

    AssetImportRequest req;
    req.mode = side.mode;
    const std::string besideDir = fs::path(cookedAssetPath).parent_path().generic_string();
    req.sourcePath = ResolveStoredImportPath(ctx, side.sourcePath, besideDir);
    req.secondaryFbxPath = ResolveStoredImportPath(ctx, side.secondaryFbxPath, besideDir);
    req.jumpStartFbxPath = ResolveStoredImportPath(ctx, side.jumpStartFbxPath, besideDir);
    req.fallLoopFbxPath = ResolveStoredImportPath(ctx, side.fallLoopFbxPath, besideDir);
    req.landFbxPath = ResolveStoredImportPath(ctx, side.landFbxPath, besideDir);
    req.skeletonJsonPath = ResolveStoredImportPath(ctx, side.skeletonJsonPath, besideDir);
    req.assetName = side.assetName;
    req.animLooping = side.animLooping;
    req.uniformScale = side.uniformScale;
    req.generateCollision = side.generateCollision;
    req.destinationFolderOverride = fs::path(cookedAssetPath).parent_path().generic_string();

    if (!fs::is_regular_file(req.sourcePath)) {
        out.message = "Reimport source missing: " + side.sourcePath;
        return false;
    }
    if (!EditorImportAsset(ctx, req, out)) {
        return false;
    }

    // Unreal-like Reimport: drop GPU/CPU cache so the open level shows the new cook.
    const std::string preview = out.previewPath.empty() ? cookedAssetPath : out.previewPath;
    const std::string ext = extLower(preview);
    if (ctx.resources != nullptr) {
        if (ext == ".lmesh") {
            ctx.resources->InvalidateStaticMesh(preview);
            if (!PathsEqualNormalized(preview, cookedAssetPath)) {
                ctx.resources->InvalidateStaticMesh(cookedAssetPath);
            }
            if (ctx.level != nullptr) {
                const auto referencesCooked = [&](const std::string& meshPath) {
                    if (meshPath.empty()) {
                        return false;
                    }
                    if (PathsEqualNormalized(meshPath, preview) ||
                        PathsEqualNormalized(meshPath, cookedAssetPath)) {
                        return true;
                    }
                    const std::string abs = ResolveAssetPath(meshPath);
                    return PathsEqualNormalized(abs, preview) ||
                           PathsEqualNormalized(abs, cookedAssetPath);
                };
                for (StaticMeshComponent& mesh : ctx.level->StaticMeshes()) {
                    if (!referencesCooked(mesh.meshPath)) {
                        continue;
                    }
                    const std::string abs = ResolveAssetPath(mesh.meshPath);
                    auto loaded = ctx.resources->LoadStaticMesh(abs.empty() ? preview : abs);
                    if ((loaded == nullptr || !loaded->Valid()) && !abs.empty()) {
                        loaded = ctx.resources->LoadStaticMesh(preview);
                    }
                    if (loaded != nullptr && loaded->Valid()) {
                        mesh.mesh = std::move(loaded);
                    }
                }
            }
        } else if (ext == ".ltx") {
            ctx.resources->InvalidateTexture(preview);
            if (!PathsEqualNormalized(preview, cookedAssetPath)) {
                ctx.resources->InvalidateTexture(cookedAssetPath);
            }
            // Materials cache texture shared_ptrs; force reload on next bind.
            ctx.resources->InvalidateAllMaterials();
        }
    }

    out.message = "Reimported → " + out.previewPath;
    return true;
}

bool EditorImportAsset(EditorContext& ctx, const AssetImportRequest& request,
                       AssetImportResult& out) {
    out = {};
    if (request.sourcePath.empty()) {
        out.message = "Source path is empty";
        return false;
    }
    if (!fs::is_regular_file(request.sourcePath)) {
        out.message = "Source file not found: " + request.sourcePath;
        return false;
    }

    std::string name = sanitizeAssetName(
        request.assetName.empty() ? stemFromPath(request.sourcePath) : request.assetName);
    if (request.mode == EAssetImportMode::CharacterFbx) {
        name = sanitizeAssetName(baseCharacterName(name));
    }
    // Unreal-like: Content Browser folder (Raw/ → parent), or Reimport override folder.
    const fs::path destDir = ImportAssetDirectory(ctx, name, request.destinationFolderOverride);

    const auto finishOk = [&](std::string preview, std::string msg) -> bool {
        out.ok = true;
        out.previewPath = std::move(preview);
        out.cookedPath = out.previewPath;
        out.importFolder = fs::path(out.previewPath).parent_path().generic_string();
        out.message = std::move(msg);
        if (!SaveImportSidecar(ctx, out.previewPath, request)) {
            std::cerr << "EditorImport: failed to write .leonimport for " << out.previewPath
                      << '\n';
        }
        const std::string ext = extLower(out.previewPath);
        if (ext == ".lmesh") {
            ctx.requestRevealContentPath = out.previewPath;
            ctx.contentBrowserSelectedPath = out.previewPath;
            ctx.requestOpenMeshPreviewPath = out.previewPath;
            ctx.showStaticMeshEditor = true;
        } else {
            ctx.requestRevealContentPath = out.previewPath;
            ctx.contentBrowserSelectedPath = out.previewPath;
        }
        ctx.requestContentRefresh = true;
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    };

    if (request.mode == EAssetImportMode::Texture2D) {
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }
        const fs::path outLtx = destDir / (name + ".ltx");
        // Heuristic: Normal / ORM / mask filenames → linear; else sRGB (Unreal-like).
        const std::string lowerName = AsciiToLower(name);
        const bool sRGB = lowerName.find("_n") == std::string::npos &&
                          lowerName.find("_normal") == std::string::npos &&
                          lowerName.find("_orm") == std::string::npos &&
                          lowerName.find("_mrao") == std::string::npos &&
                          lowerName.find("_mask") == std::string::npos;
        std::string cookErr;
        if (!CookTexture2DFromImageFile(request.sourcePath, outLtx.generic_string(), sRGB,
                                        &cookErr)) {
            out.message = "Texture2D cook failed: " + cookErr;
            return false;
        }
        std::string materialInstancePath;
        const bool isDataMap = !sRGB;
        if (!EmitMaterialsForImportedTexture(destDir.generic_string(), name,
                                             outLtx.generic_string(), isDataMap,
                                             materialInstancePath)) {
            out.message = "Texture2D cooked but material emit failed for " + name;
            return false;
        }
        return finishOk(outLtx.generic_string(),
                        "Cooked Texture2D → " + outLtx.generic_string() +
                            (sRGB ? " (sRGB)" : " (linear)") + " + " +
                            fs::path(materialInstancePath).filename().string());
    }

    if (request.mode == EAssetImportMode::StaticObj) {
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const fs::path destObj = destDir / (name + ".obj");
        std::string copyErr;
        if (!copyFileRequired(src, destObj, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }

        int warnCount = 0;
        const fs::path mtlSrc = src.parent_path() / (src.stem().string() + ".mtl");
        const fs::path mtlDst = destDir / (name + ".mtl");
        (void)copyFileOptional(mtlSrc, mtlDst, ec, warnCount);
        // Keep source stem MTL beside renamed OBJ so mtllib references still resolve.
        if (src.stem() != fs::path(name)) {
            (void)copyFileOptional(mtlSrc, destDir / (src.stem().string() + ".mtl"), ec, warnCount);
        }

        for (const auto& entry : fs::directory_iterator(src.parent_path(), ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            const auto ext = entry.path().extension().string();
            std::string lower = ext;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lower == ".png" || lower == ".jpg" || lower == ".jpeg" || lower == ".tga" ||
                lower == ".bmp" || lower == ".exr") {
                (void)copyFileOptional(entry.path(), destDir / entry.path().filename(), ec,
                                       warnCount);
            }
        }

        const fs::path destLmesh = destDir / (StaticMeshFileStem(name) + ".lmesh");
        std::string cookErr;
        if (!CookStaticMeshFromObj(destObj.generic_string(), destLmesh.generic_string(), cookErr,
                                   request.uniformScale)) {
            out.message = "OBJ copied but .lmesh cook failed: " + cookErr;
            appendWarnSuffix(out.message, warnCount);
            std::cerr << "EditorImport: " << out.message << '\n';
            out.ok = false;
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        std::string msg = "Imported OBJ → cooked " + destLmesh.generic_string();
        appendWarnSuffix(msg, warnCount);
        return finishOk(destLmesh.generic_string(), std::move(msg));
    }

    if (request.mode == EAssetImportMode::StaticFbx) {
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const fs::path destFbx = destDir / (name + ".fbx");
        std::string copyErr;
        if (!copyFileRequired(src, destFbx, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }

        const fs::path destLmesh = destDir / (StaticMeshFileStem(name) + ".lmesh");
        std::string cookErr;
        if (!CookStaticMeshFromFbx(destFbx.generic_string(), destLmesh.generic_string(), cookErr,
                                   request.uniformScale)) {
            out.message = "FBX copied but .lmesh cook failed: " + cookErr;
            std::cerr << "EditorImport: " << out.message << '\n';
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        return finishOk(destLmesh.generic_string(),
                        "Imported FBX → cooked " + destLmesh.generic_string());
    }

    if (request.mode == EAssetImportMode::StaticGltf) {
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) {
            out.message = "Failed to create " + destDir.generic_string();
            return false;
        }

        const fs::path src = request.sourcePath;
        const std::string ext = src.extension().string();
        const fs::path destGltf = destDir / (name + ext);
        std::string copyErr;
        if (!copyFileRequired(src, destGltf, ec, copyErr)) {
            out.message = copyErr;
            return false;
        }
        int warnCount = 0;
        for (const auto& entry : fs::directory_iterator(src.parent_path(), ec)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto e = entry.path().extension().string();
            for (char& c : e) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (e == ".bin" || e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".ktx" ||
                e == ".webp") {
                (void)copyFileOptional(entry.path(), destDir / entry.path().filename(), ec,
                                       warnCount);
            }
        }

        const fs::path destLmesh = destDir / (StaticMeshFileStem(name) + ".lmesh");
        const fs::path matsDir = destDir / "Materials";
        std::string cookErr;
        if (!CookStaticMeshFromGltf(destGltf.generic_string(), destLmesh.generic_string(),
                                    matsDir.generic_string(), cookErr, request.uniformScale)) {
            out.message = "glTF cook failed: " + cookErr;
            appendWarnSuffix(out.message, warnCount);
            std::cerr << "EditorImport: " << out.message << '\n';
            return false;
        }

        if (ctx.resources != nullptr) {
            (void)ctx.resources->LoadStaticMesh(destLmesh.generic_string());
        }

        std::string msg = "Imported glTF → cooked " + destLmesh.generic_string();
        appendWarnSuffix(msg, warnCount);
        return finishOk(destLmesh.generic_string(), std::move(msg));
    }

    if (request.mode == EAssetImportMode::CharacterFbx) {
        std::error_code ec;
        fs::create_directories(destDir, ec);

        // Prefer BaseName.fbx as skinned mesh when the user picked Idle/Run/Walk.
        std::string meshFbx = request.sourcePath;
        {
            const fs::path src(request.sourcePath);
            const fs::path meshCandidate = src.parent_path() / (name + ".fbx");
            if (fs::is_regular_file(meshCandidate, ec) && !ec) {
                meshFbx = meshCandidate.generic_string();
            }
        }
        std::string runFbx = request.secondaryFbxPath;
        if (runFbx.empty()) {
            runFbx = findCharacterClipFbx(fs::path(meshFbx).parent_path(), name, "_Run",
                                          {"Running", "Run"});
        }
        if (runFbx.empty()) {
            runFbx = meshFbx;
        }

        CookJumpAnimPaths jumpAnims{};
        jumpAnims.jumpStartFbx = request.jumpStartFbxPath;
        jumpAnims.fallLoopFbx = request.fallLoopFbxPath;
        jumpAnims.landFbx = request.landFbxPath;
        if (jumpAnims.jumpStartFbx.empty() || jumpAnims.fallLoopFbx.empty() ||
            jumpAnims.landFbx.empty()) {
            const fs::path meshDir = fs::path(meshFbx).parent_path();
            if (jumpAnims.jumpStartFbx.empty()) {
                jumpAnims.jumpStartFbx =
                    findCharacterClipFbx(meshDir, name, "_Jump", {"JumpingUp", "Jump"});
            }
            if (jumpAnims.fallLoopFbx.empty()) {
                jumpAnims.fallLoopFbx =
                    findCharacterClipFbx(meshDir, name, "_Fall", {"FallingIdle", "Fall"});
            }
            if (jumpAnims.landFbx.empty()) {
                jumpAnims.landFbx = findCharacterClipFbx(meshDir, name, "_Land",
                                                         {"FallingToLanding", "Land", "Landing"});
            }
        }

        if (!CookCharacterFromFbx(name, meshFbx, runFbx, destDir.generic_string(), jumpAnims)) {
            out.message = "CookCharacterFromFbx failed (need skinned FBX with skeleton)";
            return false;
        }

        // Extra locomotion clips next to the mesh (e.g. _Walk) → Anims/.
        const fs::path skelPath = destDir / (name + ".lskel");
        int extraAnims = 0;
        for (const char* suf : {"_Walk", "_Idle"}) {
            const std::string clipFbx = findFbxSibling(fs::path(meshFbx).parent_path(), name, suf);
            if (clipFbx.empty() || !fs::is_regular_file(skelPath, ec) || ec) {
                continue;
            }
            // Skip Idle when it was already baked from the mesh cook as BreathingIdle.
            if (std::string(suf) == "_Idle") {
                continue;
            }
            const std::string clipName = sanitizeAssetName(name + suf);
            const fs::path outAnim = destDir / "Anims" / (clipName + ".lanim");
            fs::create_directories(outAnim.parent_path(), ec);
            if (CookAnimSequenceFromFbx(clipFbx, skelPath.generic_string(),
                                        outAnim.generic_string(), clipName, true)) {
                ++extraAnims;
            }
        }

        const fs::path characterAsset = destDir / (name + ".lchar");
        std::string msg = "Cooked character → " + characterAsset.generic_string();
        if (extraAnims > 0) {
            msg += " (+" + std::to_string(extraAnims) + " extra anim clip(s))";
        }
        if (!jumpAnims.jumpStartFbx.empty() || !jumpAnims.fallLoopFbx.empty() ||
            !jumpAnims.landFbx.empty()) {
            msg += " (jump clips)";
        }
        // Prefer storing the mesh FBX as the reimport source.
        AssetImportRequest sideReq = request;
        sideReq.sourcePath = meshFbx;
        sideReq.secondaryFbxPath = runFbx;
        sideReq.jumpStartFbxPath = jumpAnims.jumpStartFbx;
        sideReq.fallLoopFbxPath = jumpAnims.fallLoopFbx;
        sideReq.landFbxPath = jumpAnims.landFbx;
        sideReq.assetName = name;
        out.ok = true;
        out.previewPath = characterAsset.generic_string();
        out.message = std::move(msg);
        if (!SaveImportSidecar(ctx, out.previewPath, sideReq)) {
            std::cerr << "EditorImport: failed to write .leonimport for " << out.previewPath
                      << '\n';
        }
        std::cout << "EditorImport: " << out.message << '\n';
        return true;
    }

    if (request.mode == EAssetImportMode::AnimFbx) {
        if (request.skeletonJsonPath.empty() || !fs::is_regular_file(request.skeletonJsonPath)) {
            out.message = "Anim import requires a valid *.lskel";
            return false;
        }
        const fs::path skelPath = request.skeletonJsonPath;
        const fs::path animDir = skelPath.parent_path() / "Anims";
        std::error_code ec;
        fs::create_directories(animDir, ec);
        const fs::path outAnim = animDir / (name + ".lanim");

        if (!CookAnimSequenceFromFbx(request.sourcePath, request.skeletonJsonPath,
                                     outAnim.generic_string(), name, request.animLooping)) {
            out.message = "CookAnimSequenceFromFbx failed";
            return false;
        }

        bool updatedBlendspace = false;
        const fs::path characterDir = skelPath.parent_path();
        fs::path blendspacePath;
        std::error_code findEc;
        for (const auto& entry : fs::directory_iterator(characterDir, findEc)) {
            if (findEc || !entry.is_regular_file()) {
                continue;
            }
            const std::string fname = entry.path().filename().string();
            const std::string fext = entry.path().extension().string();
            if (fext == ".lchar") {
                CharacterVisualDesc character;
                if (LoadCharacterVisualLchar(entry.path().generic_string(), character) &&
                    !character.blendSpaceRel.empty()) {
                    blendspacePath = characterDir / character.blendSpaceRel;
                    break;
                }
            }
        }
        if (blendspacePath.empty()) {
            for (const auto& entry : fs::directory_iterator(characterDir, findEc)) {
                if (findEc || !entry.is_regular_file()) {
                    continue;
                }
                const std::string fname = entry.path().filename().string();
                if (fname.find("Locomotion") != std::string::npos &&
                    fname.find(".blendspace1d.json") != std::string::npos) {
                    blendspacePath = entry.path();
                    break;
                }
            }
        }

        if (!blendspacePath.empty() && fs::is_regular_file(blendspacePath, findEc) && !findEc) {
            BlendSpace1DAssetDesc bs;
            if (LoadBlendSpace1DJson(blendspacePath.generic_string(), bs)) {
                std::error_code relEc;
                const fs::path animRel = fs::relative(outAnim, blendspacePath.parent_path(), relEc);
                const std::string animRelStr =
                    (!relEc && !animRel.empty())
                        ? animRel.generic_string()
                        : (fs::path("Anims") / (name + ".lanim")).generic_string();
                bool alreadyPresent = false;
                for (const auto& sample : bs.samples) {
                    if (sample.animRelPath == animRelStr) {
                        alreadyPresent = true;
                        break;
                    }
                }
                if (!alreadyPresent) {
                    float position = 0.5f;
                    if (!bs.samples.empty()) {
                        position = bs.samples.back().position + 0.1f;
                    }
                    bs.samples.push_back({animRelStr, position});
                    if (SaveBlendSpace1DJson(blendspacePath.generic_string(), bs)) {
                        updatedBlendspace = true;
                    }
                }
            }
        }

        std::string msg = "Cooked anim → " + outAnim.generic_string();
        if (updatedBlendspace) {
            msg += " (updated blendspace)";
        }
        return finishOk(outAnim.generic_string(), std::move(msg));
    }

    out.message = "Unknown import mode";
    return false;
}

} // namespace leon::editor
