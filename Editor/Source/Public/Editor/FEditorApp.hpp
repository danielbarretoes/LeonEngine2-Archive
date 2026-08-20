#pragma once

#include "Core/FApplication.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Editor/FViewportPanel.hpp"
#include "Editor/Panels/FContentBrowserPanel.hpp"
#include "Editor/Panels/FDetailsPanel.hpp"
#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Editor/Panels/FProjectHubPanel.hpp"
#include "Editor/Panels/FToolbarPanel.hpp"
#include "Engine/UWorld.hpp"

#include <string>

namespace Leon::Editor {

    /**
     * @brief Out-of-process Unreal Engine-inspired editor host.
     * Owns UWorld, Project Descriptor, Viewport, Outliner, Details, and Content Browser panels.
     */
    class FEditorApp : public FApplication {
    public:
        FEditorApp();

        void OnInit() override;
        void OnUpdate(FTimestep InTs) override;
        void OnShutdown() override;

        void OpenProject(const std::string& InProjectPath);
        void LoadMap(const std::string& InMapPath);
        void SaveCurrentMap();
        void BakeLightmaps(bool bInProduction);
        void LaunchGame();

    private:
        void BeginImGuiFrame();
        void EndImGuiFrame();
        void DrawDockspace();
        void DrawMenuBar();

        TRef<UWorld> EditorWorld;
        AActor* SelectedActor = nullptr;

        FProjectHubPanel ProjectHub;
        FViewportPanel Viewport;
        FOutlinerPanel Outliner;
        FDetailsPanel Details;
        FContentBrowserPanel ContentBrowser;
        FToolbarPanel Toolbar;

        std::string ActiveProjectPath;
        FProjectDescriptor ActiveProjectDescriptor;
        std::string ActiveMapPath;
        std::string ActiveMapName;

        std::string ImGuiIniPath;
        std::string EditorSavedDir;
        bool bImGuiReady = false;
        bool bShowProjectHub = false;
        bool bDockspaceInitialized = false;
    };

} // namespace Leon::Editor
