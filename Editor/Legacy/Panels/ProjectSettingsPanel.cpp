#include <cfloat>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <iostream>
#include <leon/editor/EditorCatalogs.h>
#include <leon/editor/EditorProject.h>
#include <leon/editor/LucideIcons.h>
#include <leon/editor/panels/ProjectSettingsPanel.h>
#include <leon/editor/ui/UiKit.h>
#include <leon/level/LevelCatalog.h>

namespace leon::editor {
namespace {

[[nodiscard]] std::string BasenameLabel(const std::string& path) {
    if (path.empty()) {
        return "(none)";
    }
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

/// Authoring key stored in DefaultEngine.ini (`Levels/MainMenu.llev`).
[[nodiscard]] std::string DefaultLevelAuthoringKey(const LevelEntry& entry) {
    namespace fs = std::filesystem;
    const fs::path path(entry.path);
    return (fs::path("Levels") / path.filename()).generic_string();
}

[[nodiscard]] bool MatchesAuthoringLevelKey(const std::string& storedKey, const LevelEntry& entry) {
    return storedKey == DefaultLevelAuthoringKey(entry) || BasenameLabel(storedKey) == entry.name ||
           BasenameLabel(storedKey) == std::filesystem::path(entry.path).filename().string();
}

} // namespace

void ProjectSettingsPanel::Draw(EditorContext& ctx) {
    if (!ctx.showProjectSettings) {
        return;
    }
    if (!ui::BeginPanel("Project Settings", {.pOpen = &ctx.showProjectSettings})) {
        ui::EndPanel();
        return;
    }

    if (ctx.projectPath.empty()) {
        ui::Hint("Open a project to edit Project Settings");
        ui::EndPanel();
        return;
    }

    if (descriptionProjectKey_ != ctx.projectPath) {
        descriptionProjectKey_ = ctx.projectPath;
        descriptionDraft_ = ctx.projectDescription;
    }

    ui::PushFormLayout();

    if (ui::CollapsingSection("Project")) {
        ui::FieldLabel("Name");
        ImGui::TextUnformatted(ctx.projectName.empty() ? "(unknown)" : ctx.projectName.c_str());

        char displayBuf[128];
        (void)std::snprintf(displayBuf, sizeof(displayBuf), "%s", ctx.projectDisplayName.c_str());
        ui::FieldLabel("Display Name");
        if (ui::InputText("##display_name", nullptr, displayBuf, sizeof(displayBuf))) {
            std::string err;
            if (EditorProjectService::WriteProjectDisplayName(ctx.projectPath, displayBuf, err)) {
                ctx.projectDisplayName = displayBuf;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }

        char descBuf[512];
        (void)std::snprintf(descBuf, sizeof(descBuf), "%s", descriptionDraft_.c_str());
        if (ui::TextArea("##description", "Description", descBuf, sizeof(descBuf))) {
            descriptionDraft_ = descBuf;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string err;
            if (EditorProjectService::WriteProjectDescription(ctx.projectPath, descriptionDraft_,
                                                              err)) {
                ctx.projectDescription = descriptionDraft_;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }
    }

    if (ui::CollapsingSection("Maps & Modes")) {
        // Game Default Map — shipping / packaged OpenLevel (does not change Editor open).
        {
            const std::string& current = ctx.projectGameDefaultMap;
            const std::string preview =
                current.empty() ? std::string("(first catalog entry)") : BasenameLabel(current);
            ui::SectionLabel("Game Default Map");
            ui::UiSelectDesc mapDesc;
            mapDesc.preview = preview.c_str();
            mapDesc.previewIcon = ui::UiIcon(ELucideIcon::Map);
            mapDesc.width = -1.0f;
            if (ui::BeginSelect("##GameDefaultMap", mapDesc)) {
                if (ui::SelectItem("(first catalog entry)", current.empty())) {
                    std::string err;
                    if (EditorProjectService::WriteProjectGameDefaultMap(ctx.projectPath, {},
                                                                         err)) {
                        ctx.projectGameDefaultMap.clear();
                    } else {
                        std::cerr << "Editor: " << err << '\n';
                    }
                }
                if (ctx.catalog != nullptr) {
                    for (const LevelEntry& entry : ctx.catalog->Entries()) {
                        const std::string key = DefaultLevelAuthoringKey(entry);
                        const bool selected = MatchesAuthoringLevelKey(current, entry);
                        const std::string label = entry.name.empty() ? key : entry.name;
                        if (ui::SelectItem(label.c_str(), selected, ui::UiIcon(ELucideIcon::Map))) {
                            std::string err;
                            if (EditorProjectService::WriteProjectGameDefaultMap(ctx.projectPath,
                                                                                 key, err)) {
                                ctx.projectGameDefaultMap = key;
                            } else {
                                std::cerr << "Editor: " << err << '\n';
                            }
                        }
                    }
                }
                ui::EndSelect();
            }
            ui::Hint("Default map when the shipping build opens a level.");
        }

        // Editor Startup Map — level loaded when opening the project in the Editor.
        {
            const std::string& current = ctx.projectEditorStartupMap;
            const std::string preview =
                current.empty() ? std::string("(use Game Default Map)") : BasenameLabel(current);
            ui::SectionLabel("Editor Startup Map");
            ui::UiSelectDesc mapDesc;
            mapDesc.preview = preview.c_str();
            mapDesc.previewIcon = ui::UiIcon(ELucideIcon::Map);
            mapDesc.width = -1.0f;
            if (ui::BeginSelect("##EditorStartupMap", mapDesc)) {
                if (ui::SelectItem("(use Game Default Map)", current.empty())) {
                    std::string err;
                    if (EditorProjectService::WriteProjectEditorStartupMap(ctx.projectPath, {},
                                                                           err)) {
                        ctx.projectEditorStartupMap.clear();
                    } else {
                        std::cerr << "Editor: " << err << '\n';
                    }
                }
                if (ctx.catalog != nullptr) {
                    for (const LevelEntry& entry : ctx.catalog->Entries()) {
                        const std::string key = DefaultLevelAuthoringKey(entry);
                        const bool selected = MatchesAuthoringLevelKey(current, entry);
                        const std::string label = entry.name.empty() ? key : entry.name;
                        if (ui::SelectItem(label.c_str(), selected, ui::UiIcon(ELucideIcon::Map))) {
                            std::string err;
                            if (EditorProjectService::WriteProjectEditorStartupMap(ctx.projectPath,
                                                                                   key, err)) {
                                ctx.projectEditorStartupMap = key;
                            } else {
                                std::cerr << "Editor: " << err << '\n';
                            }
                        }
                    }
                }
                ui::EndSelect();
            }
            ui::Hint("Level loaded when the editor opens — empty uses game default map.");
        }

        const std::vector<std::string> modes = ListEditorGameModes(ctx.projectPath);
        std::string currentMode = ctx.projectGlobalDefaultGameMode;
        if (currentMode.empty()) {
            currentMode = "Default";
        }
        bool currentKnown = false;
        for (const std::string& mode : modes) {
            if (mode == currentMode) {
                currentKnown = true;
                break;
            }
        }
        const std::string modePreview =
            currentKnown ? currentMode
                         : (ctx.projectGlobalDefaultGameMode.empty() ? std::string("(unset)")
                                                                     : currentMode + " (custom)");
        ui::SectionLabel("Default game mode");
        ui::UiSelectDesc modeDesc;
        modeDesc.preview = modePreview.c_str();
        modeDesc.previewIcon = ui::UiIcon(ELucideIcon::Cpu);
        modeDesc.width = -1.0f;
        if (ui::BeginSelect("##DefaultGameMode", modeDesc)) {
            if (ui::SelectItem("(unset)", ctx.projectGlobalDefaultGameMode.empty())) {
                std::string err;
                if (EditorProjectService::WriteProjectGlobalDefaultGameMode(ctx.projectPath, {},
                                                                            err)) {
                    ctx.projectGlobalDefaultGameMode.clear();
                } else {
                    std::cerr << "Editor: " << err << '\n';
                }
            }
            for (const std::string& mode : modes) {
                const bool selected = (mode == ctx.projectGlobalDefaultGameMode);
                if (ui::SelectItem(mode.c_str(), selected)) {
                    std::string err;
                    if (EditorProjectService::WriteProjectGlobalDefaultGameMode(ctx.projectPath,
                                                                                mode, err)) {
                        ctx.projectGlobalDefaultGameMode = mode;
                    } else {
                        std::cerr << "Editor: " << err << '\n';
                    }
                }
            }
            ui::EndSelect();
        }
        ui::Hint(
            "Used when a level has no game mode override. Pack IDs are also "
            "listed in DefaultEngine.ini for world settings.");
    }

    if (ui::CollapsingSection("Build")) {
        bool buildServer = ctx.projectBuildDedicatedServer;
        if (ui::Checkbox("##build_ds", "Build dedicated headless server", &buildServer,
                         ui::EUiSize::Sm)) {
            std::string err;
            if (EditorProjectService::WriteProjectBuildDedicatedServer(ctx.projectPath, buildServer,
                                                                       err)) {
                ctx.projectBuildDedicatedServer = buildServer;
            } else {
                std::cerr << "Editor: " << err << '\n';
            }
        }
        ui::Hint("Also builds <Name>-server into Shipping/ (Win + Linux scripts)");
    }

    ui::PopFormLayout();
    ui::EndPanel();
}

} // namespace leon::editor
