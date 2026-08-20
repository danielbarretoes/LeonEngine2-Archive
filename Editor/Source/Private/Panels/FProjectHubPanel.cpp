#include "Editor/Panels/FProjectHubPanel.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Editor/Utils/FEditorFileDialog.hpp"

#include <glad/glad.h>
#include <stb_image.h>
#include <imgui.h>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace Leon::Editor {

    FProjectHubPanel::FProjectHubPanel() {
        std::string defaultParent = (fs::current_path() / "Projects").string();
#ifdef _WIN32
        strncpy_s(NewProjectPath, sizeof(NewProjectPath), defaultParent.c_str(), _TRUNCATE);
#else
        std::strncpy(NewProjectPath, defaultParent.c_str(), sizeof(NewProjectPath) - 1);
        NewProjectPath[sizeof(NewProjectPath) - 1] = '\0';
#endif
    }

    FProjectHubPanel::~FProjectHubPanel() {
        DestroyBrandTexture();
    }

    void FProjectHubPanel::DestroyBrandTexture() {
        if (BrandTextureId != 0) {
            glDeleteTextures(1, &BrandTextureId);
            BrandTextureId = 0;
        }
        BrandWidth = 0;
        BrandHeight = 0;
    }

    void FProjectHubPanel::EnsureBrandTexture() {
        if (bBrandLoadAttempted)
            return;
        bBrandLoadAttempted = true;

        std::vector<std::string> logoCandidates = {
            "Editor/Resources/Brand/LeonLogoUi.png",       "../Editor/Resources/Brand/LeonLogoUi.png",
            "../../Editor/Resources/Brand/LeonLogoUi.png", "Resources/Brand/LeonLogoUi.png",
            "Engine/Assets/Brand/LeonLogoUi.png",          "Editor/Resources/Brand/LeonLogo.png",
        };

#ifdef _WIN32
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
            fs::path exeDir = fs::path(exePath).parent_path();
            logoCandidates.push_back((exeDir / "Resources" / "Brand" / "LeonLogoUi.png").string());
            logoCandidates.push_back((exeDir / "Editor" / "Resources" / "Brand" / "LeonLogoUi.png").string());
            logoCandidates.push_back(
                (exeDir / ".." / ".." / "Editor" / "Resources" / "Brand" / "LeonLogoUi.png").string());
        }
