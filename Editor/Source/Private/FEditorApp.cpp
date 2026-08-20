#include "Editor/FEditorApp.hpp"

#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/FWindow.hpp"
#include "Engine/FMapSerializer.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <filesystem>
#include <cstdlib>

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

        ImGui::StyleColorsDark();

        // Custom Unreal-style dark theme
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 4.0f;
        style.TabRounding = 3.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.32f, 1.0f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.38f, 1.0f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.27f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.32f, 0.40f, 1.0f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.16f, 0.45f, 0.75f, 1.0f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.34f, 1.0f);
        colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.18f, 1.0f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.32f, 0.40f, 1.0f);
        colors[ImGuiCol_TabActive] = ImVec4(0.22f, 0.24f, 0.30f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.18f, 1.0f);

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

        // Setup Content Browser callback
        ContentBrowser.SetOnMapSelected([this](const std::string& mapPath) { LoadMap(mapPath); });

        // Setup Toolbar callbacks
        Toolbar.SetOnOpenHub([this]() { bShowProjectHub = true; });
        Toolbar.SetOnSaveMap([this]() { SaveCurrentMap(); });
        Toolbar.SetOnBakeDraft([this]() { BakeLightmaps(false); });
        Toolbar.SetOnBakeProduction([this]() { BakeLightmaps(true); });
        Toolbar.SetOnRunGame([this]() { LaunchGame(); });

        // Check command-line argument or environment for direct project boot
        const char* envProj = std::getenv("LEON_PROJECT");
        if (envProj && std::strlen(envProj) > 0 && fs::exists(envProj)) {
            OpenProject(envProj);
            bShowProjectHub = false;
        } else {
            // Default to launcher / welcome screen
            bShowProjectHub = true;
        }

        LE_CORE_INFO("FEditorApp: ImGui editor host ready");
    }

    void FEditorApp::OpenProject(const std::string& InProjectPath) {
        if (InProjectPath.empty() || !fs::exists(InProjectPath)) {
            LE_CORE_ERROR("FEditorApp: Cannot open invalid project path '{0}'", InProjectPath);
            return;
        }

        ActiveProjectPath = fs::canonical(InProjectPath).string();
        FProjectPaths::SetProjectRoot(ActiveProjectPath);

        if (!ActiveProjectDescriptor.Load(ActiveProjectPath)) {
            LE_CORE_ERROR("FEditorApp: Failed to load descriptor from '{0}'", ActiveProjectPath);
            return;
        }

        // Configure Content Browser
        std::string contentDir = FProjectPaths::ProjectContentDir();
        UAssetManager::SetContentRoot(contentDir);
        ContentBrowser.SetContentDirectory(contentDir);

        ProjectHub.AddRecentProject(ActiveProjectPath);

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
        LE_CORE_INFO("FEditorApp: Opened project '{0}'", ActiveProjectDescriptor.ProjectName);
    }

    void FEditorApp::LoadMap(const std::string& InMapPath) {
        if (!fs::exists(InMapPath)) {
            LE_CORE_ERROR("FEditorApp: Map file not found '{0}'", InMapPath);
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
            LE_CORE_INFO("FEditorApp: Successfully loaded map '{0}' ({1} actors)", ActiveMapName,
                         EditorWorld->GetAllActors().size());
        } else {
            LE_CORE_ERROR("FEditorApp: Failed to deserialize map '{0}'", InMapPath);
        }
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
            LE_CORE_INFO("FEditorApp: Saved map to '{0}'", ActiveMapPath);
        } else {
            LE_CORE_ERROR("FEditorApp: Failed to save map to '{0}'", ActiveMapPath);
        }
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
            LE_CORE_INFO("FEditorApp: Launching game executable: {0}", cmd);
            std::system(cmd.c_str());
        } else {
            LE_CORE_WARN("FEditorApp: Game executable not found at '{0}'. Build project first.", gameExe);
        }
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
            Viewport.Draw(EditorWorld.get(), ActiveMapName, SelectedActor);
            Outliner.Draw(EditorWorld.get());
            Details.Draw(SelectedActor);
            ContentBrowser.Draw();
        }

        EndImGuiFrame();
    }

    void FEditorApp::OnShutdown() {
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

        // Setup initial default layout if dockspace is uninitialized
        if (!bDockspaceInitialized && !fs::exists(ImGuiIniPath)) {
            bDockspaceInitialized = true;
            ImGui::DockBuilderRemoveNode(DockspaceId);
            ImGui::DockBuilderAddNode(DockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(DockspaceId, ViewportInfo->WorkSize);

            ImGuiID dockMain = DockspaceId;
            ImGuiID dockTop = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Up, 0.05f, nullptr, &dockMain);
            ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.25f, nullptr, &dockMain);
            ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.55f, nullptr, &dockRight);
            ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

            ImGui::DockBuilderDockWindow("##EditorToolbar", dockTop);
            ImGui::DockBuilderDockWindow("Viewport", dockMain);
            ImGui::DockBuilderDockWindow("World Outliner", dockRight);
            ImGui::DockBuilderDockWindow("Details", dockRightBottom);
            ImGui::DockBuilderDockWindow("Content Browser", dockBottom);
            ImGui::DockBuilderFinish(DockspaceId);
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

            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Project Browser"))
                    bShowProjectHub = true;
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
