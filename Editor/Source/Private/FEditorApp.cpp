#include "Editor/FEditorApp.hpp"

#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FWindow.hpp"
#include "Editor/Commands/FDeleteActorsCommand.hpp"
#include "Editor/Commands/FDuplicateActorsCommand.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/UI/FEditorWidgets.hpp"
#include "Editor/UI/FLucideIcons.hpp"
#include "Editor/Utils/FEditorFileDialog.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Gameplay/AActor.hpp"
#include "Lightmass/FLightmass.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <cstdlib>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

namespace Leon::Editor {

    namespace {
        /** Editor cwd is usually out/Editor — walk up from cwd / project to find Scripts/<name>. */
        fs::path FindRepoScript(const std::string& InScriptName, const std::string& InProjectPath) {
            std::vector<fs::path> searchRoots = {fs::current_path()};
            if (!InProjectPath.empty())
                searchRoots.push_back(fs::path(InProjectPath).parent_path());
#ifdef _WIN32
            char exePath[MAX_PATH] = {};
            if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0)
                searchRoots.push_back(fs::path(exePath).parent_path());
#endif
            for (fs::path r : searchRoots) {
                for (int up = 0; up < 8 && !r.empty(); ++up) {
                    fs::path candidate = r / "Scripts" / InScriptName;
                    std::error_code ec;
                    if (fs::exists(candidate, ec))
                        return fs::absolute(candidate);
                    if (!r.has_parent_path() || r == r.parent_path())
                        break;
                    r = r.parent_path();
                }
            }
            return {};
        }
    } // namespace

    FEditorApp::FEditorApp(FApplicationCommandLineArgs InArgs)
        : FApplication([InArgs] {
              FApplicationProps Props;
              Props.Name = "Leon Engine Editor";
              Props.WindowWidth = 1600;
              Props.WindowHeight = 900;
              Props.bMaximized = true;
              Props.bHdClientPolicy = false;
              Props.CommandLineArgs = InArgs;
              return Props;
          }()) {
        auto ReadArg = [&](const char* KeyEquals, const char* KeyBare) -> std::string {
            for (int i = 1; i < InArgs.Count; ++i) {
                const char* Raw = InArgs.Args ? InArgs.Args[i] : nullptr;
                if (!Raw)
                    continue;
                std::string Arg = Raw;
                const std::string Prefix = KeyEquals;
                if (Arg.rfind(Prefix, 0) == 0)
                    return Arg.substr(Prefix.size());
                if (Arg == KeyBare && i + 1 < InArgs.Count && InArgs.Args[i + 1])
                    return InArgs.Args[i + 1];
            }
            return {};
        };
        for (int i = 1; i < InArgs.Count; ++i) {
            const char* Raw = InArgs.Args ? InArgs.Args[i] : nullptr;
            if (Raw && std::string(Raw) == "--pie-role=client")
                bPieClientBootstrap = true;
        }
        if (bPieClientBootstrap) {
            PieClientProject = ReadArg("--project=", "--project");
            PieClientMap = ReadArg("--map=", "--map");
            const std::string Host = ReadArg("--pie-host=", "--pie-host");
            if (!Host.empty())
                PieClientHost = Host;
            const std::string Port = ReadArg("--pie-port=", "--pie-port");
            if (!Port.empty())
                PieClientPort = std::atoi(Port.c_str());
        }
    }

    void FEditorApp::OnInit() {
        GLFWwindow* Native = GetWindow().GetNativeWindow();

        std::vector<fs::path> iconCandidates = {
            fs::path("Engine") / "Resources" / "Icon" / "Logo.png",
            fs::path("Editor") / "Resources" / "Icons" / "LeonEditor.png",
            fs::path("Resources") / "Icons" / "LeonEditor.png",
        };
#ifdef _WIN32
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
            const fs::path exeDir = fs::path(exePath).parent_path();
            iconCandidates.push_back(exeDir / "Engine" / "Resources" / "Icon" / "Logo.png");
            iconCandidates.push_back(exeDir / "Editor" / "Resources" / "Icons" / "LeonEditor.png");
            iconCandidates.push_back(exeDir / "Resources" / "Icons" / "LeonEditor.png");
        }
#endif
#ifdef _WIN32
        {
            char* envRoot = nullptr;
            size_t envLen = 0;
            if (_dupenv_s(&envRoot, &envLen, "LEON_ENGINE_ROOT") == 0 && envRoot && envRoot[0] != '\0') {
                iconCandidates.push_back(fs::path(envRoot) / "Engine" / "Resources" / "Icon" / "Logo.png");
                iconCandidates.push_back(fs::path(envRoot) / "Editor" / "Resources" / "Icons" / "LeonEditor.png");
            }
            free(envRoot);
        }
#else
        if (const char* envRoot = std::getenv("LEON_ENGINE_ROOT"); envRoot && envRoot[0] != '\0') {
            iconCandidates.push_back(fs::path(envRoot) / "Engine" / "Resources" / "Icon" / "Logo.png");
            iconCandidates.push_back(fs::path(envRoot) / "Editor" / "Resources" / "Icons" / "LeonEditor.png");
        }