#endif

        const char* envRoot = std::getenv("LEON_ENGINE_ROOT");
        if (envRoot && envRoot[0] != '\0') {
            logoCandidates.push_back(
                (fs::path(envRoot) / "Editor" / "Resources" / "Brand" / "LeonLogoUi.png").string());
        }

        std::string validPath;
        for (const auto& p : logoCandidates) {
            try {
                if (fs::exists(p)) {
                    validPath = fs::canonical(p).string();
                    break;
                }
            } catch (...) {
            }
        }

        if (validPath.empty())
            return;

        int w = 0, h = 0, channels = 0;
        stbi_set_flip_vertically_on_load(0);
        unsigned char* pixels = stbi_load(validPath.c_str(), &w, &h, &channels, 4);
        if (!pixels || w <= 0 || h <= 0) {
            if (pixels)
                stbi_image_free(pixels);
            return;
        }

        glGenTextures(1, &BrandTextureId);
        glBindTexture(GL_TEXTURE_2D, BrandTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(pixels);
        BrandWidth = w;
        BrandHeight = h;
    }

    void FProjectHubPanel::LoadRecentProjects(const std::string& InSavedDir) {
        SavedDirectory = InSavedDir;
        RecentProjects.clear();

        fs::path jsonPath = fs::path(InSavedDir) / "RecentProjects.json";
        if (fs::exists(jsonPath)) {
            std::ifstream file(jsonPath);
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            size_t pos = 0;
            while ((pos = content.find("\"Path\":", pos)) != std::string::npos) {
                size_t start = content.find('"', pos + 7);
                if (start == std::string::npos)
                    break;
                size_t end = content.find('"', start + 1);
                if (end == std::string::npos)
                    break;

                std::string projPath = content.substr(start + 1, end - start - 1);
                if (!projPath.empty() && fs::exists(projPath)) {
                    FProjectDescriptor desc;
                    std::string projName = desc.Load(projPath) ? desc.ProjectName : fs::path(projPath).stem().string();

                    FRecentProjectInfo info;
                    info.Path = projPath;
                    info.Name = projName;
                    info.EngineVersion = desc.EngineVersion;
                    RecentProjects.push_back(info);
                }
                pos = end + 1;
            }
        }

        // Always discover built-in projects if not already present
        auto addIfMissing = [this](const std::string& InRelativePath) {
            fs::path absPath = fs::absolute(InRelativePath);
            if (fs::exists(absPath)) {
                std::string canonical = absPath.string();
                for (const auto& p : RecentProjects) {
                    if (fs::equivalent(fs::path(p.Path), absPath))
                        return;
                }
                FProjectDescriptor desc;
                FRecentProjectInfo info;
                info.Path = canonical;
                info.Name = desc.Load(canonical) ? desc.ProjectName : absPath.stem().string();
                info.EngineVersion = desc.EngineVersion;
                RecentProjects.push_back(info);
            }
        };

        addIfMissing("Projects/Sandbox/Sandbox.lproject");
        addIfMissing("Projects/LeonTournament/LeonTournament.lproject");
    }

    void FProjectHubPanel::SaveRecentProjects(const std::string& InSavedDir) {
        if (InSavedDir.empty())
            return;
        fs::create_directories(InSavedDir);
        fs::path jsonPath = fs::path(InSavedDir) / "RecentProjects.json";

        std::ofstream file(jsonPath);
        if (!file.is_open())
            return;

        file << "{\n  \"RecentProjects\": [\n";
        for (size_t i = 0; i < RecentProjects.size(); ++i) {
            const auto& p = RecentProjects[i];
            std::string escapedPath;
            for (char c : p.Path) {
                if (c == '\\')
                    escapedPath += "/";
                else
                    escapedPath += c;
            }

            file << "    {\n";
            file << "      \"Name\": \"" << p.Name << "\",\n";
            file << "      \"Path\": \"" << escapedPath << "\",\n";
            file << "      \"EngineVersion\": \"" << p.EngineVersion << "\"\n";
            file << "    }" << (i + 1 < RecentProjects.size() ? "," : "") << "\n";
        }
        file << "  ]\n}\n";
    }

    void FProjectHubPanel::AddRecentProject(const std::string& InProjectPath) {
        if (InProjectPath.empty())
            return;
        fs::path p = fs::absolute(InProjectPath);
        if (!fs::exists(p))
            return;

        std::string canonical = p.string();
        for (auto it = RecentProjects.begin(); it != RecentProjects.end(); ++it) {
            if (fs::equivalent(fs::path(it->Path), p)) {
                RecentProjects.erase(it);
                break;
            }
        }

        FProjectDescriptor desc;
        FRecentProjectInfo info;
        info.Path = canonical;
        info.Name = desc.Load(canonical) ? desc.ProjectName : p.stem().string();
        info.EngineVersion = desc.EngineVersion;
        RecentProjects.insert(RecentProjects.begin(), info);

        SaveRecentProjects(SavedDirectory);
    }

    void FProjectHubPanel::DrawFullscreen(bool bCanReturnToEditor, bool* bInOutOpen) {
        EnsureBrandTexture();

        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.09f, 0.09f, 0.11f, 1.0f));

        if (ImGui::Begin("##LeonProjectLauncherFullscreen", nullptr, flags)) {
            float winW = ImGui::GetWindowWidth();
            float winH = ImGui::GetWindowHeight();

            // Center card container
            float cardW = std::min(winW - 60.0f, 980.0f);
            float cardH = std::min(winH - 60.0f, 640.0f);
            float startX = (winW - cardW) * 0.5f;
            float startY = (winH - cardH) * 0.5f;

            ImGui::SetCursorPos(ImVec2(startX, startY));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));

            if (ImGui::BeginChild("##LauncherCard", ImVec2(cardW, cardH), true,
                                  ImGuiWindowFlags_AlwaysUseWindowPadding)) {
                // Header with Logo
                if (BrandTextureId != 0) {
                    constexpr float kLogoSize = 56.0f;
                    ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(BrandTextureId)),
                                 ImVec2(kLogoSize, kLogoSize));
                    ImGui::SameLine();
                }

                ImGui::BeginGroup();
                if (FEditorTheme::FontBold)
                    ImGui::PushFont(FEditorTheme::FontBold);
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "LEON ENGINE");
                if (FEditorTheme::FontBold)
                    ImGui::PopFont();
                ImGui::SameLine();
                ImGui::TextDisabled("v0.15.0  |  Unreal Engine-Aligned Micro Framework");
                ImGui::TextDisabled("Select an existing project, browse your disk, or create a new game project.");
                ImGui::EndGroup();

                if (bCanReturnToEditor && bInOutOpen) {
                    ImGui::SameLine(cardW - 150.0f);
                    if (ImGui::Button("Back to Editor", ImVec2(120.0f, 28.0f))) {
                        *bInOutOpen = false;
                    }
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Left Navigation Tabs / Right Workspace
                ImGui::Columns(2, "LauncherColumns", false);
                ImGui::SetColumnWidth(0, 200.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
                if (ImGui::Selectable("  Recent Projects", SelectedTab == 0, 0, ImVec2(180.0f, 36.0f)))
                    SelectedTab = 0;
                if (ImGui::Selectable("  New Project", SelectedTab == 1, 0, ImVec2(180.0f, 36.0f)))
                    SelectedTab = 1;
                ImGui::PopStyleVar();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Native Windows Browse Button
                if (ImGui::Button("Browse Disk...", ImVec2(180.0f, 34.0f))) {
                    std::string picked = FEditorFileDialog::OpenFile(
                        "Leon Project (*.lproject)\0*.lproject\0All Files (*.*)\0*.*\0", "Select LeonEngine Project");
                    if (picked.empty()) {
                        picked = FEditorFileDialog::PickFolder("Select LeonEngine Project Directory");
                        if (!picked.empty()) {
                            // Find .lproject in picked directory
                            for (const auto& entry : fs::directory_iterator(picked)) {
                                if (entry.path().extension() == ".lproject") {
                                    picked = entry.path().string();
                                    break;
                                }
                            }
                        }
                    }

                    if (!picked.empty() && fs::exists(picked)) {
                        AddRecentProject(picked);
                        if (OnProjectSelected)
                            OnProjectSelected(picked);
                        if (bInOutOpen)
                            *bInOutOpen = false;
                    }
                }

                ImGui::NextColumn();

                // Content Panel
                if (SelectedTab == 0) {
                    DrawRecentTab();
                } else {
                    DrawNewProjectTab();
                }

                ImGui::Columns(1);

                // Status message
                if (!StatusMessage.empty()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(bStatusIsError ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                                                      : ImVec4(0.35f, 1.0f, 0.35f, 1.0f),
                                       "%s", StatusMessage.c_str());
                }
            }
            ImGui::EndChild();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }
        ImGui::End();

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    void FProjectHubPanel::DrawRecentTab() {
        ImGui::TextUnformatted("Recent Projects");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::BeginChild("RecentListChild", ImVec2(0.0f, -10.0f), false);

        if (RecentProjects.empty()) {
            ImGui::TextDisabled("No recent projects found.");
        } else {
            for (size_t i = 0; i < RecentProjects.size(); ++i) {
                const auto& proj = RecentProjects[i];
                ImGui::PushID(static_cast<int>(i));

                bool bExists = fs::exists(proj.Path);

                ImGui::BeginGroup();
                ImGui::TextColored(bExists ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s",
                                   proj.Name.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled("(Engine v%s)", proj.EngineVersion.c_str());
                ImGui::TextDisabled("%s", proj.Path.c_str());
                ImGui::EndGroup();

                ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
                if (ImGui::Button(bExists ? "Open" : "Missing", ImVec2(90.0f, 32.0f)) && bExists) {
                    AddRecentProject(proj.Path);
                    if (OnProjectSelected) {
                        OnProjectSelected(proj.Path);
                    }
                }

                ImGui::Separator();
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
    }

    void FProjectHubPanel::DrawNewProjectTab() {
        ImGui::TextUnformatted("Create New Project");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Project Name:");
        ImGui::InputText("##NewProjectName", NewProjectName, sizeof(NewProjectName));

        ImGui::Spacing();
        ImGui::Text("Parent Directory:");
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
        ImGui::InputText("##NewProjectPath", NewProjectPath, sizeof(NewProjectPath));
        ImGui::SameLine();
        if (ImGui::Button("Browse...", ImVec2(90.0f, 0.0f))) {
            std::string picked = FEditorFileDialog::PickFolder("Select Projects Root Folder");
            if (!picked.empty()) {
#ifdef _WIN32
                strncpy_s(NewProjectPath, sizeof(NewProjectPath), picked.c_str(), _TRUNCATE);
#else
                std::strncpy(NewProjectPath, picked.c_str(), sizeof(NewProjectPath) - 1);
                NewProjectPath[sizeof(NewProjectPath) - 1] = '\0';
#endif
            }
        }

        ImGui::Spacing();
        ImGui::Text("Template:");
        const char* templates[] = {"Blank Project (C++ & Empty Map)", "3D Showcase Level (Materials & Lighting)"};
        ImGui::Combo("##TemplateCombo", &SelectedTemplateIndex, templates, IM_ARRAYSIZE(templates));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        fs::path fullTarget = fs::path(NewProjectPath) / NewProjectName;
        ImGui::TextDisabled("Will create project at:\n%s", fullTarget.string().c_str());

        ImGui::Spacing();
        if (ImGui::Button("Create & Open Project", ImVec2(200.0f, 36.0f))) {
            CreateNewProject();
        }
    }

    void FProjectHubPanel::CreateNewProject() {
        if (std::strlen(NewProjectName) == 0) {
            bStatusIsError = true;
            StatusMessage = "Project name cannot be empty!";
            return;
        }

        fs::path projectDir = fs::path(NewProjectPath) / NewProjectName;
        if (fs::exists(projectDir)) {
            bStatusIsError = true;
            StatusMessage = "Target directory already exists!";
            return;
        }

        try {
            fs::create_directories(projectDir / "Config");
            fs::create_directories(projectDir / "Content" / "Maps");
            fs::create_directories(projectDir / "Source" / NewProjectName);

            fs::path lprojectPath = projectDir / (std::string(NewProjectName) + ".lproject");
            FProjectDescriptor desc;
            desc.ProjectName = NewProjectName;
            desc.EngineVersion = "0.15.0";
            desc.DefaultMap = "/Game/Maps/MainLevel";
            desc.DefaultGameMode = "AGameModeBase";
            desc.Save(lprojectPath.string());

            std::ofstream engineIni(projectDir / "Config" / "DefaultEngine.ini");
            engineIni << "[/Script/Engine.Engine]\nDefaultMap=/Game/Maps/MainLevel\n";

            std::ofstream gameIni(projectDir / "Config" / "DefaultGame.ini");
            gameIni << "[/Script/EngineSettings.GeneralProjectSettings]\nProjectName=" << NewProjectName << "\n";

            std::ofstream inputIni(projectDir / "Config" / "DefaultInput.ini");
            inputIni << "[/Script/Engine.InputSettings]\n";

            std::ofstream mapFile(projectDir / "Content" / "Maps" / "MainLevel.lmap");
            mapFile << "Name: MainLevel\n";
            mapFile << "Environment:\n  StaticLighting: true\n  LightmapResolution: 512\n";
            mapFile << "Actors:\n";
            mapFile << "  - Name: DirectionalLight_0\n    Type: DirectionalLight\n    DirectionalLight:\n      Color: "
                       "[1.0, 0.95, 0.85]\n      Intensity: 3.0\n";
            mapFile << "    Transform:\n      Position: [0, 10, 0]\n      Rotation: [-45, 45, 0]\n";

            bStatusIsError = false;
            StatusMessage = "Project created successfully!";
            AddRecentProject(lprojectPath.string());

            if (OnProjectSelected) {
                OnProjectSelected(lprojectPath.string());
            }
        } catch (const std::exception& e) {
            bStatusIsError = true;
            StatusMessage = std::string("Failed to create project: ") + e.what();
        }
    }

} // namespace Leon::Editor
