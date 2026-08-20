#include "Editor/FEditorApp.hpp"

#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FWindow.hpp"
#include "Editor/Commands/FDeleteActorsCommand.hpp"
#include "Editor/Commands/FDuplicateActorsCommand.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/Utils/FEditorFileDialog.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Gameplay/AActor.hpp"
#include "Lightmass/FLightmass.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Leon::Editor {

    FEditorApp::FEditorApp()
        : FApplication([] {
              FApplicationProps Props;
              Props.Name = "Leon Engine - Editor";
              Props.WindowWidth = 1600;
              Props.WindowHeight = 900;
              return Props;
          }()) {}

    void FEditorApp::OnInit() {
        GLFWwindow* Native = GetWindow().GetNativeWindow();
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
        // Always open maximized with the default dock layout (ignore previous imgui.ini dock state).
        glfwMaximizeWindow(Native);
        bNeedResetLayout = true;

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

        // Setup Toolbar callbacks
        Toolbar.SetOnOpenHub([this]() { bShowProjectHub = true; });
        Toolbar.SetOnSaveMap([this]() { SaveCurrentMap(); });
        Toolbar.SetOnBakeDraft([this]() { BakeLightmaps(false); });
        Toolbar.SetOnBakeProduction([this]() { BakeLightmaps(true); });
        Toolbar.SetOnRunGame([this]() { LaunchGame(); });
        Toolbar.SetOnResetLayout([this]() { bNeedResetLayout = true; });

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
        if (!envProjStr.empty() && fs::exists(envProjStr)) {
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

        // Editor cwd is usually out/Editor — locate Scripts from the repo root.
        fs::path scriptPath;
        std::vector<fs::path> searchRoots = {fs::current_path()};
        if (!ActiveProjectPath.empty())
            searchRoots.push_back(fs::path(ActiveProjectPath).parent_path());
        for (fs::path r : searchRoots) {
            for (int up = 0; up < 8 && !r.empty(); ++up) {
                fs::path candidate = r / "Scripts" / "bake_lightmaps.py";
                if (fs::exists(candidate)) {
                    scriptPath = fs::absolute(candidate);
                    break;
                }
                if (!r.has_parent_path() || r == r.parent_path())
                    break;
                r = r.parent_path();
            }
            if (!scriptPath.empty())
                break;
        }
        if (scriptPath.empty()) {
            OutputLog.AddLog(ELogLevel::Error, "Build",
                             "Could not find Scripts/bake_lightmaps.py (run the editor from the LeonEngine repo).");
            ShowToast("Bake failed: bake script not found", true);
            return;
        }

        const fs::path repoRoot = scriptPath.parent_path().parent_path();
        std::ostringstream cmd;
#if defined(_WIN32)
        cmd << "cmd /C \"cd /d \"" << repoRoot.string() << "\" && python \"" << scriptPath.string()
            << "\" --project \"" << project << "\" --map \"" << mapPath << "\" --quality " << quality
            << " --force\"";
#else
        cmd << "cd \"" << repoRoot.string() << "\" && python \"" << scriptPath.string() << "\" --project \""
            << project << "\" --map \"" << mapPath << "\" --quality " << quality << " --force";
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

        std::thread([this, command = cmd.str()]() {
            const int res = std::system(command.c_str());
            BakeExitCode = res;
            bBakeFinished = true;
            bBakeRunning = false;
        }).detach();
    }

    void FEditorApp::PollBakeJob() {
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

    void FEditorApp::LaunchGame() {
        if (ActiveProjectPath.empty()) {
            OutputLog.AddLog(ELogLevel::Warning, "Run", "Cannot launch game: no active project.");
            return;
        }

        SaveCurrentMap();

        std::string script = (fs::path("Scripts") / "run_project.py").string();
        std::string cmd = "python " + script + " --project \"" + ActiveProjectPath + "\"";
#if defined(_WIN32)
        cmd = "start " + cmd;
#else
        cmd = cmd + " &";
#endif
        int res = std::system(cmd.c_str());
        (void)res;
        OutputLog.AddLog(ELogLevel::Info, "Run", "Game instance launched.");
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
        ImGui::DockBuilderDockWindow("  Place Actors", dockLeft);

        // Center: Viewport
        ImGui::DockBuilderDockWindow("  Viewport", dockMain);

        // Right Top: World Outliner, World Settings, Project Settings
        ImGui::DockBuilderDockWindow("  World Outliner", dockRightTop);
        ImGui::DockBuilderDockWindow("  World Settings", dockRightTop);
        ImGui::DockBuilderDockWindow("  Project Settings", dockRightTop);

        // Right Bottom: Details
        ImGui::DockBuilderDockWindow("  Details", dockRightBottom);

        // Bottom: Content Browser, Output Log
        ImGui::DockBuilderDockWindow("  Content Browser", dockBottom);
        ImGui::DockBuilderDockWindow("  Output Log", dockBottom);

        ImGui::DockBuilderFinish(DockspaceId);
    }

    void FEditorApp::OnUpdate(FTimestep InTs) {
        (void)InTs;
        if (!bImGuiReady) {
            return;
        }

        // Re-assert maximize after the first frames (display/DPI settle).
        if (!bStartupMaximizeApplied) {
            if (GLFWwindow* native = GetWindow().GetNativeWindow()) {
                if (glfwGetWindowAttrib(native, GLFW_MAXIMIZED) != GLFW_TRUE)
                    glfwMaximizeWindow(native);
            }
            bStartupMaximizeApplied = true;
        }

        BeginImGuiFrame();

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
                    Viewport.Draw(EditorWorld.get(), ActiveMapName, SelectedActor, &bShowViewport);
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
                SafeDrawPanel("WorldSettings", [&]() { WorldSettings.Draw(EditorWorld.get(), &bShowWorldSettings); });
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
                out << "Maximized=1\n";
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

        // Fixed top toolbar (Save / Bake / Play) — not part of the docked panel grid.
        {
            const float toolbarH = 38.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
            if (ImGui::BeginChild("##EditorToolbarStrip", ImVec2(0.0f, toolbarH), false,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                Toolbar.Draw(ActiveProjectDescriptor.ProjectName, ActiveMapName, Context.GetStatusMessage());
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        const ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");

        // Always apply default layout on first dock frame of a session (and when Reset is requested).
        if (bNeedResetLayout || !bDockspaceInitialized) {
            bDockspaceInitialized = true;
            bNeedResetLayout = false;
            bShowViewport = true;
            bShowPlaceActors = true;
            bShowOutliner = true;
            bShowDetails = true;
            bShowContentBrowser = true;
            bShowOutputLog = true;
            bShowWorldSettings = true;
            bShowProjectSettings = true;
            ResetDefaultLayout();
        }

        ImGui::DockSpace(DockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
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
                    bShowViewport = true;
                    bShowPlaceActors = true;
                    bShowOutliner = true;
                    bShowDetails = true;
                    bShowContentBrowser = true;
                    bShowOutputLog = true;
                    bShowWorldSettings = true;
                    bShowProjectSettings = true;
                    bNeedResetLayout = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Build")) {
                if (ImGui::MenuItem("Bake Lightmaps (Draft)", nullptr, false, !ActiveMapPath.empty())) {
                    BakeLightmaps(false);
                }
                if (ImGui::MenuItem("Bake Lightmaps (Production)", nullptr, false, !ActiveMapPath.empty())) {
                    BakeLightmaps(true);
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