#endif
        for (const fs::path& candidate : iconCandidates) {
            std::error_code existsEc;
            if (fs::exists(candidate, existsEc)) {
                GetWindow().SetIconFromFile(candidate.string());
                break;
            }
        }
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        EditorSavedDir = (fs::path("Editor") / "Saved").string();
        std::error_code Ec;
        fs::create_directories(EditorSavedDir, Ec);
        ImGuiIniPath = (fs::path(EditorSavedDir) / "imgui.ini").string();
        IO.IniFilename = ImGuiIniPath.c_str();

        WindowConfigIniPath = (fs::path(EditorSavedDir) / "EditorWindow.ini").string();
        LayoutStore.Init(EditorSavedDir);
        if (!LayoutStore.GetActiveLayoutName().empty()) {
            const std::string& Active = LayoutStore.GetActiveLayoutName();
            const bool bExists =
                std::find(LayoutStore.GetLayoutNames().begin(), LayoutStore.GetLayoutNames().end(), Active) !=
                LayoutStore.GetLayoutNames().end();
            if (bExists) {
                PendingLayoutName = Active;
                bNeedLoadNamedLayout = true;
                bNeedResetLayout = false;
            } else {
                LayoutStore.ClearActiveLayout();
                bNeedResetLayout = true;
            }
        } else {
            bNeedResetLayout = true;
        }

        // Apply Unreal dark theme and load Inter fonts
        FEditorTheme::ApplyTheme();
        FEditorTheme::LoadFonts(IO, "Editor/Resources/Fonts");

        ImGui_ImplGlfw_InitForOpenGL(Native, true);
        ImGui_ImplOpenGL3_Init("#version 450");
        bImGuiReady = true;

        // Initialize Core Asset Subsystems
        UAssetManager::Init();

        // Setup Project Hub
        ProjectHub.LoadRecentProjects(EditorSavedDir);
        ProjectHub.SetOnProjectSelected([this](const std::string& path) { OpenProject(path); });

        // Setup Context & Panels
        Context.GetSelection().RegisterActorSelectionCallback([this](const std::vector<AActor*>& actors) {
            SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
            // Empty selection (e.g. after Delete): drop gizmo hover/drag so viewport pick works again.
            if (actors.empty()) {
                Viewport.CancelGizmoInteraction();
            }
        });

        ContentBrowser.SetEditorContext(&Context);
        Outliner.SetEditorContext(&Context);
        Details.SetEditorContext(&Context);
        Viewport.SetEditorContext(&Context);

        // Setup Outliner callbacks (Unreal-like):
        // - Single click: select only (viewport outline follows Context selection).
        // - Do NOT move the camera. Do NOT force-scroll (item is already under cursor).
        // - Double-click / Focus menu / F: camera focus via OnActorFocus.
        Outliner.SetOnActorSelected([this](AActor* actor) {
            SelectedActor = actor;
        });
        Outliner.SetOnActorFocus([this](AActor* actor) {
            if (actor) {
                Viewport.FocusOnActor(actor);
            }
        });
        Outliner.SetOnDeleteRequested([this]() { DeleteSelectedActors(); });

        // Setup Viewport callback (Unreal-like):
        // - Click pick already wrote Context selection + draws outline in viewport.
        // - Scroll/reveal the actor in the Outliner (focus the tree row, not the camera).
        // - Camera framing is F / outliner double-click only — never on pick.
        Viewport.SetOnActorSelected([this](AActor* actor) {
            SelectedActor = actor;
            if (actor) {
                Outliner.ScrollToActor(actor);
            }
        });
        Viewport.SetOnActorSpawned([this](AActor* actor) {
            Context.GetSelection().SelectActor(actor, false);
            OutputLog.AddLog(ELogLevel::Info, "World",
                             "Spawned actor via drop: " + (actor ? actor->GetName() : "null"));
        });

        // Setup Place Actors callback
        PlaceActors.SetOnActorSpawned([this](AActor* actor) {
            Context.GetSelection().SelectActor(actor, false);
            Viewport.FocusOnActor(actor);
            OutputLog.AddLog(ELogLevel::Info, "World", "Spawned actor: " + (actor ? actor->GetName() : "null"));
        });

        // Setup Content Browser callback
        ContentBrowser.SetOnMapSelected([this](const std::string& mapPath) { LoadMap(mapPath); });
        ContentBrowser.SetOnSaveAll([this]() {
            SaveCurrentMap();
            if (!ActiveProjectPath.empty()) {
                ActiveProjectDescriptor.Save(ActiveProjectPath);
            }
        });

        // Setup Toolbar callbacks (Play In Editor)
        Toolbar.SetPlaySettings(&PlaySettings);
        Toolbar.SetOnSaveMap([this]() { SaveCurrentMap(); });
        Toolbar.SetOnBakeDraft([this]() { BakeLightmaps(false); });
        Toolbar.SetOnBakeProduction([this]() { BakeLightmaps(true); });
        Toolbar.SetOnPlay([this]() { StartPlayInEditor(); });
        Toolbar.SetOnStop([this]() { StopPlayInEditor(); });

        PlaySession.SetSaveMapCallback([this]() { SaveCurrentMap(); });
        PlaySession.SetLogCallback([this](const std::string& Msg, bool bErr) {
            OutputLog.AddLog(bErr ? ELogLevel::Error : ELogLevel::Info, "PIE", Msg);
            ShowToast(Msg, bErr);
        });
        PlaySession.SetSpawnClientCallback(
            [this](const FPlaySettings& Settings, int ClientIndex) { return SpawnPieClientProcess(Settings, ClientIndex); });

        PlaySettings.LoadFromFile((fs::path(EditorSavedDir) / "PlaySettings.json").string());

        // Mirror engine/editor logs into the Output Log panel (console still prints).
        FLog::SetSink([this](Leon::ELogLevel level, std::string_view tag, std::string_view message) {
            ELogLevel panelLevel = ELogLevel::Info;
            if (level == Leon::ELogLevel::Warn) {
                panelLevel = ELogLevel::Warning;
            } else if (level == Leon::ELogLevel::Error || level == Leon::ELogLevel::Fatal) {
                panelLevel = ELogLevel::Error;
            }
            OutputLog.AddLog(panelLevel, std::string(tag), std::string(message));
        });

        OutputLog.AddLog(ELogLevel::Info, "Editor", "LeonEditor suite ready (Inter typography active)");

        // Check command-line argument or environment for direct project boot
        std::string envProjStr;
#if defined(_WIN32)
        char* envVal = nullptr;
        size_t len = 0;
        if (_dupenv_s(&envVal, &len, "LEON_PROJECT") == 0 && envVal) {
            envProjStr = envVal;
            std::free(envVal);
        }
#else
        if (const char* envVal = std::getenv("LEON_PROJECT")) {
            envProjStr = envVal;
        }
