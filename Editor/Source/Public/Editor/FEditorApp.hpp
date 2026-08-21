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
#include "Editor/Play/FGameModuleLoader.hpp"
#include "Editor/Play/FPlaySession.hpp"
#include "Editor/Play/FPlaySettings.hpp"
#include "Editor/Window/FEditorWindow.hpp"
#include "Engine/UWorld.hpp"

#include <atomic>
#include <cstdint>
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
        explicit FEditorApp(FApplicationCommandLineArgs InArgs = {});

        void OnInit() override;
        void OnUpdate(FTimestep InTs) override;
        void OnShutdown() override;
        void OnEvent(FEvent& InEvent) override;

        void OpenProject(const std::string& InProjectPath);
        void LoadMap(const std::string& InMapPath);
        void SaveCurrentMap();
        void BakeLightmaps(bool bInProduction);
        void LaunchGame();
        void StartPlayInEditor();
        void StopPlayInEditor();
        bool SpawnPieClientProcess(const FPlaySettings& InSettings, int InClientIndex);
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
        void StartAssetImport(const std::string& InSourcePath);
        void PollImportJob();
        void DrawToastOverlay();
        void ShowToast(const std::string& InMessage, bool bInError = false);
        void SyncPlayInEditorCursor();

        enum class EPendingUnsavedAction : uint8_t {
            None = 0,
            LoadMap,
            OpenProject,
            CloseApp,
        };

        [[nodiscard]] bool PromptIfMapDirty(EPendingUnsavedAction InAction, const std::string& InPath = {});
        void DrawUnsavedChangesModal();
        void ExecutePendingUnsavedAction(bool bInSaveFirst);
        void ApplyPendingLoadMap();
        void ApplyPendingLoadMap(const std::string& InMapPath);
        void ApplyPendingOpenProject();
        [[nodiscard]] std::string FormatMapDisplayName() const;

        TRef<UWorld> EditorWorld;

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
        FPlaySession PlaySession;
        FPlaySettings PlaySettings;

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

        // Async Content Browser import (AssetTool via Scripts/import_assets.py)
        std::atomic<bool> bImportRunning{false};
        std::atomic<bool> bImportFinished{false};
        std::atomic<int> ImportExitCode{0};
        std::mutex ImportMutex;
        std::deque<std::string> ImportLogLines;

        // Transient toast (bottom-center, Unreal-like notification)
        std::string ToastMessage;
        float ToastSecondsRemaining = 0.0f;
        bool bToastError = false;

        EPendingUnsavedAction PendingUnsavedAction = EPendingUnsavedAction::None;
        std::string PendingUnsavedPath;
        bool bOpenUnsavedModal = false;

        /** True while PIE GameOnly hides and disables the OS cursor (restored on Stop / UI pause). */
        bool bPlayInEditorCursorHidden = false;

        /** Set when launched as a PIE client child process. */
        bool bPieClientBootstrap = false;
        std::string PieClientProject;
        std::string PieClientMap;
        std::string PieClientHost = "127.0.0.1";
        int PieClientPort = 7777;
    };

} // namespace Leon::Editor

namespace Leon {
    using Editor::FEditorApp;
}
