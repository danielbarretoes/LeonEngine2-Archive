#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <leon/core/LeonProjectConfig.h>
#include <leon/core/Paths.h>
#include <leon/core/Version.h>
#include <leon/editor/EditorProject.h>
#include <leon/level/LeonLevelFormat.h>
#include <nlohmann/json.hpp>

namespace leon::editor {
namespace {

constexpr std::size_t kMaxRecents = 12;

std::string SanitizeDisplayFallback(const std::string& name) {
    return name.empty() ? "Untitled" : name;
}

[[nodiscard]] bool LoadMutableProjectSettings(const std::string& projectDirectory,
                                              LeonProjectSettings& outSettings,
                                              std::string& outDirectory, std::string& outError) {
    if (!ResolveLeonProjectDirectory(projectDirectory, outDirectory, outError)) {
        return false;
    }
    return LoadLeonProjectSettings(outDirectory, outSettings, outError);
}

[[nodiscard]] bool SaveMutableProjectSettings(const std::string& directory,
                                              const LeonProjectSettings& settings,
                                              std::string& outError) {
    return SaveLeonProjectSettings(directory, settings, outError);
}

/// Merge template Content library into a scaffolded project Content/ (overwrite).
bool MergeTemplateContent(const std::filesystem::path& templateContent,
                          const std::filesystem::path& projectContent, std::string& outError) {
    std::error_code ec;
    if (!std::filesystem::is_directory(templateContent, ec) || ec) {
        outError = "Template Content/ not found: " + templateContent.string();
        return false;
    }
    std::filesystem::create_directories(projectContent, ec);
    if (ec) {
        outError = "Cannot create Content/: " + ec.message();
        return false;
    }
    std::filesystem::copy(templateContent, projectContent,
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing,
                          ec);
    if (ec) {
        outError = "Cannot merge template Content: " + ec.message();
        return false;
    }
    return true;
}

struct TemplateMeta {
    std::string displayName;
    std::string globalDefaultGameMode = "Default";
};

[[nodiscard]] bool ReadTemplateMeta(const std::filesystem::path& templateDir, TemplateMeta& out,
                                    std::string& outError) {
    out = {};
    out.globalDefaultGameMode = "Default";
    const auto marker = templateDir / "template.json";
    std::ifstream in(marker);
    if (!in.is_open()) {
        outError = "Template is missing template.json: " + templateDir.filename().string();
        return false;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (const std::exception& ex) {
        outError = std::string("Invalid template.json: ") + ex.what();
        return false;
    }
    if (!doc.is_object()) {
        outError = "Invalid template.json (expected object)";
        return false;
    }
    if (doc.contains("displayName") && doc["displayName"].is_string()) {
        out.displayName = doc["displayName"].get<std::string>();
    }
    if (doc.contains("globalDefaultGameMode") && doc["globalDefaultGameMode"].is_string()) {
        out.globalDefaultGameMode = doc["globalDefaultGameMode"].get<std::string>();
    }
    return true;
}

/// Stamp project identity after scaffold + Content merge.
bool StampNewProjectIdentity(const std::filesystem::path& projectDir, const std::string& name,
                             const std::string& displayName, const std::string& templateId,
                             const std::string& globalDefaultGameMode, std::string& outError) {
    LeonProjectSettings settings{};
    settings.name = name;
    settings.displayName = displayName;
    settings.templateId = templateId;
    settings.globalDefaultGameMode =
        globalDefaultGameMode.empty() ? "Default" : globalDefaultGameMode;

    if (settings.globalDefaultGameMode == "third-person") {
        settings.gameModes = {"third-person"};
    } else if (settings.globalDefaultGameMode != "Default") {
        settings.gameModes = {settings.globalDefaultGameMode};
    }

    constexpr const char* kDefaultMap = "Levels/Main.llev";
    settings.gameDefaultMap = kDefaultMap;
    settings.editorStartupMap = kDefaultMap;

    settings.editorScalability = MakeDefaultEditorScalability();
    settings.hasEditorScalability = true;

    return SaveLeonProjectSettings(projectDir.string(), settings, outError);
}

} // namespace

std::string EditorProjectService::RecentsFilePath() {
    return (ExecutableDirectory() / "editor_recent_projects.json").lexically_normal().string();
}

void EditorProjectService::LoadRecents() {
    recents_.clear();
    const std::string path = RecentsFilePath();
    std::ifstream in(path);
    if (!in.is_open()) {
        return;
    }
    nlohmann::json doc;
    try {
        in >> doc;
    } catch (...) {
        return;
    }
    if (!doc.is_object() || !doc.contains("projects") || !doc["projects"].is_array()) {
        return;
    }
    for (const auto& item : doc["projects"]) {
        if (!item.is_object()) {
            continue;
        }
        RecentProject recent;
        recent.path = item.value("path", "");
        recent.name = item.value("name", "");
        recent.displayName = item.value("displayName", recent.name);
        recent.engineVersion = item.value("engineVersion", "");
        if (recent.path.empty()) {
            continue;
        }
        std::error_code ec;
        if (!std::filesystem::is_directory(recent.path, ec) || ec) {
            continue;
        }
        if (!IsLeonProjectDirectory(std::filesystem::path(recent.path))) {
            continue;
        }
        // Prefer stamped version from Config/DefaultGame.ini when the recents entry is older.
        if (recent.engineVersion.empty()) {
            EditorProjectInfo info;
            std::string ignored;
            if (ReadProjectInfo(recent.path, info, ignored) && !info.engineVersion.empty()) {
                recent.engineVersion = info.engineVersion;
            }
        }
        recents_.push_back(std::move(recent));
        if (recents_.size() >= kMaxRecents) {
            break;
        }
    }
}

void EditorProjectService::SaveRecents() const {
    nlohmann::json doc;
    doc["projects"] = nlohmann::json::array();
    for (const RecentProject& recent : recents_) {
        nlohmann::json entry = {
            {"path", recent.path},
            {"name", recent.name},
            {"displayName", recent.displayName},
        };
        if (!recent.engineVersion.empty()) {
            entry["engineVersion"] = recent.engineVersion;
        }
        doc["projects"].push_back(std::move(entry));
    }
    std::ofstream out(RecentsFilePath());
    if (!out.is_open()) {
        std::cerr << "Editor: cannot write recent projects file\n";
        return;
    }
    out << doc.dump(2) << '\n';
}

void EditorProjectService::Remember(const std::string& projectPath) {
    EditorProjectInfo info;
    std::string err;
    if (!ReadProjectInfo(projectPath, info, err)) {
        return;
    }

    const std::string version = EngineVersionString();
    if (!WriteProjectEngineVersion(info.path, version, err)) {
        std::cerr << "Editor: could not stamp engine version on project: " << err << '\n';
    } else {
        info.engineVersion = version;
    }

    recents_.erase(std::remove_if(recents_.begin(), recents_.end(),
                                  [&](const RecentProject& r) {
                                      return std::filesystem::path(r.path).lexically_normal() ==
                                             std::filesystem::path(info.path).lexically_normal();
                                  }),
                   recents_.end());
    RecentProject recent;
    recent.path = info.path;
    recent.name = info.name;
    recent.displayName = info.displayName;
    recent.engineVersion = info.engineVersion.empty() ? version : info.engineVersion;
    recents_.insert(recents_.begin(), std::move(recent));
    if (recents_.size() > kMaxRecents) {
        recents_.resize(kMaxRecents);
    }
    SaveRecents();
}

bool EditorProjectService::ResolveProjectDirectory(const std::string& pathOrMarker,
                                                   std::string& outDirectory,
                                                   std::string& outError) {
    return ResolveLeonProjectDirectory(pathOrMarker, outDirectory, outError);
}

bool EditorProjectService::ReadProjectInfo(const std::string& projectDirectory,
                                           EditorProjectInfo& out, std::string& outError) {
    out = {};
    LeonProjectSettings settings;
    if (!LoadLeonProjectSettings(projectDirectory, settings, outError)) {
        return false;
    }
    std::string directory;
    if (!ResolveLeonProjectDirectory(projectDirectory, directory, outError)) {
        return false;
    }
    out.path = directory;
    out.name = settings.name;
    out.displayName = settings.displayName;
    out.engineVersion = settings.engineVersion;
    out.gameDefaultMap = settings.gameDefaultMap;
    out.editorStartupMap = settings.editorStartupMap;
    out.globalDefaultGameMode = settings.globalDefaultGameMode;
    out.templateId = settings.templateId;
    out.description = settings.description;
    out.buildDedicatedServer = settings.buildDedicatedServer;
    out.hasEditorScalability = settings.hasEditorScalability;
    out.editorScalability = settings.editorScalability;
    return true;
}

bool EditorProjectService::WriteProjectEngineVersion(const std::string& projectDirectory,
                                                     const std::string& engineVersion,
                                                     std::string& outError) {
    outError.clear();
    if (engineVersion.empty()) {
        outError = "Empty engine version.";
        return false;
    }
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.engineVersion = engineVersion;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectGameDefaultMap(const std::string& projectDirectory,
                                                      const std::string& gameDefaultMap,
                                                      std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.gameDefaultMap = gameDefaultMap;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectEditorStartupMap(const std::string& projectDirectory,
                                                        const std::string& editorStartupMap,
                                                        std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.editorStartupMap = editorStartupMap;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectGlobalDefaultGameMode(
    const std::string& projectDirectory, const std::string& globalDefaultGameMode,
    std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.globalDefaultGameMode = globalDefaultGameMode;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectDisplayName(const std::string& projectDirectory,
                                                   const std::string& displayName,
                                                   std::string& outError) {
    outError.clear();
    if (displayName.empty()) {
        outError = "Display name cannot be empty.";
        return false;
    }
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.displayName = displayName;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectDescription(const std::string& projectDirectory,
                                                   const std::string& description,
                                                   std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.description = description;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectBuildDedicatedServer(const std::string& projectDirectory,
                                                            bool buildDedicatedServer,
                                                            std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.buildDedicatedServer = buildDedicatedServer;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::WriteProjectEditorScalability(
    const std::string& projectDirectory, const PostProcessSettings& post,
    const TextureQualitySettings& textureQuality, std::string& outError) {
    outError.clear();
    LeonProjectSettings settings;
    std::string directory;
    if (!LoadMutableProjectSettings(projectDirectory, settings, directory, outError)) {
        return false;
    }
    settings.editorScalability.postProcess = post;
    settings.editorScalability.textureQuality = textureQuality;
    settings.hasEditorScalability = true;
    return SaveMutableProjectSettings(directory, settings, outError);
}

bool EditorProjectService::IsValidProjectName(const std::string& name, std::string& outError) {
    if (name.empty()) {
        outError = "Name cannot be empty.";
        return false;
    }
    if (name.size() > 64) {
        outError = "Name is too long.";
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(name[0])) && name[0] != '_') {
        outError = "Name must start with a letter or underscore.";
        return false;
    }
    for (char ch : name) {
        const unsigned char uc = static_cast<unsigned char>(ch);
        if (!std::isalnum(uc) && ch != '_') {
            outError = "Use only letters, digits, and underscores.";
            return false;
        }
    }
    return true;
}

std::string EditorProjectService::DefaultProjectsRoot() {
    return ResolveProjectsDirectory();
}

std::string EditorProjectService::TemplatesRoot() {
    return ResolveAssetPath("Templates");
}

std::vector<std::string> EditorProjectService::ListTemplateIds() {
    std::vector<std::string> ids;
    const std::string root = TemplatesRoot();
    if (root.empty()) {
        return ids;
    }
    std::error_code ec;
    const std::filesystem::path templatesRoot(root);
    if (!std::filesystem::is_directory(templatesRoot, ec) || ec) {
        return ids;
    }
    for (const auto& entry : std::filesystem::directory_iterator(templatesRoot, ec)) {
        if (ec || !entry.is_directory()) {
            continue;
        }
        const std::string name = entry.path().filename().string();
        if (name.empty() || name[0] == '.' || name == "_shared") {
            continue;
        }
        if (std::filesystem::is_regular_file(entry.path() / "template.json", ec) && !ec) {
            ids.push_back(name);
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool EditorProjectService::WriteTextFile(const std::filesystem::path& path,
                                         const std::string& contents, std::string& outError) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        outError = "Cannot create parent for " + path.string();
        return false;
    }
    std::ofstream out(path);
    if (!out.is_open()) {
        outError = "Cannot write " + path.string();
        return false;
    }
    out << contents;
    return true;
}

bool EditorProjectService::WriteStarterLevelFile(const std::filesystem::path& path,
                                                 std::string& outError) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        outError = "Cannot create parent for " + path.string();
        return false;
    }

    LevelDocument doc;
    doc.name = "Main";
    // Empty Override → Project Settings globalDefaultGameMode (Blank Default / ThirdPerson
    // third-person).
    doc.gameMode.clear();

    LevelActorRecord ground;
    ground.actorClass = ELevelActorClass::Plane;
    ground.scale = {10.0f, 1.0f, 10.0f};
    ground.collisionEnabled = true;
    ground.enableGravity = false;
    doc.actors.push_back(std::move(ground));

    LevelActorRecord playerStart;
    playerStart.actorClass = ELevelActorClass::PlayerStart;
    playerStart.position = {0.0f, 1.0f, 4.0f};
    playerStart.rotationDegrees = {0.0f, 180.0f, 0.0f};
    playerStart.enableGravity = false;
    doc.actors.push_back(std::move(playerStart));

    LevelLightRecord sun;
    sun.lightClass = ELevelLightClass::DirectionalLight;
    sun.rotationDegrees = {50.0f, -30.0f, 0.0f};
    doc.lights.push_back(sun);

    if (!SaveLeonLevelFile(path.string(), doc)) {
        outError = "Cannot write " + path.string();
        return false;
    }
    return true;
}

bool EditorProjectService::ScaffoldMinimalPack(const std::filesystem::path& projectDir,
                                               const std::string& name,
                                               const std::string& displayName,
                                               std::string& outError) {
    // New Project pack seed (Blank gameplay). Template Content is merged afterward.
    (void)name;
    (void)displayName;

    const char* mainCpp =
        "#include <leon/packs/Blank/RegisterModes.h>\n"
        "#include <leon/runtime/RunLeonGame.h>\n\n"
        "#ifndef LEON_PACK_NAME\n"
        "#define LEON_PACK_NAME \"Blank\"\n"
        "#endif\n\n"
        "int main(int argc, char** argv) {\n"
        "    return leon::runtime::RunLeonGame(argc, argv, LEON_PACK_NAME,\n"
        "                                      &leon::packs::blank::RegisterModes);\n"
        "}\n";

    const char* registerH = "#pragma once\n\n"
                            "#include <leon/Engine.h>\n"
                            "#include <leon/gameplay/GameplayRouter.h>\n\n"
                            "namespace leon::packs::blank {\n\n"
                            "void RegisterModes(Engine& engine, GameplayRouter& router);\n\n"
                            "} // namespace leon::packs::blank\n";

    const char* registerCpp =
        "#include <leon/packs/Blank/RegisterModes.h>\n\n"
        "namespace leon::packs::blank {\n\n"
        "void RegisterModes(Engine& /*engine*/, GameplayRouter& /*router*/) {}\n\n"
        "} // namespace leon::packs::blank\n";

    const char* gameplayCmake = R"cmake(if(TARGET leon_blank_gameplay)
    return()
endif()

if(NOT TARGET leon_engine)
    message(FATAL_ERROR "leon_blank_gameplay requires leon_engine")
endif()

set(_leon_blank_root "${CMAKE_CURRENT_LIST_DIR}")

add_library(leon_blank_gameplay STATIC
    "${_leon_blank_root}/src/RegisterModes.cpp"
)
target_include_directories(leon_blank_gameplay
    PUBLIC
        "${_leon_blank_root}/include"
)
target_link_libraries(leon_blank_gameplay PUBLIC leon_engine)
if(NOT DEFINED LEON_REPO_ROOT)
    get_filename_component(LEON_REPO_ROOT "${_leon_blank_root}/../.." ABSOLUTE)
endif()
include("${LEON_REPO_ROOT}/Build/LeonCompileOptions.cmake")
leon_apply_compile_options(leon_blank_gameplay)
if(MSVC)
    target_compile_definitions(leon_blank_gameplay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
)cmake";

    const char* cmake =
        R"cmake(# Projects/<folder> — Blank gameplay seed (pack name = folder). Content from Templates/.
cmake_minimum_required(VERSION 3.20)

get_filename_component(LEON_PACK_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
project(Leon${LEON_PACK_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CONFIGURATION_TYPES)
    foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${_cfg}" _cfg_up)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_up} "${CMAKE_BINARY_DIR}/${_cfg}")
    endforeach()
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(NOT LEON_REPO_ROOT)
    set(_leon_probe "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach(_i RANGE 0 12)
        if(EXISTS "${_leon_probe}/Build/Dependencies.cmake")
            set(LEON_REPO_ROOT "${_leon_probe}")
            break()
        endif()
        get_filename_component(_leon_probe "${_leon_probe}/.." ABSOLUTE)
    endforeach()
    unset(_leon_probe)
endif()
get_filename_component(LEON_REPO_ROOT "${LEON_REPO_ROOT}" ABSOLUTE)
if(NOT EXISTS "${LEON_REPO_ROOT}/Build/Dependencies.cmake")
    message(FATAL_ERROR
        "Leon Engine root not found (Build/Dependencies.cmake). "
        "Pass -DLEON_REPO_ROOT=<path> or place the project under <repo>/Projects/.")
endif()

set(LEON_BUILD_CLIENT OFF CACHE BOOL "" FORCE)
set(LEON_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LEON_ENGINE_ASSETS "${LEON_REPO_ROOT}/Engine/Assets" CACHE PATH "Engine runtime assets root" FORCE)
set(LEON_PROJECTS_ROOT "${LEON_REPO_ROOT}/Projects" CACHE PATH "Projects root" FORCE)

include("${LEON_REPO_ROOT}/Build/Dependencies.cmake")
add_subdirectory("${LEON_REPO_ROOT}/Engine" "${CMAKE_BINARY_DIR}/_leon_engine")
add_subdirectory("${LEON_REPO_ROOT}/Runtime" "${CMAKE_BINARY_DIR}/_leon_runtime")
include("${CMAKE_CURRENT_SOURCE_DIR}/GameplayLib.cmake")

set(LEON_PACK_TARGET "leon-${LEON_PACK_NAME}")
add_executable(${LEON_PACK_TARGET} main.cpp)
set_target_properties(${LEON_PACK_TARGET} PROPERTIES OUTPUT_NAME "${LEON_PACK_NAME}")
target_compile_definitions(${LEON_PACK_TARGET} PRIVATE LEON_PACK_NAME="${LEON_PACK_NAME}")
target_link_libraries(${LEON_PACK_TARGET} PRIVATE leon_blank_gameplay leon_runtime)

# Flat Shipping layout next to the exe (see Docs/NAMING.md §6.1).
set(_leon_sync "${LEON_REPO_ROOT}/Build/SyncDirectory.cmake")
set(_exe_dir "$<TARGET_FILE_DIR:${LEON_PACK_TARGET}>")
add_custom_command(TARGET ${LEON_PACK_TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${LEON_REPO_ROOT}/Engine/Assets
        -DDST=${_exe_dir}/Engine/Assets
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Config
        -DDST=${_exe_dir}/Config
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
    COMMAND ${CMAKE_COMMAND}
        -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/Content
        -DDST=${_exe_dir}/Content
        -DREMOVE_ORPHANS=ON
        -P ${_leon_sync}
)

if(MSVC)
    target_compile_options(${LEON_PACK_TARGET} PRIVATE /W4 /permissive- /Zc:__cplusplus)
    target_compile_definitions(${LEON_PACK_TARGET} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_compile_options(${LEON_PACK_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
endif()
)cmake";

    if (!WriteStarterLevelFile(projectDir / "Content" / "Levels" / "Main.llev", outError)) {
        return false;
    }
    if (!WriteTextFile(projectDir / "main.cpp", mainCpp, outError)) {
        return false;
    }
    if (!WriteTextFile(projectDir / "GameplayLib.cmake", gameplayCmake, outError)) {
        return false;
    }
    {
        const auto includeDir = projectDir / "include" / "leon" / "packs" / "Blank";
        std::error_code ec;
        std::filesystem::create_directories(includeDir, ec);
        if (ec) {
            outError = "Cannot create " + includeDir.string() + ": " + ec.message();
            return false;
        }
        if (!WriteTextFile(includeDir / "RegisterModes.h", registerH, outError)) {
            return false;
        }
    }
    {
        const auto srcDir = projectDir / "src";
        std::error_code ec;
        std::filesystem::create_directories(srcDir, ec);
        if (ec) {
            outError = "Cannot create " + srcDir.string() + ": " + ec.message();
            return false;
        }
        if (!WriteTextFile(srcDir / "RegisterModes.cpp", registerCpp, outError)) {
            return false;
        }
    }
    return WriteTextFile(projectDir / "CMakeLists.txt", cmake, outError);
}

bool EditorProjectService::CreateBlankProject(const std::string& parentDirectory,
                                              const std::string& projectName,
                                              const std::string& displayName,
                                              std::string& outProjectPath, std::string& outError) {
    return CreateFromTemplate(parentDirectory, projectName, displayName, "Blank", outProjectPath,
                              outError);
}

bool EditorProjectService::CreateFromTemplate(const std::string& parentDirectory,
                                              const std::string& projectName,
                                              const std::string& displayName,
                                              const std::string& templateId,
                                              std::string& outProjectPath, std::string& outError) {
    outProjectPath.clear();
    if (!IsValidProjectName(projectName, outError)) {
        return false;
    }
    if (templateId.empty()) {
        outError = "Select a template.";
        return false;
    }
    const std::string shown =
        displayName.empty() ? SanitizeDisplayFallback(projectName) : displayName;

    if (parentDirectory.empty()) {
        outError = "Parent directory is empty.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path parent(parentDirectory);
    if (!std::filesystem::is_directory(parent, ec) || ec) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            outError = "Cannot create parent directory: " + parentDirectory;
            return false;
        }
    }

    const std::filesystem::path projectDir = parent / projectName;
    if (std::filesystem::exists(projectDir, ec)) {
        outError = "A folder already exists: " + projectDir.string();
        return false;
    }

    const std::string templatesRoot = TemplatesRoot();
    const std::filesystem::path templateDir =
        templatesRoot.empty() ? std::filesystem::path{}
                              : std::filesystem::path(templatesRoot) / templateId;

    TemplateMeta meta;
    meta.globalDefaultGameMode = "Default";
    const bool haveTemplate =
        !templatesRoot.empty() && std::filesystem::is_directory(templateDir, ec) && !ec &&
        std::filesystem::is_regular_file(templateDir / "template.json", ec) && !ec;

    if (haveTemplate) {
        if (!ReadTemplateMeta(templateDir, meta, outError)) {
            return false;
        }
    } else if (templateId != "Blank") {
        outError = "Unknown template: " + templateId;
        return false;
    }

    if (!ScaffoldMinimalPack(projectDir, projectName, shown, outError)) {
        return false;
    }

    if (haveTemplate) {
        const auto contentSrc = templateDir / "Content";
        if (std::filesystem::is_directory(contentSrc, ec) && !ec) {
            if (!MergeTemplateContent(contentSrc, projectDir / "Content", outError)) {
                return false;
            }
        }
    }

    if (!StampNewProjectIdentity(projectDir, projectName, shown, templateId,
                                 meta.globalDefaultGameMode, outError)) {
        return false;
    }

    // ThirdPerson: register Engine ThirdPersonGameMode (same class PIE + Shipping).
    if (templateId == "ThirdPerson") {
        const auto registerSrc = projectDir / "src" / "RegisterModes.cpp";
        const char* tpRegisterCpp =
            "#include <leon/packs/Blank/RegisterModes.h>\n"
            "#include <leon/gameplay/ThirdPersonGameMode.h>\n"
            "#include <memory>\n\n"
            "namespace leon::packs::blank {\n\n"
            "void RegisterModes(Engine& /*engine*/, GameplayRouter& router) {\n"
            "    router.AddMode(std::make_unique<leon::ThirdPersonGameMode>());\n"
            "}\n\n"
            "} // namespace leon::packs::blank\n";
        if (!WriteTextFile(registerSrc, tpRegisterCpp, outError)) {
            return false;
        }
    }

    outProjectPath = projectDir.lexically_normal().string();
    return true;
}

} // namespace leon::editor
