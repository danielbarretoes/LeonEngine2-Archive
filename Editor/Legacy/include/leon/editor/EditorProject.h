#pragma once

#include <filesystem>
#include <leon/render/Scalability.h>
#include <string>
#include <vector>

namespace leon::editor {

struct RecentProject {
    std::string path;
    std::string name;
    std::string displayName;
    /// Engine version that last opened this project (empty if unknown).
    std::string engineVersion;
};

struct EditorProjectInfo {
    std::string path;
    std::string name;
    std::string displayName;
    /// Last engine version written into Config/DefaultGame.ini (may be empty).
    std::string engineVersion;
    /// Shipping OpenLevel (`GameDefaultMap` in DefaultEngine.ini).
    /// Authoring key like `Levels/MainMenu.llev`. Empty → first catalog entry at runtime.
    std::string gameDefaultMap;
    /// Map loaded when the Editor opens this project (`EditorStartupMap`). Empty → fall back to
    /// `gameDefaultMap`, then first catalog entry.
    std::string editorStartupMap;
    /// Pack GameMode when a level has no override (`GlobalDefaultGameMode`).
    std::string globalDefaultGameMode;
    /// Content library id (`Templates/<id>/`) when created via New Project.
    std::string templateId;
    std::string description;
    /// When true, Build Game also builds `leon-<Name>-server` (headless dedicated).
    bool buildDedicatedServer = false;
    /// When true, DefaultScalability.ini was present.
    bool hasEditorScalability = false;
    /// Project-scoped editor scalability (DefaultScalability.ini).
    EditorScalabilitySettings editorScalability{};
};

/// Project hub helpers: recent list, open/create. Templates/ are Content libraries
/// (Blank, ThirdPerson) merged into a scaffolded pack — not full project copies.
/// Distinct from editor New Level templates (Engine/Assets/LevelTemplates/).
class EditorProjectService {
public:
    void LoadRecents();
    void SaveRecents() const;
    void Remember(const std::string& projectPath);

    [[nodiscard]] const std::vector<RecentProject>& Recents() const { return recents_; }

    /// True if `path` is a project folder (contains `Config/DefaultEngine.ini`) or that marker
    /// file.
    [[nodiscard]] static bool ResolveProjectDirectory(const std::string& pathOrMarker,
                                                      std::string& outDirectory,
                                                      std::string& outError);

    [[nodiscard]] static bool ReadProjectInfo(const std::string& projectDirectory,
                                              EditorProjectInfo& out, std::string& outError);

    [[nodiscard]] static bool WriteProjectEngineVersion(const std::string& projectDirectory,
                                                        const std::string& engineVersion,
                                                        std::string& outError);

    /// Prefer `Levels/<Name>.llev` relative to project Content. Empty clears the field.
    [[nodiscard]] static bool WriteProjectGameDefaultMap(const std::string& projectDirectory,
                                                         const std::string& gameDefaultMap,
                                                         std::string& outError);

    /// Empty clears the field (editor then uses `gameDefaultMap`).
    [[nodiscard]] static bool WriteProjectEditorStartupMap(const std::string& projectDirectory,
                                                           const std::string& editorStartupMap,
                                                           std::string& outError);

    /// Empty clears the field.
    [[nodiscard]] static bool
    WriteProjectGlobalDefaultGameMode(const std::string& projectDirectory,
                                      const std::string& globalDefaultGameMode,
                                      std::string& outError);

    [[nodiscard]] static bool WriteProjectDisplayName(const std::string& projectDirectory,
                                                      const std::string& displayName,
                                                      std::string& outError);

    [[nodiscard]] static bool WriteProjectDescription(const std::string& projectDirectory,
                                                      const std::string& description,
                                                      std::string& outError);

    [[nodiscard]] static bool WriteProjectBuildDedicatedServer(const std::string& projectDirectory,
                                                               bool buildDedicatedServer,
                                                               std::string& outError);

    [[nodiscard]] static bool WriteProjectEditorScalability(
        const std::string& projectDirectory, const PostProcessSettings& post,
        const TextureQualitySettings& textureQuality, std::string& outError);

    /// Create `parent/<name>/` via Blank content library (same as CreateFromTemplate "Blank").
    [[nodiscard]] static bool CreateBlankProject(const std::string& parentDirectory,
                                                 const std::string& projectName,
                                                 const std::string& displayName,
                                                 std::string& outProjectPath,
                                                 std::string& outError);

    /// Scaffold a minimal Blank pack, then merge `Templates/<templateId>/Content` into it.
    [[nodiscard]] static bool
    CreateFromTemplate(const std::string& parentDirectory, const std::string& projectName,
                       const std::string& displayName, const std::string& templateId,
                       std::string& outProjectPath, std::string& outError);

    [[nodiscard]] static std::string DefaultProjectsRoot();
    [[nodiscard]] static std::string TemplatesRoot();
    [[nodiscard]] static std::vector<std::string> ListTemplateIds();

    [[nodiscard]] static bool IsValidProjectName(const std::string& name, std::string& outError);

private:
    std::vector<RecentProject> recents_;

    [[nodiscard]] static std::string RecentsFilePath();
    [[nodiscard]] static bool WriteTextFile(const std::filesystem::path& path,
                                            const std::string& contents, std::string& outError);
    /// Write the scaffolded `Levels/Main.llev` (ground plane + PlayerStart + sun).
    [[nodiscard]] static bool WriteStarterLevelFile(const std::filesystem::path& path,
                                                    std::string& outError);
    [[nodiscard]] static bool ScaffoldMinimalPack(const std::filesystem::path& projectDir,
                                                  const std::string& name,
                                                  const std::string& displayName,
                                                  std::string& outError);
};

} // namespace leon::editor