#endif
        if (bPieClientBootstrap && !PieClientProject.empty() && fs::exists(PieClientProject)) {
            OpenProject(PieClientProject);
            if (!PieClientMap.empty() && fs::exists(PieClientMap))
                LoadMap(PieClientMap);
            PlaySettings.NetMode = EPlayNetMode::Client;
            PlaySettings.NumberOfPlayers = 1;
            PlaySettings.PlayMode = EPlayMode::SelectedViewport;
            PlaySettings.ClientAddress = PieClientHost;
            PlaySettings.ListenPort = PieClientPort;
            PlaySettings.bAutoSaveMapBeforePlay = false;
            StartPlayInEditor();
            bShowProjectHub = false;
        } else if (!envProjStr.empty() && fs::exists(envProjStr)) {
            OpenProject(envProjStr);
            bShowProjectHub = false;
        } else {
            // Default to standalone launcher / welcome screen
            bShowProjectHub = true;
        }

        UpdateWindowTitle();
        LE_CORE_INFO("FEditorApp: ImGui editor host ready");
    }

    void FEditorApp::UpdateWindowTitle() {
        GLFWwindow* native = GetWindow().GetNativeWindow();
        if (!native)
            return;

        std::string title = "Leon Engine Editor";
        if (!ActiveProjectDescriptor.ProjectName.empty()) {
            title += " - [" + ActiveProjectDescriptor.ProjectName + "]";
        }
        if (!ActiveMapName.empty()) {
            title += " - " + ActiveMapName;
        }
        glfwSetWindowTitle(native, title.c_str());
    }

    void FEditorApp::OpenProject(const std::string& InProjectPath) {
        if (InProjectPath.empty() || !fs::exists(InProjectPath)) {
            LE_CORE_ERROR("FEditorApp: Cannot open invalid project path '{}'", InProjectPath);
            OutputLog.AddLog(ELogLevel::Error, "Project", "Cannot open invalid project: " + InProjectPath);
            return;
        }

        ActiveProjectPath = fs::canonical(InProjectPath).string();
        FProjectPaths::SetProjectRoot(ActiveProjectPath);

        if (!ActiveProjectDescriptor.Load(ActiveProjectPath)) {
            LE_CORE_ERROR("FEditorApp: Failed to load descriptor from '{}'", ActiveProjectPath);
            OutputLog.AddLog(ELogLevel::Error, "Project", "Failed to parse descriptor: " + ActiveProjectPath);
            return;
        }

        Context.SetActiveProjectPath(ActiveProjectPath);
        ProjectHub.AddRecentProject(ActiveProjectPath);

        const std::string ProjectDir = fs::path(ActiveProjectPath).parent_path().string();
        if (!FGameModuleLoader::LoadForProject(ActiveProjectDescriptor, ActiveProjectPath, ProjectDir)) {
            OutputLog.AddLog(ELogLevel::Warning, "Project",
                             "Game module not loaded — PIE may lack project GameModes/classes.");
        }

        // Update Content Browser root to project Content directory
        std::string contentDir = FProjectPaths::ProjectContentDir();
        ContentBrowser.SetContentDirectory(contentDir);

        // Load Default Map from descriptor
        std::string defaultMap = ActiveProjectDescriptor.DefaultMap;
        if (!defaultMap.empty()) {
            std::string resolvedMap = FProjectPaths::ResolveVirtualPath(defaultMap);
            if (!resolvedMap.ends_with(".lmap")) {
                resolvedMap += ".lmap";
            }
            if (fs::exists(resolvedMap)) {
                LoadMap(resolvedMap);
            } else {
                LE_CORE_WARN("FEditorApp: Default map not found '{}', starting with empty level", resolvedMap);
                LoadMap("");
            }
        } else {
            // Create a blank world
            if (EditorWorld) {
                EditorWorld->EndPlay();
                EditorWorld->Clear();
            }
            EditorWorld = UWorld::Create("EditorWorld");
            EditorWorld->InitWorld();
            ActiveMapPath.clear();
            ActiveMapName = "Untitled";
            Context.SetActiveWorld(EditorWorld.get());
            Context.SetActiveMapPath(ActiveMapPath);
        }

        bShowProjectHub = false;
        UpdateWindowTitle();
        LE_CORE_INFO("FEditorApp: Opened project '{}'", ActiveProjectDescriptor.ProjectName);
    }

    void FEditorApp::LoadMap(const std::string& InMapPath) {
        if (PlaySession.IsPlaying()) {
            ShowToast("Stop Play before loading a map", true);
            return;
        }
        if (!InMapPath.empty() && !fs::exists(InMapPath)) {
            LE_CORE_ERROR("FEditorApp: Map file not found '{}'", InMapPath);
            OutputLog.AddLog(ELogLevel::Error, "Map", "Map not found: " + InMapPath);
            return;
        }

        if (EditorWorld) {
            EditorWorld->EndPlay();
            EditorWorld->Clear();
            EditorWorld.reset();
        }

        EditorWorld = UWorld::Create("EditorWorld");
        EditorWorld->InitWorld();

        if (!InMapPath.empty()) {
            FMapSerializer serializer(EditorWorld);
            if (serializer.Deserialize(InMapPath)) {
                ActiveMapPath = InMapPath;
                ActiveMapName = fs::path(InMapPath).stem().string();
                SelectedActor = nullptr;
                Outliner.SetSelectedActor(nullptr);
                Context.SetActiveWorld(EditorWorld.get());
                Context.SetActiveMapPath(ActiveMapPath);
                Context.GetHistory().Clear();
                OutputLog.AddLog(ELogLevel::Info, "Map",
                                 "Loaded map: " + ActiveMapName + " (" +
                                     std::to_string(EditorWorld->GetAllActors().size()) + " actors)");
                FLightmass::RefreshRuntimeLightmapTrust(*EditorWorld);
                if (!EditorWorld->AreLightmapsTrusted()) {
                    OutputLog.AddLog(ELogLevel::Warning, "Lighting",
                                     "Lightmaps are stale or missing — Static lights fall back to dynamic until you "
                                     "Bake Lighting. For outdoor sun prefer Mobility: Stationary.");
                }
                LE_CORE_INFO("FEditorApp: Successfully loaded map '{}' ({} actors)", ActiveMapName,
                             EditorWorld->GetAllActors().size());
            } else {
                OutputLog.AddLog(ELogLevel::Error, "Map", "Failed to deserialize map: " + InMapPath);
                LE_CORE_ERROR("FEditorApp: Failed to deserialize map '{}'", InMapPath);
            }
        } else {
            ActiveMapPath.clear();
            ActiveMapName = "Untitled";
            SelectedActor = nullptr;
            Outliner.SetSelectedActor(nullptr);
            Context.SetActiveWorld(EditorWorld.get());
            Context.SetActiveMapPath(ActiveMapPath);
        }

        UpdateWindowTitle();
    }

    void FEditorApp::SaveCurrentMap() {
        if (!EditorWorld)
            return;

        if (ActiveMapPath.empty()) {
            std::string contentDir = FProjectPaths::ProjectContentDir();
            std::string mapsDir = (fs::path(contentDir) / "Maps").string();
            fs::create_directories(mapsDir);

            std::string savePath = FEditorFileDialog::SaveFile("Leon Map (*.lmap)\0*.lmap\0", "lmap", mapsDir.c_str());
            if (savePath.empty())
                return;

            ActiveMapPath = savePath;
            ActiveMapName = fs::path(savePath).stem().string();
            Context.SetActiveMapPath(ActiveMapPath);
        }

        FMapSerializer serializer(EditorWorld);
        if (serializer.Serialize(ActiveMapPath)) {
            OutputLog.AddLog(ELogLevel::Info, "Map", "Saved map: " + ActiveMapPath);
            LE_CORE_INFO("FEditorApp: Successfully saved map '{}'", ActiveMapPath);
            UpdateWindowTitle();
        } else {
            OutputLog.AddLog(ELogLevel::Error, "Map", "Failed to save map: " + ActiveMapPath);
            LE_CORE_ERROR("FEditorApp: Failed to save map '{}'", ActiveMapPath);
        }
    }

    void FEditorApp::BakeLightmaps(bool bInProduction) {
        if (ActiveProjectPath.empty() || ActiveMapPath.empty()) {
            OutputLog.AddLog(ELogLevel::Warning, "Build", "Cannot bake lightmaps: no map is active.");
            ShowToast("Cannot bake: no map is active", true);
            return;
        }

        if (bBakeRunning.load()) {
            OutputLog.AddLog(ELogLevel::Warning, "Build", "A lighting build is already running.");
            ShowToast("Lighting build already in progress", true);
            return;
        }

        // Persist map so the baker sees the latest actors.
        SaveCurrentMap();

        const std::string quality = bInProduction ? "Production" : "Draft";
        BakeModeLabel = quality;
        const std::string project = ActiveProjectPath;
        const std::string mapPath = ActiveMapPath;

        const fs::path scriptPath = FindRepoScript("bake_lightmaps.py", ActiveProjectPath);
        if (scriptPath.empty()) {
            OutputLog.AddLog(ELogLevel::Error, "Build",
                             "Could not find Scripts/bake_lightmaps.py (run the editor from the LeonEngine repo).");
            ShowToast("Bake failed: bake script not found", true);
            return;
        }

        const fs::path repoRoot = scriptPath.parent_path().parent_path();
        std::ostringstream cmd;
#if defined(_WIN32)
        // python -u + 2>&1 so progress lines flush and stderr merges into the captured pipe.
        cmd << "cmd /C \"cd /d \"" << repoRoot.string() << "\" && set PYTHONUNBUFFERED=1&& python -u \""
            << scriptPath.string() << "\" --project \"" << project << "\" --map \"" << mapPath
            << "\" --quality " << quality << " --force 2>&1\"";
#else
        cmd << "cd \"" << repoRoot.string() << "\" && PYTHONUNBUFFERED=1 python -u \"" << scriptPath.string()
            << "\" --project \"" << project << "\" --map \"" << mapPath << "\" --quality " << quality
            << " --force 2>&1";
#endif

        OutputLog.AddLog(ELogLevel::Info, "Build",
                         std::string("Starting ") + quality + " lighting build for " + ActiveMapName + "...");
        OutputLog.AddLog(ELogLevel::Info, "Build", "Repo: " + repoRoot.string());
        OutputLog.AddLog(ELogLevel::Info, "Build",
                         "Invoking: python Scripts/bake_lightmaps.py --quality " + quality +
                             " --force --map " + mapPath);
        Context.SetStatusMessage(std::string("Building Lighting (") + quality + ")...");
        ShowToast(std::string("Building Lighting (") + quality + ")...");
        bShowOutputLog = true;

        bBakeRunning = true;
        bBakeFinished = false;
        BakeExitCode = 0;
        {
            std::lock_guard<std::mutex> Lock(BakeMutex);
            BakeLogLines.clear();
        }

        std::thread([this, command = cmd.str()]() {
#if defined(_WIN32)
            FILE* Pipe = _popen(command.c_str(), "rt");
#else
            FILE* Pipe = popen(command.c_str(), "r");
#endif
            if (!Pipe) {
                {
                    std::lock_guard<std::mutex> Lock(BakeMutex);
                    BakeLogLines.push_back("[ERROR] Failed to start lighting build process.");
                }
                BakeExitCode = -1;
                bBakeFinished = true;
                bBakeRunning = false;
                return;
            }

            char Buf[1024];
            while (std::fgets(Buf, sizeof(Buf), Pipe)) {
                std::string Line(Buf);
                while (!Line.empty() && (Line.back() == '\n' || Line.back() == '\r'))
                    Line.pop_back();
                if (Line.empty())
                    continue;
                std::lock_guard<std::mutex> Lock(BakeMutex);
                BakeLogLines.push_back(std::move(Line));
            }

#if defined(_WIN32)
            const int Res = _pclose(Pipe);
#else
            const int Res = pclose(Pipe);
#endif
            // Windows: _pclose returns the process exit code directly with MSVC CRT.
            BakeExitCode = Res;
            bBakeFinished = true;
            bBakeRunning = false;
        }).detach();
    }

    void FEditorApp::PollBakeJob() {
        // Forward Lightmass / script stdout into the editor Output Log while the job runs.
        std::deque<std::string> Pending;
        {
            std::lock_guard<std::mutex> Lock(BakeMutex);
            Pending.swap(BakeLogLines);
        }
        for (const std::string& Line : Pending) {
            ELogLevel Level = ELogLevel::Info;
            if (Line.find("[ERROR]") != std::string::npos || Line.find("ERROR:") != std::string::npos ||
                Line.find("Bake failed") != std::string::npos) {
                Level = ELogLevel::Error;
            } else if (Line.find("[WARN]") != std::string::npos || Line.find("Warning") != std::string::npos) {
                Level = ELogLevel::Warning;
            }
            OutputLog.AddLog(Level, "Build", Line);
        }

        if (!bBakeFinished.exchange(false))
            return;

        const int code = BakeExitCode.load();
        if (code == 0) {
            const std::string msg = BakeModeLabel + " lighting build finished successfully.";
            OutputLog.AddLog(ELogLevel::Info, "Build", msg);

            // Baker stamped the .lmap and wrote .llightmap on disk — hot-reload so the viewport
            // shows the new lighting without a manual map reopen.
            if (!ActiveMapPath.empty()) {
                FPerspectiveCamera& cam = Viewport.GetCamera();
                const glm::vec3 camPos = cam.GetPosition();
                const float camPitch = cam.GetPitch();
                const float camYaw = cam.GetYaw();
                const std::string mapPath = ActiveMapPath;

                UAssetManager::InvalidateLightmaps();
                LoadMap(mapPath);
                // LoadMap already refreshes lightmap trust.
                if (EditorWorld && EditorWorld->AreLightmapsTrusted()) {
                    OutputLog.AddLog(ELogLevel::Info, "Build",
                                     "Lightmaps trusted and active (Static lights use baked lighting).");
                } else if (EditorWorld) {
                    OutputLog.AddLog(ELogLevel::Warning, "Build",
                                     "Bake finished but lightmaps are not trusted — check Output Log / rebake with "
                                     "Force if needed.");
                }

                cam.SetPosition(camPos);
                cam.SetRotation(camPitch, camYaw);
            }

            ShowToast(msg, false);
            Context.SetStatusMessage("Ready");
        } else {
            const std::string msg =
                BakeModeLabel + " lighting build failed (exit " + std::to_string(code) + "). See Output Log.";
            OutputLog.AddLog(ELogLevel::Error, "Build", msg);
            ShowToast(msg, true);
            Context.SetStatusMessage("Lighting build failed");
        }
    }

    void FEditorApp::ShowToast(const std::string& InMessage, bool bInError) {
        ToastMessage = InMessage;
        ToastSecondsRemaining = 5.0f;
        bToastError = bInError;
    }

    void FEditorApp::DrawToastOverlay() {
        if (ToastMessage.empty() || ToastSecondsRemaining <= 0.0f)
            return;

        ToastSecondsRemaining -= ImGui::GetIO().DeltaTime;
        if (ToastSecondsRemaining <= 0.0f) {
            ToastMessage.clear();
            return;
        }

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const ImVec2 pivot(0.5f, 1.0f);
        const ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y - 24.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always, pivot);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("##EditorToast", nullptr, flags)) {
            const ImVec4 col = bToastError ? ImVec4(1.0f, 0.4f, 0.35f, 1.0f) : ImVec4(0.95f, 0.85f, 0.35f, 1.0f);
            ImGui::TextColored(col, "%s", ToastMessage.c_str());
        }
        ImGui::End();
    }

    void FEditorApp::StartPlayInEditor() {
        if (PlaySession.IsPlaying())
            return;
        if (ActiveProjectPath.empty()) {
            ShowToast("Cannot play: no active project", true);
            return;
        }
        if (!FGameModuleLoader::IsLoaded()) {
            ShowToast("Cannot play: game module not loaded", true);
            OutputLog.AddLog(ELogLevel::Error, "PIE", "Game module required for Play In Editor");
            return;
        }

        PlaySettings.Clamp();
        PlaySettings.SaveToFile((fs::path(EditorSavedDir) / "PlaySettings.json").string());

        if (!PlaySession.Start(PlaySettings, ActiveProjectDescriptor, ActiveMapPath, ActiveMapName))
            return;

        Toolbar.SetPlaying(true);
        Viewport.SetPlayingInEditor(true);
    }

    void FEditorApp::StopPlayInEditor() {
        PlaySession.Stop();
        Toolbar.SetPlaying(false);
        Viewport.SetPlayingInEditor(false);
    }

    bool FEditorApp::SpawnPieClientProcess(const FPlaySettings& InSettings, int InClientIndex) {
#ifdef _WIN32
        char ExePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, ExePath, MAX_PATH) == 0)
            return false;

        std::ostringstream Cmd;
        Cmd << "\"" << ExePath << "\""
            << " --pie-role=client"
            << " --pie-host=" << InSettings.ClientAddress << " --pie-port=" << InSettings.ListenPort
            << " --project=\"" << ActiveProjectPath << "\""
            << " --map=\"" << ActiveMapPath << "\""
            << " --pie-client-index=" << InClientIndex;

        STARTUPINFOA Si{};
        Si.cb = sizeof(Si);
        PROCESS_INFORMATION Pi{};
        std::string CmdLine = Cmd.str();
        std::vector<char> Mutable(CmdLine.begin(), CmdLine.end());
        Mutable.push_back('\0');

        if (!CreateProcessA(nullptr, Mutable.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &Si,
                            &Pi)) {
            OutputLog.AddLog(ELogLevel::Error, "PIE", "Failed to spawn PIE client process");
            return false;
        }
        CloseHandle(Pi.hThread);
        PlaySession.RegisterChildProcess(Pi.hProcess);
        OutputLog.AddLog(ELogLevel::Info, "PIE", "Spawned PIE client #" + std::to_string(InClientIndex));
        return true;
