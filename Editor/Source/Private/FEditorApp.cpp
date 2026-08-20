#include "Editor/FEditorApp.hpp"

#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FWindow.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Editor/Utils/FEditorFileDialog.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Gameplay/AActor.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

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
        if (fs::exists(WindowConfigIniPath)) {
            std::ifstream in(WindowConfigIniPath);
            if (in.is_open()) {
                std::string line;
                int w = 1600, h = 900, px = 100, py = 100, maxVal = 0;
                while (std::getline(in, line)) {
                    if (line.rfind("Width=", 0) == 0)
                        w = std::stoi(line.substr(6));
                    else if (line.rfind("Height=", 0) == 0)
                        h = std::stoi(line.substr(7));
                    else if (line.rfind("PosX=", 0) == 0)
                        px = std::stoi(line.substr(5));
                    else if (line.rfind("PosY=", 0) == 0)
                        py = std::stoi(line.substr(5));
                    else if (line.rfind("Maximized=", 0) == 0)
                        maxVal = std::stoi(line.substr(10));
                }
                if (w > 400 && h > 300) {
                    glfwSetWindowSize(Native, w, h);
                    glfwSetWindowPos(Native, px, py);
                }
                if (maxVal == 1) {
                    glfwMaximizeWindow(Native);
                }
            }
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
        Context.GetSelection().RegisterActorSelectionCallback(
            [this](const std::vector<AActor*>&) { SelectedActor = Context.GetSelection().GetPrimarySelectedActor(); });

        ContentBrowser.SetEditorContext(&Context);
        Outliner.SetEditorContext(&Context);
        Details.SetEditorContext(&Context);
        Viewport.SetEditorContext(&Context);

        // Setup Outliner callbacks
        // OnActorSelected: selection was already made by the Outliner internally.
        // We only sync SelectedActor for the Details panel. Do NOT call SelectActor
        // here again (would double-fire selection callbacks) and do NOT focus the
        // camera (outliner click should not move the camera; only double-click should).
        Outliner.SetOnActorSelected([this](AActor* actor) {
            SelectedActor = actor;
        });
        // OnActorFocus: fired by double-click in outliner — this is when we focus.
        Outliner.SetOnActorFocus([this](AActor* actor) {
            if (actor) {
                Viewport.FocusOnActor(actor);
            }
        });

        // Setup Viewport callback
        // OnActorSelected: selection was already made by the Viewport internally via
        // Context->GetSelection().SelectActor(). We only sync SelectedActor, scroll
        // the Outliner to the picked actor, and focus the camera.
        // Do NOT call SelectActor again here — it would double-fire callbacks.
        Viewport.SetOnActorSelected([this](AActor* actor) {
            SelectedActor = actor;
            if (actor) {
                Outliner.ScrollToActor(actor);
                Viewport.FocusOnActor(actor);
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
        if (!fs::exists(ImGuiIniPath)) {
            bNeedResetLayout = true;
        }

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
                OutputLog.AddLog(ELogLevel::Info, "Map",
                                 "Loaded map: " + ActiveMapName + " (" +
                                     std::to_string(EditorWorld->GetAllActors().size()) + " actors)");
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
            return;
        }

        OutputLog.AddLog(ELogLevel::Info, "Build",
                         bInProduction ? "Starting production bake..." : "Starting draft bake...");

        std::string mode = bInProduction ? "production" : "draft";
        std::string script = (fs::path("Scripts") / "bake_lightmaps.py").string();

        std::string cmd = "python " + script + " --project \"" + ActiveProjectPath + "\" --quality " + mode;
#if defined(_WIN32)
        cmd = "start /B " + cmd;
#else
        cmd = cmd + " &";
#endif
        int res = std::system(cmd.c_str());
        (void)res;
        OutputLog.AddLog(ELogLevel::Info, "Build", "Lightmass baker dispatched asynchronously.");
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
        ImGui::DockBuilderDockWindow("Place Actors", dockLeft);

        // Center: Viewport
        ImGui::DockBuilderDockWindow("Viewport", dockMain);

        // Right Top: World Outliner, World Settings, Project Settings
        ImGui::DockBuilderDockWindow("World Outliner", dockRightTop);
        ImGui::DockBuilderDockWindow("World Settings", dockRightTop);
        ImGui::DockBuilderDockWindow("Project Settings", dockRightTop);

        // Right Bottom: Details
        ImGui::DockBuilderDockWindow("Details", dockRightBottom);

        // Bottom: Content Browser, Output Log
        ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
        ImGui::DockBuilderDockWindow("Output Log", dockBottom);

        ImGui::DockBuilderFinish(DockspaceId);
    }

    void FEditorApp::OnUpdate(FTimestep InTs) {
        (void)InTs;
        if (!bImGuiReady) {
            return;
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
            SafeDrawPanel("Toolbar", [&]() { Toolbar.Draw(ActiveProjectDescriptor.ProjectName, ActiveMapName); });

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
            // Global Delete shortcut for active world
            if (EditorWorld && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !ImGui::GetIO().WantTextInput) {
                std::vector<AActor*> actorsToDelete = Context.GetSelection().GetSelectedActors();
                if (actorsToDelete.empty() && SelectedActor) {
                    actorsToDelete.push_back(SelectedActor);
                }
                for (AActor* act : actorsToDelete) {
                    if (act) {
                        EditorWorld->DestroyActor(act);
                    }
                }
                Context.GetSelection().ClearActorSelection();
                SelectedActor = nullptr;
            }
        }

        EndImGuiFrame();
    }

    void FEditorApp::OnShutdown() {
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

        const ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");

        // Setup initial default layout if requested or on first run
        if (bNeedResetLayout || (!bDockspaceInitialized && !fs::exists(ImGuiIniPath))) {
            bDockspaceInitialized = true;
            bNeedResetLayout = false;
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
