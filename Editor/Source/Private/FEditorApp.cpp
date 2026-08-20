#include "Editor/FEditorApp.hpp"

#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FWindow.hpp"
#include "Editor/UI/FEditorTheme.hpp"
#include "Engine/FMapSerializer.hpp"

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

        // Setup Outliner callbacks
        Outliner.SetOnActorSelected([this](AActor* actor) { SelectedActor = actor; });
        Outliner.SetOnActorFocus([this](AActor* actor) { Viewport.FocusOnActor(actor); });

        // Setup Place Actors callback
        PlaceActors.SetOnActorSpawned([this](AActor* actor) {
            SelectedActor = actor;
            Outliner.SetSelectedActor(actor);
            Viewport.FocusOnActor(actor);
            OutputLog.AddLog(ELogLevel::Info, "World", "Spawned actor: " + (actor ? actor->GetName() : "null"));
        });

        // Setup Content Browser callback
        ContentBrowser.SetOnMapSelected([this](const std::string& mapPath) { LoadMap(mapPath); });

        // Setup Toolbar callbacks
        Toolbar.SetOnOpenHub([this]() { bShowProjectHub = true; });
        Toolbar.SetOnSaveMap([this]() { SaveCurrentMap(); });
        Toolbar.SetOnBakeDraft([this]() { BakeLightmaps(false); });
        Toolbar.SetOnBakeProduction([this]() { BakeLightmaps(true); });
        Toolbar.SetOnRunGame([this]() { LaunchGame(); });
        Toolbar.SetOnResetLayout([this]() { bNeedResetLayout = true; });

        OutputLog.AddLog(ELogLevel::Info, "Editor", "LeonEditor suite ready (Inter typography active)");

        // Check command-line argument or environment for direct project boot
        const char* envProj = std::getenv("LEON_PROJECT");
        if (envProj && std::strlen(envProj) > 0 && fs::exists(envProj)) {
            OpenProject(envProj);
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
            LE_CORE_ERROR("FEditorApp: Cannot open invalid project path '{0}'", InProjectPath);
            OutputLog.AddLog(ELogLevel::Error, "Project", "Cannot open invalid project: " + InProjectPath);
            return;
        }

        ActiveProjectPath = fs::canonical(InProjectPath).string();
        FProjectPaths::SetProjectRoot(ActiveProjectPath);

        if (!ActiveProjectDescriptor.Load(ActiveProjectPath)) {
            LE_CORE_ERROR("FEditorApp: Failed to load descriptor from '{0}'", ActiveProjectPath);
            OutputLog.AddLog(ELogLevel::Error, "Project", "Failed to parse descriptor: " + ActiveProjectPath);
            return;
        }

        // Configure Content Browser
        std::string contentDir = FProjectPaths::ProjectContentDir();
        UAssetManager::SetContentRoot(contentDir);
        ContentBrowser.SetContentDirectory(contentDir);

        ProjectHub.AddRecentProject(ActiveProjectPath);
        OutputLog.AddLog(ELogLevel::Info, "Project", "Opened project: " + ActiveProjectDescriptor.ProjectName);

        // Load Default Map from descriptor
        std::string defaultMap = ActiveProjectDescriptor.DefaultMap;
        std::string resolvedMapPath = FProjectPaths::ResolveVirtualPath(defaultMap);
        if (!fs::exists(resolvedMapPath) && !defaultMap.ends_with(".lmap")) {
            resolvedMapPath += ".lmap";
        }

        if (fs::exists(resolvedMapPath)) {
            LoadMap(resolvedMapPath);
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
        }

        bShowProjectHub = false;
        if (!fs::exists(ImGuiIniPath)) {
            bNeedResetLayout = true;
        }

        UpdateWindowTitle();
        LE_CORE_INFO("FEditorApp: Opened project '{0}'", ActiveProjectDescriptor.ProjectName);
    }

    void FEditorApp::LoadMap(const std::string& InMapPath) {
        if (!fs::exists(InMapPath)) {
            LE_CORE_ERROR("FEditorApp: Map file not found '{0}'", InMapPath);
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

        FMapSerializer serializer(EditorWorld);
        if (serializer.Deserialize(InMapPath)) {
            ActiveMapPath = InMapPath;
            ActiveMapName = fs::path(InMapPath).stem().string();
            SelectedActor = nullptr;
            Outliner.SetSelectedActor(nullptr);
            OutputLog.AddLog(ELogLevel::Info, "Map",
                             "Loaded map: " + ActiveMapName + " (" +
                                 std::to_string(EditorWorld->GetAllActors().size()) + " actors)");
            LE_CORE_INFO("FEditorApp: Successfully loaded map '{0}' ({1} actors)", ActiveMapName,
                         EditorWorld->GetAllActors().size());
        } else {
            OutputLog.AddLog(ELogLevel::Error, "Map", "Failed to deserialize map: " + InMapPath);
            LE_CORE_ERROR("FEditorApp: Failed to deserialize map '{0}'", InMapPath);
        }

        UpdateWindowTitle();
    }

    void FEditorApp::SaveCurrentMap() {
        if (!EditorWorld)
            return;

        if (ActiveMapPath.empty()) {
            if (!ActiveProjectPath.empty()) {
                ActiveMapPath = (fs::path(FProjectPaths::ProjectContentDir()) / "Maps" / "NewMap.lmap").string();
                ActiveMapName = "NewMap";
            } else {
                return;
            }
        }

        FMapSerializer serializer(EditorWorld);
        if (serializer.Serialize(ActiveMapPath)) {
            OutputLog.AddLog(ELogLevel::Info, "Map", "Saved map: " + ActiveMapPath);
            LE_CORE_INFO("FEditorApp: Saved map to '{0}'", ActiveMapPath);
        } else {
            OutputLog.AddLog(ELogLevel::Error, "Map", "Failed to save map: " + ActiveMapPath);
            LE_CORE_ERROR("FEditorApp: Failed to save map to '{0}'", ActiveMapPath);
        }

        UpdateWindowTitle();
    }

    void FEditorApp::BakeLightmaps(bool bInProduction) {
        if (ActiveMapPath.empty() || ActiveProjectPath.empty())
            return;

        std::string toolExe = (fs::path("Tools") / "Lightmass" / "LightmassTool.exe").string();
        if (!fs::exists(toolExe)) {
            toolExe = (fs::path("out") / "Engine" / "_tools" / "Lightmass" / "LightmassTool.exe").string();
        }

        std::string quality = bInProduction ? "production" : "draft";
        std::string cmd = toolExe + " bake --project \"" + ActiveProjectPath + "\" --map \"" + ActiveMapName +
                          "\" --quality " + quality;
        OutputLog.AddLog(ELogLevel::Info, "Lightmass", "Baking lightmaps (" + quality + ")...");
        LE_CORE_INFO("FEditorApp: Launching bake command: {0}", cmd);
        std::system(cmd.c_str());
    }

    void FEditorApp::LaunchGame() {
        if (ActiveProjectPath.empty())
            return;
        std::string projName = ActiveProjectDescriptor.ProjectName;
        std::string gameExe = (fs::path("out") / "Projects" / projName / "_project" / (projName + ".exe")).string();
        if (!fs::exists(gameExe)) {
            gameExe = (fs::path("out") / "Projects" / projName / (projName + ".exe")).string();
        }

        if (fs::exists(gameExe)) {
            std::string cmd = "\"" + gameExe + "\"";
            OutputLog.AddLog(ELogLevel::Info, "Game", "Launching game executable: " + gameExe);
            LE_CORE_INFO("FEditorApp: Launching game executable: {0}", cmd);
            std::system(cmd.c_str());
        } else {
            OutputLog.AddLog(ELogLevel::Warning, "Game", "Game executable not found. Build project first.");
            LE_CORE_WARN("FEditorApp: Game executable not found at '{0}'. Build project first.", gameExe);
        }
    }

    void FEditorApp::ResetDefaultLayout() {
        const ImGuiViewport* ViewportInfo = ImGui::GetMainViewport();
        const ImGuiID DockspaceId = ImGui::GetID("LeonEditorDockspaceId");

        ImGui::DockBuilderRemoveNode(DockspaceId);
        ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(DockspaceId, ViewportInfo->WorkSize);

        ImGuiID dockMain = DockspaceId;

        // 1. Top toolbar strip (over all panels)
        ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.055f, nullptr, &dockMain);

        // 2. Bottom panel (Content Browser + Output Log tabs)
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.28f, nullptr, &dockMain);

        // 3. Left panel (Place Actors palette)
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.18f, nullptr, &dockMain);

        // 4. Right panel (Outliner / Settings / Details)
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.26f, nullptr, &dockMain);
        ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.52f, nullptr, &dockRight);
        ImGuiID dockRightTop = dockRight;

        // Dock windows:
        // Top
        ImGui::DockBuilderDockWindow("##EditorToolbar", dockTop);

        // Left
        ImGui::DockBuilderDockWindow("Place Actors", dockLeft);

        // Center Viewport
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

        if (bShowProjectHub || ActiveProjectPath.empty()) {
            // Standalone pre-window: Only the Welcome / Project Hub is rendered
            bool bCanReturn = !ActiveProjectPath.empty();
            ProjectHub.DrawFullscreen(bCanReturn, &bShowProjectHub);
        } else {
            // Full Editor Suite with Dockspace and Viewport
            DrawDockspace();
            Toolbar.Draw(ActiveProjectDescriptor.ProjectName, ActiveMapName);

            // Left / Palette
            PlaceActors.Draw(EditorWorld.get());

            // Center Viewport
            Viewport.Draw(EditorWorld.get(), ActiveMapName, SelectedActor);

            // Right
            Outliner.Draw(EditorWorld.get());
            Details.Draw(SelectedActor);
            WorldSettings.Draw(EditorWorld.get());
            ProjectSettings.Draw(ActiveProjectDescriptor, ActiveProjectPath);

            // Bottom
            ContentBrowser.Draw();
            OutputLog.Draw();
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

        if (EditorWorld) {
            EditorWorld->EndPlay();
            EditorWorld->Clear();
            EditorWorld.reset();
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
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, History.CanUndo())) {
                    History.Undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, History.CanRedo())) {
                    History.Redo();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Project Browser"))
                    bShowProjectHub = true;
                ImGui::Separator();
                if (ImGui::MenuItem("Reset to Default Layout")) {
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
