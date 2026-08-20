#pragma once

#include "Core/Base.hpp"
#include <functional>
#include <string>
#include <vector>

namespace Leon::Editor {

    struct FRecentProjectInfo {
        std::string Name;
        std::string Path;
        std::string EngineVersion;
        std::string LastOpened;
    };

    /**
     * @brief Unreal Engine Project Browser analogue.
     * Dedicated welcome launcher window: lets the user pick recent projects,
     * browse disk for an .lproject, or create a brand new project.
     */
    class FProjectHubPanel {
    public:
        using FOnProjectSelected = std::function<void(const std::string& InProjectPath)>;

        FProjectHubPanel();

        void SetOnProjectSelected(FOnProjectSelected InCallback) { OnProjectSelected = std::move(InCallback); }

        void LoadRecentProjects(const std::string& InSavedDir);
        void SaveRecentProjects(const std::string& InSavedDir);
        void AddRecentProject(const std::string& InProjectPath);

        const std::vector<FRecentProjectInfo>& GetRecentProjects() const { return RecentProjects; }

        /** Renders full-screen standalone launcher window (prior to editor loading) */
        void DrawFullscreen(bool bCanReturnToEditor = false, bool* bInOutOpen = nullptr);

    private:
        void DrawRecentTab();
        void DrawNewProjectTab();
        void CreateNewProject();

        FOnProjectSelected OnProjectSelected;
        std::vector<FRecentProjectInfo> RecentProjects;
        std::string SavedDirectory;

        int SelectedTab = 0; // 0: Recent, 1: New Project
        char NewProjectName[128] = "MyProject";
        char NewProjectPath[512] = "Projects";
        int SelectedTemplateIndex = 0;
        std::string StatusMessage;
        bool bStatusIsError = false;
    };

} // namespace Leon::Editor
