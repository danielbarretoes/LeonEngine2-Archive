#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <leon/core/LeonProjectConfig.h>
#include <leon/core/Paths.h>
#include <leon/editor/EditorCatalogs.h>
#include <leon/level/LeonLevelFormat.h>
#include <system_error>

namespace leon::editor {
namespace fs = std::filesystem;

namespace {

void AppendUniqueMode(std::vector<std::string>& modes, const std::string& id) {
    if (id.empty()) {
        return;
    }
    if (std::find(modes.begin(), modes.end(), id) != modes.end()) {
        return;
    }
    modes.push_back(id);
}

void AppendModesFromProjectConfig(std::vector<std::string>& modes, const fs::path& projectPath) {
    // Intent: expose pack GameMode registration ids in editor combos without scanning the DLL.
    LeonProjectSettings settings;
    std::string error;
    if (!LoadLeonProjectSettings(projectPath.string(), settings, error)) {
        return;
    }
    AppendUniqueMode(modes, settings.globalDefaultGameMode);
    for (const std::string& mode : settings.gameModes) {
        AppendUniqueMode(modes, mode);
    }
}

void AppendModesFromLevels(std::vector<std::string>& modes, const fs::path& projectPath) {
    const fs::path levelsDir = ProjectContentDirectory(projectPath) / "Levels";
    std::error_code ec;
    if (!fs::is_directory(levelsDir, ec)) {
        return;
    }
    for (const auto& it : fs::directory_iterator(levelsDir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (it.path().extension() != ".llev") {
            continue;
        }
        LevelDocument level;
        if (!LoadLeonLevelFile(it.path().string(), level)) {
            continue;
        }
        AppendUniqueMode(modes, level.gameMode);
    }
}

template <typename EntryT>
void DedupByPath(std::vector<EntryT>& out) {
    std::sort(out.begin(), out.end(), [](const EntryT& a, const EntryT& b) {
        if (a.displayName != b.displayName) {
            return a.displayName < b.displayName;
        }
        return a.authoringPath < b.authoringPath;
    });
    out.erase(std::unique(out.begin(), out.end(),
                          [](const EntryT& a, const EntryT& b) {
                              return a.authoringPath == b.authoringPath ||
                                     (!a.absolutePath.empty() && a.absolutePath == b.absolutePath);
                          }),
              out.end());
}

void AppendMaterialsFromDir(std::vector<EditorMaterialEntry>& out, const fs::path& dir,
                            const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (it.path().extension() != ".lmat") {
            continue;
        }
        EditorMaterialEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

void AppendHdrFromDir(std::vector<EditorSkyboxEntry>& out, const fs::path& dir,
                      const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        std::string ext = it.path().extension().string();
        for (char& c : ext) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (ext != ".hdr") {
            continue;
        }
        EditorSkyboxEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

void AppendEngineMaterial(std::vector<EditorMaterialEntry>& out, const char* authoringRel,
                          const char* displayName) {
    EditorMaterialEntry entry;
    entry.displayName = displayName;
    entry.authoringPath = authoringRel;
    entry.absolutePath = ResolveAssetPath(std::string("assets/") + authoringRel);
    if (entry.absolutePath.empty()) {
        entry.absolutePath = ResolveAssetPath(authoringRel);
    }
    out.push_back(std::move(entry));
}

void AppendEngineSkybox(std::vector<EditorSkyboxEntry>& out, const char* authoringRel,
                        const char* displayName) {
    EditorSkyboxEntry entry;
    entry.displayName = displayName;
    entry.authoringPath = authoringRel;
    entry.absolutePath = ResolveAssetPath(std::string("assets/") + authoringRel);
    if (entry.absolutePath.empty()) {
        entry.absolutePath = ResolveAssetPath(authoringRel);
    }
    out.push_back(std::move(entry));
}

void AppendBlueprintsFromDir(std::vector<EditorBlueprintEntry>& out, const fs::path& dir,
                             const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (it.path().extension() != ".lbp") {
            continue;
        }
        EditorBlueprintEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

[[nodiscard]] bool IsTextureExtension(std::string ext) {
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".ltx" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
           ext == ".bmp" || ext == ".exr";
}

void AppendTexturesFromDir(std::vector<EditorTextureEntry>& out, const fs::path& dir,
                           const fs::path& authoringRoot) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return;
    }
    for (const auto& it : fs::recursive_directory_iterator(dir, ec)) {
        if (ec || !it.is_regular_file()) {
            continue;
        }
        if (!IsTextureExtension(it.path().extension().string())) {
            continue;
        }
        EditorTextureEntry entry;
        entry.displayName = it.path().stem().string();
        entry.absolutePath = it.path().generic_string();
        std::error_code relEc;
        const fs::path rel = fs::relative(it.path(), authoringRoot, relEc);
        if (!relEc) {
            entry.authoringPath = rel.generic_string();
        } else {
            entry.authoringPath = entry.absolutePath;
        }
        out.push_back(std::move(entry));
    }
}

} // namespace

std::vector<std::string> ListEditorGameModes(const std::string& projectPath) {
    // Built-in editor previews. Pack-specific ids come from Config/*.ini / level overrides.
    std::vector<std::string> modes{"Default", "third-person"};
    if (!projectPath.empty()) {
        AppendModesFromProjectConfig(modes, fs::path(projectPath));
        AppendModesFromLevels(modes, fs::path(projectPath));
    }
    return modes;
}

std::vector<EditorMaterialEntry> CollectEditorMaterials(const std::string& projectPath) {
    std::vector<EditorMaterialEntry> out;
    AppendEngineMaterial(out, "Materials/M_Default.lmat", "M_Default");
    AppendEngineMaterial(out, "Materials/M_WorldGrid.lmat", "M_WorldGrid");
    AppendEngineMaterial(out, "Materials/M_SolidMetal.lmat", "M_SolidMetal");

    if (!projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(projectPath);
        AppendMaterialsFromDir(out, content / "Materials", content);
    }

    DedupByPath(out);
    return out;
}

std::vector<EditorSkyboxEntry> CollectEditorSkyboxes(const std::string& projectPath) {
    std::vector<EditorSkyboxEntry> out;
    AppendEngineSkybox(out, "Hdr/AutumnFieldPuresky1k.hdr", "DefaultSky");

    const std::string engineHdrDir = ResolveAssetPath("assets/Hdr");
    if (!engineHdrDir.empty()) {
        // Authoring paths relative to assets/ so level stores Hdr/foo.hdr.
        const fs::path assetsRoot = fs::path(engineHdrDir).parent_path();
        AppendHdrFromDir(out, engineHdrDir, assetsRoot);
    }

    if (!projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(projectPath);
        AppendHdrFromDir(out, content / "Hdr", content);
    }

    DedupByPath(out);
    return out;
}

std::vector<EditorBlueprintEntry> CollectEditorBlueprints(const std::string& projectPath) {
    std::vector<EditorBlueprintEntry> out;
    if (projectPath.empty()) {
        return out;
    }
    const fs::path content = ProjectContentDirectory(projectPath);
    AppendBlueprintsFromDir(out, content / "Blueprints", content);
    // Also pick up BP_*.lbp under other Content folders (e.g. Content/Maps/Blueprints).
    AppendBlueprintsFromDir(out, content, content);
    DedupByPath(out);
    return out;
}

std::vector<EditorTextureEntry> CollectEditorTextures(const std::string& projectPath) {
    std::vector<EditorTextureEntry> out;

    const std::string engineTexDir = ResolveAssetPath("assets/Textures");
    if (!engineTexDir.empty()) {
        const fs::path assetsRoot = fs::path(engineTexDir).parent_path();
        AppendTexturesFromDir(out, engineTexDir, assetsRoot);
    }

    if (!projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(projectPath);
        AppendTexturesFromDir(out, content, content);
    }

    DedupByPath(out);
    std::sort(out.begin(), out.end(), [](const EditorTextureEntry& a, const EditorTextureEntry& b) {
        return a.displayName < b.displayName;
    });
    return out;
}

} // namespace leon::editor