#else
        (void)InSettings;
        (void)InClientIndex;
        return false;
#endif
    }

    void FEditorApp::LaunchGame() {
        if (ActiveProjectPath.empty()) {
            OutputLog.AddLog(ELogLevel::Warning, "Run", "Cannot launch game: no active project.");
            ShowToast("Cannot launch: no active project", true);
            return;
        }

        const fs::path scriptPath = FindRepoScript("run_project.py", ActiveProjectPath);
        if (scriptPath.empty()) {
            OutputLog.AddLog(ELogLevel::Error, "Run",
                             "Could not find Scripts/run_project.py (run the editor from the LeonEngine repo).");
            ShowToast("Launch failed: run script not found", true);
            return;
        }

        SaveCurrentMap();

        const fs::path repoRoot = scriptPath.parent_path().parent_path();
        const std::string project = ActiveProjectPath;

        std::ostringstream cmd;
#if defined(_WIN32)
        cmd << "cmd /C start \"\" /D \"" << repoRoot.string() << "\" python \"" << scriptPath.string()
            << "\" --project \"" << project << "\"";
#else
        cmd << "cd \"" << repoRoot.string() << "\" && python \"" << scriptPath.string() << "\" --project \"" << project
            << "\" &";
#endif

        OutputLog.AddLog(ELogLevel::Info, "Run", "Launching packaged game for project: " + project);
        const int res = std::system(cmd.str().c_str());
        if (res != 0) {
            OutputLog.AddLog(ELogLevel::Error, "Run", "Failed to start game process (exit " + std::to_string(res) + ").");
            ShowToast("Launch failed to start", true);
            return;
        }
        ShowToast("Launching packaged game...");
    }

    void FEditorApp::ResetDefaultLayout() {
        ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");
        ImGui::DockBuilderRemoveNode(DockspaceId);
        ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID dockMain = DockspaceId;
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.28f, nullptr, &dockMain);
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
        ImGuiID dockRightTop = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Up, 0.50f, nullptr, &dockRight);
        ImGuiID dockRightBottom = dockRight;

        // Left: Place Actors
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::PlaceActors, dockLeft);

        // Center: Viewport
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::Viewport, dockMain);

        // Right Top: World Outliner, World Settings, Project Settings
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::WorldOutliner, dockRightTop);
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::WorldSettings, dockRightTop);
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::ProjectSettings, dockRightTop);

        // Right Bottom: Details
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::Details, dockRightBottom);

        // Bottom: Output Log then Content Browser last so Content Browser is the active tab.
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::OutputLog, dockBottom);
        ImGui::DockBuilderDockWindow(FPanelWindowTitles::ContentBrowser, dockBottom);

        ImGui::DockBuilderFinish(DockspaceId);
    }

    void FEditorApp::RequestResetDefaultLayout() {
        bNeedLoadNamedLayout = false;
        PendingLayoutName.clear();
        bNeedResetLayout = true;
    }

    void FEditorApp::RequestLoadNamedLayout(const std::string& InName) {
        const std::string Name = FEditorLayoutStore::SanitizeLayoutName(InName);
        if (Name.empty())
            return;
        PendingLayoutName = Name;
        bNeedResetLayout = false;
        bNeedLoadNamedLayout = true;
    }

    bool FEditorApp::SaveCurrentLayoutAs(const std::string& InName) {
        if (!LayoutStore.SaveNamedLayout(InName, CapturePanelVisibility())) {
            ShowToast("Could not save layout (invalid name?)", true);
            return false;
        }
        ShowToast("Layout saved: " + LayoutStore.GetActiveLayoutName());
        OutputLog.AddLog(ELogLevel::Info, "Layout", "Saved layout '" + LayoutStore.GetActiveLayoutName() + "'");
        return true;
    }

    FEditorPanelVisibility FEditorApp::CapturePanelVisibility() const {
        FEditorPanelVisibility Panels;
        Panels.bShowViewport = bShowViewport;
        Panels.bShowPlaceActors = bShowPlaceActors;
        Panels.bShowOutliner = bShowOutliner;
        Panels.bShowDetails = bShowDetails;
        Panels.bShowContentBrowser = bShowContentBrowser;
        Panels.bShowOutputLog = bShowOutputLog;
        Panels.bShowWorldSettings = bShowWorldSettings;
        Panels.bShowProjectSettings = bShowProjectSettings;
        return Panels;
    }

    void FEditorApp::ApplyPanelVisibility(const FEditorPanelVisibility& InPanels) {
        bShowViewport = InPanels.bShowViewport;
        bShowPlaceActors = InPanels.bShowPlaceActors;
        bShowOutliner = InPanels.bShowOutliner;
        bShowDetails = InPanels.bShowDetails;
        bShowContentBrowser = InPanels.bShowContentBrowser;
        bShowOutputLog = InPanels.bShowOutputLog;
        bShowWorldSettings = InPanels.bShowWorldSettings;
        bShowProjectSettings = InPanels.bShowProjectSettings;
    }

    void FEditorApp::OnUpdate(FTimestep InTs) {
        if (!bImGuiReady) {
            return;
        }

        if (PlaySession.IsPlaying())
            PlaySession.Tick(InTs.GetSeconds());
        Toolbar.SetPlaying(PlaySession.IsPlaying());
        Viewport.SetPlayingInEditor(PlaySession.IsPlaying());

        BeginImGuiFrame();

        if (PlaySession.IsPlaying() && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            PlaySession.RequestStop();

        auto SafeDrawPanel = [this](const char* PanelName, auto&& DrawFn) {
            try {
                DrawFn();
            } catch (const std::exception& e) {
                LE_CORE_ERROR("Exception in panel '{}': {}", PanelName, e.what());
                OutputLog.AddLog(ELogLevel::Error, PanelName, std::string("Unhandled exception: ") + e.what());
            } catch (...) {
                LE_CORE_ERROR("Unknown exception in panel '{}'", PanelName);
                OutputLog.AddLog(ELogLevel::Error, PanelName, "Unknown exception caught during render");
            }
        };

        if (bShowProjectHub || ActiveProjectPath.empty()) {
            // Standalone pre-window: Only the Welcome / Project Hub is rendered
            bool bCanReturn = !ActiveProjectPath.empty();
            SafeDrawPanel("ProjectHub", [&]() { ProjectHub.DrawFullscreen(bCanReturn, &bShowProjectHub); });
        } else {
            // Full Editor Suite with Dockspace and Viewport
            SafeDrawPanel("Dockspace", [&]() { DrawDockspace(); });

            // Left / Palette
            if (bShowPlaceActors) {
                SafeDrawPanel("PlaceActors", [&]() { PlaceActors.Draw(EditorWorld.get(), &bShowPlaceActors); });
            }

            // Center Viewport
            if (bShowViewport) {
                SafeDrawPanel("Viewport", [&]() {
                    UWorld* DrawWorld =
                        PlaySession.IsPlaying() ? PlaySession.GetPlayWorld() : EditorWorld.get();
                    Viewport.Draw(DrawWorld, ActiveMapName, SelectedActor, &bShowViewport);
                });
            }

            // Right
            if (bShowOutliner) {
                SafeDrawPanel("Outliner", [&]() { Outliner.Draw(EditorWorld.get(), &bShowOutliner); });
            }
            if (bShowDetails) {
                SafeDrawPanel("Details", [&]() { Details.Draw(SelectedActor, &bShowDetails); });
            }
            if (bShowWorldSettings) {
                SafeDrawPanel("WorldSettings", [&]() {
                    WorldSettings.Draw(EditorWorld.get(), ActiveProjectDescriptor.DefaultGameMode, &bShowWorldSettings);
                });
            }
            if (bShowProjectSettings) {
                SafeDrawPanel("ProjectSettings", [&]() {
                    ProjectSettings.Draw(ActiveProjectDescriptor, ActiveProjectPath, &bShowProjectSettings);
                });
            }

            // Bottom
            if (bShowContentBrowser) {
                SafeDrawPanel("ContentBrowser", [&]() { ContentBrowser.Draw(&bShowContentBrowser); });
            }
            if (bShowOutputLog) {
                SafeDrawPanel("OutputLog", [&]() { OutputLog.Draw(&bShowOutputLog); });
            }

            if (bNeedFocusContentBrowser) {
                ImGui::SetWindowFocus(FPanelWindowTitles::ContentBrowser);
                bNeedFocusContentBrowser = false;
            }

            // Global Edit hotkeys (Unreal-like)
            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantTextInput) {
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                    if (io.KeyShift) {
                        if (Context.GetHistory().CanRedo()) {
                            const std::string desc = Context.GetHistory().GetRedoDescription();
                            Context.GetHistory().Redo();
                            SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
                            OutputLog.AddLog(ELogLevel::Info, "Edit", "Redo: " + desc);
                            ShowToast("Redo: " + desc);
                        }
                    } else if (Context.GetHistory().CanUndo()) {
                        const std::string desc = Context.GetHistory().GetUndoDescription();
                        Context.GetHistory().Undo();
                        SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
                        OutputLog.AddLog(ELogLevel::Info, "Edit", "Undo: " + desc);
                        ShowToast("Undo: " + desc);
                    }
                } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
                    if (Context.GetHistory().CanRedo()) {
                        const std::string desc = Context.GetHistory().GetRedoDescription();
                        Context.GetHistory().Redo();
                        SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
                        OutputLog.AddLog(ELogLevel::Info, "Edit", "Redo: " + desc);
                        ShowToast("Redo: " + desc);
                    }
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                    DeleteSelectedActors();
                } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
                    DuplicateSelectedActors();
                }
            }
        }

        PollBakeJob();
        DrawToastOverlay();

        EndImGuiFrame();
    }

    void FEditorApp::OnShutdown() {
        StopPlayInEditor();
        PlaySettings.SaveToFile((fs::path(EditorSavedDir) / "PlaySettings.json").string());
        FGameModuleLoader::Unload();
        FLog::ClearSink();

        GLFWwindow* native = GetWindow().GetNativeWindow();
        if (native && !WindowConfigIniPath.empty()) {
            bool bMax = (glfwGetWindowAttrib(native, GLFW_MAXIMIZED) == GLFW_TRUE);
            int px = 0, py = 0, w = 1600, h = 900;
            if (!bMax) {
                glfwGetWindowPos(native, &px, &py);
                glfwGetWindowSize(native, &w, &h);
            }
            std::ofstream out(WindowConfigIniPath);
            if (out.is_open()) {
                out << "[EditorWindow]\n";
                out << "Width=" << w << "\n";
                out << "Height=" << h << "\n";
                out << "PosX=" << px << "\n";
                out << "PosY=" << py << "\n";
                out << "Maximized=" << (bMax ? 1 : 0) << "\n";
            }
        }

        if (bImGuiReady) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            bImGuiReady = false;
        }
    }

    void FEditorApp::DeleteSelectedActors() {
        if (!EditorWorld) {
            return;
        }

        std::vector<AActor*> actorsToDelete = Context.GetSelection().GetSelectedActors();
        if (actorsToDelete.empty() && SelectedActor) {
            actorsToDelete.push_back(SelectedActor);
        }
        if (actorsToDelete.empty()) {
            return;
        }

        auto command =
            std::make_unique<FDeleteActorsCommand>(EditorWorld.get(), &Context.GetSelection(), actorsToDelete);
        const std::string desc = command->GetDescription();
        Context.GetHistory().ExecuteCommand(std::move(command));
        Viewport.CancelGizmoInteraction();
        SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
        OutputLog.AddLog(ELogLevel::Info, "Edit", desc + "  (Ctrl+Z to undo)");
        ShowToast(desc);
    }

    void FEditorApp::DuplicateSelectedActors() {
        if (!EditorWorld)
            return;

        std::vector<AActor*> sources = Context.GetSelection().GetSelectedActors();
        if (sources.empty() && SelectedActor)
            sources.push_back(SelectedActor);
        if (sources.empty())
            return;

        auto command =
            std::make_unique<FDuplicateActorsCommand>(EditorWorld.get(), &Context.GetSelection(), sources, 1.0f);
        const std::string desc = command->GetDescription();
        Context.GetHistory().ExecuteCommand(std::move(command));
        Viewport.CancelGizmoInteraction();
        SelectedActor = Context.GetSelection().GetPrimarySelectedActor();
        OutputLog.AddLog(ELogLevel::Info, "Edit", desc + "  (Ctrl+Z to undo)");
        ShowToast(desc);
    }

    void FEditorApp::BeginImGuiFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void FEditorApp::EndImGuiFrame() {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void FEditorApp::DrawDockspace() {
        const ImGuiViewport* ViewportInfo = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ViewportInfo->WorkPos);
        ImGui::SetNextWindowSize(ViewportInfo->WorkSize);
        ImGui::SetNextWindowViewport(ViewportInfo->ID);

        ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                       ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("LeonEditorDockspace", nullptr, WindowFlags);
        ImGui::PopStyleVar(3);

        DrawMenuBar();

        // Fixed top toolbar (Save / Play / Bake) — not part of the docked panel grid.
        {
            constexpr float kMarginX = 10.0f;
            constexpr float kMarginTop = 6.0f;
            constexpr float kMarginBottom = 8.0f;
            constexpr float kInnerPadX = 12.0f;
            constexpr float kInnerPadY = 7.0f;
            constexpr float kBtnH = 26.0f;
            const float toolbarH = kBtnH + kInnerPadY * 2.0f;

            ImGui::Dummy(ImVec2(0.0f, kMarginTop));

            const float availW = ImGui::GetContentRegionAvail().x;
            const float stripW = (availW > kMarginX * 2.0f) ? (availW - kMarginX * 2.0f) : availW;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + kMarginX);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kInnerPadX, kInnerPadY));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 0.0f));
            if (ImGui::BeginChild("##EditorToolbarStrip", ImVec2(stripW, toolbarH), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                Toolbar.Draw(ActiveProjectDescriptor.ProjectName, ActiveMapName, Context.GetStatusMessage());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0.0f, kMarginBottom));
        }

        const ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");

        if (bNeedResetLayout) {
            bDockspaceInitialized = true;
            bNeedResetLayout = false;
            bNeedLoadNamedLayout = false;
            PendingLayoutName.clear();
            // Tear down current dock tree so the bundled Default.ini can rebuild cleanly.
            ImGui::DockBuilderRemoveNode(DockspaceId);
            FEditorPanelVisibility Panels;
            if (LayoutStore.LoadBundledDefaultLayout(Panels)) {
                ApplyPanelVisibility(Panels);
                bNeedFocusContentBrowser = true;
                OutputLog.AddLog(ELogLevel::Info, "Layout", "Reset to default layout");
                ShowToast("Layout: Default");
            } else {
                ApplyPanelVisibility(FEditorPanelVisibility{});
                ResetDefaultLayout();
                LayoutStore.ClearActiveLayout();
                bNeedFocusContentBrowser = true;
                ShowToast("Bundled default missing — used fallback dock layout", true);
            }
        } else if (bNeedLoadNamedLayout) {
            bDockspaceInitialized = true;
            bNeedLoadNamedLayout = false;
            // Tear down current dock tree so the ini snapshot can rebuild cleanly.
            ImGui::DockBuilderRemoveNode(DockspaceId);
            FEditorPanelVisibility Panels;
            if (LayoutStore.LoadNamedLayout(PendingLayoutName, Panels)) {
                ApplyPanelVisibility(Panels);
                OutputLog.AddLog(ELogLevel::Info, "Layout", "Loaded layout '" + PendingLayoutName + "'");
                ShowToast("Layout: " + PendingLayoutName);
            } else {
                ApplyPanelVisibility(FEditorPanelVisibility{});
                ResetDefaultLayout();
                LayoutStore.ClearActiveLayout();
                ShowToast("Layout not found — reset to default", true);
            }
            PendingLayoutName.clear();
        } else if (!bDockspaceInitialized) {
            bDockspaceInitialized = true;
            ApplyPanelVisibility(FEditorPanelVisibility{});
            ResetDefaultLayout();
        }

        ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        DrawSaveLayoutModal();
        ImGui::End();
    }

    void FEditorApp::DrawLayoutMenus() {
        if (ImGui::BeginMenu("Layout")) {
            if (ImGui::MenuItem("Save Layout As...")) {
                const std::string& Active = LayoutStore.GetActiveLayoutName();
                if (!Active.empty()) {
                    std::snprintf(SaveLayoutNameBuffer, sizeof(SaveLayoutNameBuffer), "%s", Active.c_str());
                } else {
                    SaveLayoutNameBuffer[0] = '\0';
                }
                bOpenSaveLayoutModal = true;
            }

            ImGui::Separator();

            const bool bIsDefault = LayoutStore.GetActiveLayoutName().empty();
            if (ImGui::MenuItem("Default", nullptr, bIsDefault)) {
                RequestResetDefaultLayout();
            }

            LayoutStore.RefreshLayoutList();
            const auto& Names = LayoutStore.GetLayoutNames();
            if (!Names.empty()) {
                ImGui::Separator();
                for (const std::string& Name : Names) {
                    const bool bSelected = (LayoutStore.GetActiveLayoutName() == Name);
                    if (ImGui::MenuItem(Name.c_str(), nullptr, bSelected)) {
                        RequestLoadNamedLayout(Name);
                    }
                }
            }

            if (!Names.empty()) {
                ImGui::Separator();
                if (ImGui::BeginMenu("Delete Layout")) {
                    for (const std::string& Name : Names) {
                        if (ImGui::MenuItem(Name.c_str())) {
                            const bool bWasActive = (LayoutStore.GetActiveLayoutName() == Name);
                            if (LayoutStore.DeleteNamedLayout(Name)) {
                                OutputLog.AddLog(ELogLevel::Info, "Layout", "Deleted layout '" + Name + "'");
                                ShowToast("Deleted layout: " + Name);
                                if (bWasActive)
                                    RequestResetDefaultLayout();
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Reset to Default Layout")) {
                RequestResetDefaultLayout();
            }
            ImGui::EndMenu();
        }
    }

    void FEditorApp::DrawSaveLayoutModal() {
        if (bOpenSaveLayoutModal) {
            ImGui::OpenPopup("Save Layout As");
            bOpenSaveLayoutModal = false;
        }

        if (ImGui::BeginPopupModal("Save Layout As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Name for the current dock layout:");
            ImGui::SetNextItemWidth(280.0f);
            const bool bEnter = ImGui::InputText("##SaveLayoutName", SaveLayoutNameBuffer, sizeof(SaveLayoutNameBuffer),
                                                 ImGuiInputTextFlags_EnterReturnsTrue |
                                                     ImGuiInputTextFlags_AutoSelectAll);

            const std::string Sanitized = FEditorLayoutStore::SanitizeLayoutName(SaveLayoutNameBuffer);
            const bool bCanSave = !Sanitized.empty();
            if (!bCanSave && SaveLayoutNameBuffer[0] != '\0') {
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
                                   "Use letters, numbers, spaces, - or _ (not Default/Main).");
            }

            if ((bEnter ||
                 FEditorWidgets::DrawPrimaryButton(ELucideIcon::Save, "##SaveLayout", "Save", ImVec2(120.0f, 0.0f))) &&
                bCanSave) {
                if (SaveCurrentLayoutAs(Sanitized))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (FEditorWidgets::DrawButton(ELucideIcon::X, "##CancelSaveLayout", "Cancel", ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void FEditorApp::DrawMenuBar() {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Project Browser...", "Ctrl+P")) {
                    bShowProjectHub = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Save Current Map", "Ctrl+S", false, !ActiveMapPath.empty())) {
                    SaveCurrentMap();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    Close();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, Context.GetHistory().CanUndo())) {
                    Context.GetHistory().Undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, Context.GetHistory().CanRedo())) {
                    Context.GetHistory().Redo();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false,
                                    Context.GetSelection().GetSelectedActorCount() > 0 || SelectedActor != nullptr)) {
                    DuplicateSelectedActors();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Project Browser"))
                    bShowProjectHub = true;
                ImGui::Separator();
                ImGui::MenuItem("Viewport", nullptr, &bShowViewport);
                ImGui::MenuItem("Place Actors", nullptr, &bShowPlaceActors);
                ImGui::MenuItem("World Outliner", nullptr, &bShowOutliner);
                ImGui::MenuItem("Details", nullptr, &bShowDetails);
                ImGui::MenuItem("Content Browser", nullptr, &bShowContentBrowser);
                ImGui::MenuItem("Output Log", nullptr, &bShowOutputLog);
                ImGui::MenuItem("World Settings", nullptr, &bShowWorldSettings);
                ImGui::MenuItem("Project Settings", nullptr, &bShowProjectSettings);
                ImGui::Separator();
                if (ImGui::MenuItem("Reset to Default Layout")) {
                    RequestResetDefaultLayout();
                }
                ImGui::EndMenu();
            }

            DrawLayoutMenus();

            if (ImGui::BeginMenu("Build")) {
                if (ImGui::MenuItem("Bake Lightmaps (Draft)", nullptr, false, !ActiveMapPath.empty())) {
                    BakeLightmaps(false);
                }
                if (ImGui::MenuItem("Bake Lightmaps (Production)", nullptr, false, !ActiveMapPath.empty())) {
                    BakeLightmaps(true);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Launch Packaged Game...", nullptr, false, !ActiveProjectPath.empty())) {
                    LaunchGame();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About LeonEngine2")) {
                    // Information
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
    }

} // namespace Leon::Editor
