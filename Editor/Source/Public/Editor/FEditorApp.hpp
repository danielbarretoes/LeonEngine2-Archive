#pragma once

#include "Core/FApplication.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Editor/Context/FEditorContext.hpp"
#include "Editor/FEditorLayoutStore.hpp"
#include "Editor/Gizmos/FTransformGizmo.hpp"
#include "Editor/Panels/FContentBrowserPanel.hpp"
#include "Editor/Panels/FDetailsPanel.hpp"
#include "Editor/Panels/FOutlinerPanel.hpp"
#include "Editor/Panels/FOutputLogPanel.hpp"
#include "Editor/Panels/FPlaceActorsPanel.hpp"
#include "Editor/Panels/FProjectHubPanel.hpp"
#include "Editor/Panels/FProjectSettingsPanel.hpp"
#include "Editor/Panels/FToolbarPanel.hpp"
#include "Editor/Panels/FViewportPanel.hpp"
#include "Editor/Panels/FWorldSettingsPanel.hpp"
#include "Editor/Window/FEditorWindow.hpp"
#include "Engine/UWorld.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

namespace Leon::Editor {

    /**
     * @brief Out-of-process Unreal Engine-inspired editor host.
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
        void ResetDefaultLayout();
        void RequestResetDefaultLayout();
        void RequestLoadNamedLayout(const std::string& InName);
        bool SaveCurrentLayoutAs(const std::string& InName);

        [[nodiscard]] FEditorContext& GetContext() { return Context; }
        [[nodiscard]] const FEditorContext& GetContext() const { return Context; }

    private:
        void BeginImGuiFrame();
        void EndImGuiFrame();
        void DrawDockspace();
        void DrawMenuBar();
        void DrawLayoutMenus();
        void DrawSaveLayoutModal();
        void ApplyPanelVisibility(const FEditorPanelVisibility& InPanels);
        [[nodiscard]] FEditorPanelVisibility CapturePanelVisibility() const;
        void UpdateWindowTitle();
        /** Clears selection first, cancels gizmo, then destroys via undo history. */
        void DeleteSelectedActors();
        /** Duplicate selection with +X offset (Unreal Ctrl+D). */
        void DuplicateSelectedActors();
        void PollBakeJob();
        void DrawToastOverlay();
        void ShowToast(const std::string& InMessage, bool bInError = false);

        TRef<UWorld> EditorWorld;
        AActor* SelectedActor = nullptr;

        FEditorContext Context;

        FProjectHubPanel ProjectHub;
        FViewportPanel Viewport;
        FOutlinerPanel Outliner;
        FDetailsPanel Details;
        FContentBrowserPanel ContentBrowser;
        FToolbarPanel Toolbar;
        FOutputLogPanel OutputLog;
        FPlaceActorsPanel PlaceActors;
        FWorldSettingsPanel WorldSettings;
        FProjectSettingsPanel ProjectSettings;
        FTransformGizmo Gizmo;

        std::string ActiveProjectPath;
        FProjectDescriptor ActiveProjectDescriptor;
        std::string ActiveMapPath;
        std::string ActiveMapName;

        std::string ImGuiIniPath;
        std::string EditorSavedDir;
        std::string WindowConfigIniPath;
        FEditorLayoutStore LayoutStore;
        bool bImGuiReady = false;
        bool bShowProjectHub = false;
        bool bDockspaceInitialized = false;
        bool bNeedResetLayout = false;
        bool bNeedLoadNamedLayout = false;
        bool bNeedFocusContentBrowser = false;
        std::string PendingLayoutName;
        bool bOpenSaveLayoutModal = false;
        char SaveLayoutNameBuffer[64] = {};

        // Panel visibility toggles
        bool bShowViewport = true;
        bool bShowPlaceActors = true;
        bool bShowOutliner = true;
        bool bShowDetails = true;
        bool bShowContentBrowser = true;
        bool bShowOutputLog = true;
        bool bShowWorldSettings = true;
        bool bShowProjectSettings = true;

        // Async lightmap bake status
        std::atomic<bool> bBakeRunning{false};
        std::atomic<bool> bBakeFinished{false};
        std::atomic<int> BakeExitCode{0};
        std::string BakeModeLabel;
        std::mutex BakeMutex;
        std::deque<std::string> BakeLogLines;

        // Transient toast (bottom-center, Unreal-like notification)
        std::string ToastMessage;
        float ToastSecondsRemaining = 0.0f;
        bool bToastError = false;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorApp;
}
