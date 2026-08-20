#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuizmo.h>
#include <iostream>
#include <leon/core/Log.h>
#include <leon/core/Paths.h>
#include <leon/core/Window.h>
#include <leon/editor/AssetImport.h>
#include <leon/editor/EditorApp.h>
#include <leon/editor/EditorLevelFactory.h>
#include <leon/editor/EditorOutputLog.h>
#include <leon/editor/EditorPreferences.h>
#include <leon/editor/EditorTheme.h>
#include <leon/editor/EditorToast.h>
#include <leon/editor/EditorWindowIcon.h>
#include <leon/level/LightmapBaker.h>
#include <leon/editor/panels/ViewportPanel.h>
#include <leon/editor/PieAspectFit.h>
#include <leon/editor/ShowcaseEditorLog.h>
#include <leon/level/LevelLoader.h>
#include <leon/render/Scalability.h>
#include <string>

namespace leon::editor {
namespace {

/// Match HUD layout to the letterboxed play rect (not the full editor OS window).
void SyncPieHudViewportSize(Engine& engine, EditorContext& ctx, Window* pieWindow) {
    if (ctx.pieNewWindow && pieWindow != nullptr && pieWindow->Handle() != nullptr) {
        int fbW = 0;
        int fbH = 0;
        pieWindow->GetFramebufferSize(fbW, fbH);
        int drawX = 0;
        int drawY = 0;
        int drawW = fbW;
        int drawH = fbH;
        FitPieViewport(fbW, fbH, ctx.pieAspect, drawX, drawY, drawW, drawH);
        engine.SetHudViewportSize(drawW, drawH);
        return;
    }

    // Selected Viewport: use last fitted logical size × window DPI scale.
    int winW = 0;
    int winH = 0;
    int fbW = 0;
    int fbH = 0;
    engine.GetWindow().GetWindowSize(winW, winH);
    engine.GetWindow().GetFramebufferSize(fbW, fbH);
    const float scaleX = winW > 0 ? static_cast<float>(fbW) / static_cast<float>(winW) : 1.0f;
    const float scaleY = winH > 0 ? static_cast<float>(fbH) / static_cast<float>(winH) : 1.0f;
    const int hudW =
        std::max(1, static_cast<int>(std::lround(static_cast<float>(ctx.viewportWidth) * scaleX)));
    const int hudH =
        std::max(1, static_cast<int>(std::lround(static_cast<float>(ctx.viewportHeight) * scaleY)));
    engine.SetHudViewportSize(hudW, hudH);
}

} // namespace

bool EditorApp::Initialize(Engine& engine) {
    ctx_.engine = &engine;
    ctx_.level = &engine.GetLevel();
    ctx_.world = &world_;
    ctx_.camera = &engine.GetCamera();
    ctx_.renderer = &engine.GetRenderer();
    ctx_.window = &engine.GetWindow();
    ctx_.resources = &engine.GetResources();
    ctx_.catalog = &catalog_;
    engine.GetRenderer().SetResourceCache(&engine.GetResources());
    ctx_.history = &history_;

    engine.SetCursorCaptured(false);
    engine.SetOrbitMouseEnabled(false);
    engine.SetKeyboardOrbitEnabled(false);
    engine.SetSuppressCameraDrag(true);
    ApplyLeonWindowIcon(engine.GetWindow());

    GLFWwindow* glfwWindow = engine.GetWindow().Handle();
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Persist docking / window layout beside the executable (Window → Save Layout).
    imguiIniPath_ = EditorLayout::LayoutIniPath();
    io.IniFilename = imguiIniPath_.c_str();
    ApplyEditorTheme();
    (void)LoadEditorFonts();

    ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    imguiReady_ = true;
    InstallLastRunLogFile();
    EditorOutputLog::Instance().InstallStreamTee();

    projects_.LoadRecents();
    // User prefs (grid, Play mode / Net Mode / players) — beside exe, like layout/recents.
    EditorPreferences::LoadInto(ctx_);
    savedUserPrefs_ = EditorPreferences::Capture(ctx_);
    engine.SetHudStatsVisible(ctx_.showStats);
    welcome_.Reset();
    projectOpen_ = false;
    engine.ApplyDefaultScalability();
    return true;
}

bool EditorApp::OpenProject(Engine& engine, const std::string& projectPath) {
    EditorProjectInfo info;
    std::string err;
    if (!EditorProjectService::ReadProjectInfo(projectPath, info, err)) {
        std::cerr << "Editor: cannot open project: " << err << '\n';
        return false;
    }

    if (ctx_.piePlaying) {
        StopPie(engine);
    }

    layout_.ResetAssetEditors(ctx_);

    ctx_.ClearSelection();
    ctx_.dirty = false;
    ctx_.lightingOutOfDate = false;
    ctx_.levelPath.clear();
    ctx_.pendingOpenPath.clear();
    ctx_.projectPath = info.path;
    ctx_.projectName = info.name;
    ctx_.projectDisplayName = info.displayName;
    ctx_.projectGameDefaultMap = info.gameDefaultMap;
    ctx_.projectEditorStartupMap = info.editorStartupMap;
    ctx_.projectGlobalDefaultGameMode = info.globalDefaultGameMode;
    ctx_.projectTemplateId = info.templateId;
    ctx_.projectDescription = info.description;
    ctx_.projectBuildDedicatedServer = info.buildDedicatedServer;
    ctx_.requestCloseProject = false;
    ctx_.pendingCloseProject = false;
    ctx_.pendingExit = false;
    ctx_.requestContentRefresh = true;
    SetActiveContentRoot(info.path);

    engine.ApplyPackScalability(info.path);

    if (!info.hasEditorScalability) {
        const EditorScalabilitySettings defaults = MakeDefaultEditorScalability();
        std::string scalabilityErr;
        if (!EditorProjectService::WriteProjectEditorScalability(
                info.path, defaults.postProcess, defaults.textureQuality, scalabilityErr)) {
            std::cerr << "Editor: could not persist default DefaultScalability.ini: "
                      << scalabilityErr << '\n';
        }
    }

    if (!catalog_.ScanPack(info.path)) {
        std::cerr << "Editor: project has no levels under '" << info.path << "/Content/Levels'\n";
    }

    // Flow: editorStartupMap → gameDefaultMap → first catalog entry (shipping uses gameDefaultMap
    // only).
    auto resolveCatalogPath = [&](const std::string& authoringKey) -> std::string {
        if (authoringKey.empty() || catalog_.IsEmpty()) {
            return {};
        }
        const std::string key = std::filesystem::path(authoringKey).stem().string();
        const std::size_t idx = catalog_.FindIndexByLevelKey(key);
        if (idx < catalog_.NumEntries()) {
            return catalog_.Entries()[idx].path;
        }
        return {};
    };

    std::string openPath;
    if (!catalog_.IsEmpty()) {
        openPath = resolveCatalogPath(info.editorStartupMap);
        if (openPath.empty()) {
            openPath = resolveCatalogPath(info.gameDefaultMap);
        }
        if (openPath.empty()) {
            openPath = catalog_.Entries().front().path;
        }
    }
    if (!openPath.empty()) {
        if (LoadLevelFile(engine, openPath)) {
            ctx_.levelPath = openPath;
            ctx_.dirty = false;
            ctx_.level = &engine.GetLevel();
            ViewportPanel::FocusLevelBounds(ctx_);
            if (IsShowcaseProject(ctx_.projectName)) {
                LogShowcaseEditorManifest(ctx_, engine.GetLevel());
            }
        } else {
            std::cerr << "Editor: failed to load project level '" << openPath
                      << "' (check materials under the pack / ContentValidator)\n";
            std::string tmplErr;
            if (CreateLevelFromTemplate(ENewLevelTemplate::Blank, engine, tmplErr)) {
                ctx_.levelPath.clear();
                ctx_.dirty = false;
                ctx_.level = &engine.GetLevel();
                EditorToast("Failed to load editor startup map — opened blank level",
                            EEditorToastKind::Error, 6.0f);
            } else {
                EditorToast(tmplErr.empty() ? "Failed to load editor startup map (blank template missing)"
                                            : ("Failed to load editor startup map — " + tmplErr),
                            EEditorToastKind::Error, 6.0f);
            }
        }
    }

    history_.Clear();
    history_.Capture(ctx_);
    projects_.Remember(info.path);
    projectOpen_ = true;
    return true;
}

void EditorApp::CloseProject(Engine& engine) {
    if (ctx_.piePlaying) {
        StopPie(engine);
    }
    // Unreal-like Close Project: discard open packages so Welcome has no dirty tabs.
    layout_.DiscardUnsavedPackages(ctx_);
    ctx_.ClearSelection();
    ctx_.lightingOutOfDate = false;
    ctx_.levelPath.clear();
    ctx_.pendingOpenPath.clear();
    ctx_.projectPath.clear();
    ctx_.projectName.clear();
    ctx_.projectDisplayName.clear();
    ctx_.projectGameDefaultMap.clear();
    ctx_.projectEditorStartupMap.clear();
    ctx_.projectGlobalDefaultGameMode.clear();
    ctx_.projectTemplateId.clear();
    ctx_.projectDescription.clear();
    ctx_.requestCloseProject = false;
    ctx_.pendingCloseProject = false;
    ctx_.pendingExit = false;
    SetActiveContentRoot({});
    catalog_ = LevelCatalog{};
    ctx_.catalog = &catalog_;
    history_.Clear();
    projectOpen_ = false;
    engine.ApplyDefaultScalability();
    projects_.LoadRecents();
    welcome_.Reset();
}

void EditorApp::Shutdown() {
    EditorPreferences::Save(EditorPreferences::Capture(ctx_));
    if (ctx_.piePlaying && ctx_.engine != nullptr) {
        StopPie(*ctx_.engine);
    }
    DestroyPiePresentResources();
    pieTarget_.Destroy();
    if (imguiReady_) {
        if (const char* ini = ImGui::GetIO().IniFilename; ini != nullptr && ini[0] != '\0') {
            ImGui::SaveIniSettingsToDisk(ini);
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    layout_.Viewport().Target().Destroy();
}

void EditorApp::BeginImGuiFrame() {
    ImGuiIO& io = ImGui::GetIO();
    // Selected Viewport PIE: GLFW hides the cursor but keeps feeding mouse coords (often
    // warped to the window center). Without this, ImGui still hovers/clicks menus & docks.
    const bool lockEditorUiMouse = ctx_.piePlaying && !ctx_.pieNewWindow && !ctx_.piePaused;
    if (lockEditorUiMouse) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    if (lockEditorUiMouse) {
        io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
        io.MousePosPrev = ImVec2(-FLT_MAX, -FLT_MAX);
        for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i) {
            io.MouseDown[i] = false;
            io.MouseClicked[i] = false;
            io.MouseDoubleClicked[i] = false;
            io.MouseReleased[i] = false;
        }
        io.MouseWheel = 0.0f;
        io.MouseWheelH = 0.0f;
        io.WantCaptureMouse = false;
    }

    ImGui::NewFrame();
    // Required each frame before Manipulate / IsUsing / IsOver.
    ImGuizmo::BeginFrame();
}

void EditorApp::EndImGuiFrame() {
    ImGui::Render();
    int fbW = 0;
    int fbH = 0;
    ctx_.window->GetFramebufferSize(fbW, fbH);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW, fbH);
    glClearColor(18.0f / 255.0f, 18.0f / 255.0f, 18.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void EditorApp::UpdateWindowTitle(Engine& engine) {
    std::string title = "Leon Editor";
    if (!projectOpen_) {
        title += " — Welcome";
        glfwSetWindowTitle(engine.GetWindow().Handle(), title.c_str());
        return;
    }
    if (!ctx_.projectDisplayName.empty()) {
        title += " — ";
        title += ctx_.projectDisplayName;
    } else if (!ctx_.projectName.empty()) {
        title += " — ";
        title += ctx_.projectName;
    }
    if (ctx_.piePlaying) {
        title += ctx_.piePaused ? " [PAUSED]" : " [PIE]";
    }
    if (!ctx_.levelPath.empty()) {
        title += " — ";
        title += ctx_.levelPath;
    } else if (ctx_.level != nullptr && !ctx_.level->Name().empty()) {
        title += " — ";
        title += ctx_.level->Name();
    }
    if (ctx_.dirty) {
        title += " *";
    }
    glfwSetWindowTitle(engine.GetWindow().Handle(), title.c_str());
}

void EditorApp::SaveEditorCamera(const Camera& camera) {
    editorCameraBackup_.SetMode(camera.Mode());
    editorCameraBackup_.SetTarget(camera.Target());
    editorCameraBackup_.SetEyeLocation(camera.EyeLocation());
    editorCameraBackup_.SetDistance(camera.Distance());
    editorCameraBackup_.SetYawPitch(camera.YawDegrees(), camera.PitchDegrees());
    editorCameraBackup_.SetFieldOfView(camera.FieldOfView());
    editorCameraSaved_ = true;
}

void EditorApp::RestoreEditorCamera(Camera& camera) const {
    if (!editorCameraSaved_) {
        return;
    }
    camera.SetMode(editorCameraBackup_.Mode());
    camera.SetTarget(editorCameraBackup_.Target());
    camera.SetEyeLocation(editorCameraBackup_.EyeLocation());
    camera.SetDistance(editorCameraBackup_.Distance());
    camera.SetYawPitch(editorCameraBackup_.YawDegrees(), editorCameraBackup_.PitchDegrees());
    camera.SetFieldOfView(editorCameraBackup_.FieldOfView());
}

void EditorApp::Run(Engine& engine) {
    if (!imguiReady_) {
        std::cerr << "EditorApp not initialized\n";
        return;
    }

    // Default editor navigation to FreeLook (RMB + WASD).
    {
        Camera& cam = engine.GetCamera();
        const glm::vec3 eye = cam.GetCameraLocation();
        cam.SetMode(ECameraMode::FreeLook);
        cam.SetEyeLocation(eye);
    }

    auto previous = std::chrono::steady_clock::now();
    while (engine.IsRunning() && !engine.GetWindow().ShouldClose()) {
        const auto now = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        engine.GetWindow().PollEvents();
        // OS file manager → editor window: queue Import dialog for supported sources.
        if (projectOpen_ && !ctx_.piePlaying) {
            const std::vector<std::string> dropped = engine.GetWindow().TakeDroppedFiles();
            for (const std::string& path : dropped) {
                if (!IsImportableSourcePath(path)) {
                    EditorToast(std::string("Cannot import: ") + path, EEditorToastKind::Warning,
                                4.0f);
                    continue;
                }
                if (ctx_.pendingImportSourcePath.empty() && !ctx_.requestOpenImportDialog) {
                    ctx_.pendingImportSourcePath = path;
                    ctx_.requestOpenImportDialog = true;
                } else {
                    // Import dialog already busy — stash for next open (ImportDialog::Draw).
                    if (ctx_.pendingImportSourcePath.empty()) {
                        ctx_.pendingImportSourcePath = path;
                    }
                }
            }
        } else {
            (void)engine.GetWindow().TakeDroppedFiles();
        }
        if (ctx_.piePlaying && ctx_.pieNewWindow && pieWindow_.Handle() != nullptr) {
            pieWindow_.PollEvents();
            engine.GetInput().Update(pieWindow_);
        } else {
            engine.GetInput().Update(engine.GetWindow());
        }
        (void)engine.GetRenderer().ReloadShaders(false);

        ctx_.deltaTime = deltaTime;

        if (projectOpen_) {
            if (ctx_.requestCloseProject) {
                CloseProject(engine);
            }
            if (ctx_.requestPieStart && !ctx_.piePlaying) {
                StartPie(engine);
            }
            if (ctx_.piePlaying) {
                TickPendingPieClientSpawns();
            }
            if (ctx_.requestPiePauseToggle && ctx_.piePlaying) {
                ctx_.piePaused = !ctx_.piePaused;
                ctx_.requestPiePauseToggle = false;
                if (ctx_.piePaused) {
                    engine.SetCursorCaptured(false);
                    engine.SetPlayMouseLookActive(false);
                } else if (ctx_.pieNewWindow) {
                    engine.SetCursorCaptured(true);
                    engine.SetPlayMouseLookActive(true);
                    pieWindow_.Focus();
                } else {
                    engine.SetCursorCaptured(true);
                    engine.SetPlayMouseLookActive(true);
                }
            }
            if (ctx_.requestPieStop && ctx_.piePlaying) {
                StopPie(engine);
            }
            if (ctx_.requestBuildLights && ctx_.level != nullptr && !ctx_.piePlaying) {
                ctx_.requestBuildLights = false;
                // Unreal-like: Map BuiltData requires a saved `.llev` path.
                if (ctx_.levelPath.empty()) {
                    if (!layout_.SaveLevel(ctx_, true)) {
                        ctx_.buildLightsStatus = "Build Lighting cancelled — save the level first";
                        EditorToast(ctx_.buildLightsStatus, EEditorToastKind::Warning, 4.5f);
                    }
                }
                if (!ctx_.levelPath.empty()) {
                    const int baked =
                        leon::BuildLighting(*ctx_.level, &ctx_.buildLightsStatus, ctx_.levelPath);
                    bool levelSaved = false;
                    // Persist lightmapId so reopen / Stop PIE keep BuiltData links.
                    if (baked >= 0) {
                        levelSaved = layout_.SaveLevel(ctx_, false);
                        if (levelSaved) {
                            ctx_.buildLightsStatus += " (level saved)";
                        } else {
                            ctx_.MarkDirty();
                        }
                    } else {
                        // In-memory IDs / cleared Movable may diverge from disk.
                        ctx_.MarkDirty();
                    }
                    ctx_.requestContentRefresh = true;
                    EditorLogInfo(ctx_.buildLightsStatus);
                    EEditorToastKind toastKind = EEditorToastKind::Success;
                    if (baked < 0) {
                        toastKind = EEditorToastKind::Error;
                    } else if (baked == 0) {
                        toastKind = EEditorToastKind::Warning;
                    } else if (levelSaved) {
                        ctx_.lightingOutOfDate = false;
                    }
                    EditorToast(ctx_.buildLightsStatus, toastKind, 5.0f);
                    std::cout << ctx_.buildLightsStatus << '\n';
                }
            } else if (ctx_.requestBuildLights) {
                ctx_.requestBuildLights = false;
            }
            {
                const bool esc =
                    ctx_.piePlaying && ctx_.pieNewWindow && pieWindow_.Handle() != nullptr
                        ? pieWindow_.IsKeyPressed(GLFW_KEY_ESCAPE)
                        : engine.GetWindow().IsKeyPressed(GLFW_KEY_ESCAPE);
                if (ctx_.piePlaying && esc && !escapeWasDown_) {
                    StopPie(engine);
                }
                escapeWasDown_ = esc;
            }

            if (ctx_.piePlaying && (pieMode_ != nullptr || pieUsesHostSession_)) {
                if (ctx_.piePaused) {
                    engine.SetPlayMouseLookActive(false);
                    if (engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(false);
                    }
                } else if (ctx_.pieNewWindow) {
                    const bool focused = pieWindow_.Handle() != nullptr && pieWindow_.IsFocused();
                    engine.SetPlayMouseLookActive(focused);
                    if (focused && !engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(true);
                    }
                } else {
                    // Keep look + hidden cursor for the whole Selected Viewport session.
                    // (Hover-only look left the OS cursor visible and edge-clamped.)
                    engine.SetPlayMouseLookActive(true);
                    if (!engine.IsCursorCaptured()) {
                        engine.SetCursorCaptured(true);
                    }
                }
                // Selected Viewport: tick before ImGui so the Viewport DrawScene sees skinned
                // draws. New Window: tick after ImGui (below) — editor panels also call DrawScene
                // and would clear the skeletal queue before RenderPieWindow.
                if (!ctx_.piePaused && !ctx_.pieNewWindow) {
                    SyncPieHudViewportSize(engine, ctx_, nullptr);
                    if (pieUsesHostSession_) {
                        pieSession_.HandleUiInput();
                        engine.TickPlayAudio();
                        pieSession_.Tick(deltaTime);
                        engine.TickPlayHud(deltaTime);
                        if (GameMode* active = pieSession_.Router().GetActive()) {
                            ctx_.world = &active->GetWorld();
                        }
                    } else if (pieMode_ != nullptr) {
                        engine.TickPlayAudio();
                        pieMode_->Tick(engine, deltaTime);
                        engine.TickPlayHud(deltaTime);
                    }
                    if (ctx_.pieNetMode != EEditorPlayNetMode::Standalone) {
                        engine.GetGameInstance().GetNetDriver().Poll();
                    }
                }
            }
        }

        BeginImGuiFrame();
        if (!projectOpen_) {
            welcome_.Draw(projects_);
            if (welcome_.HasPendingOpen()) {
                const std::string path = welcome_.PendingOpenPath();
                welcome_.ClearPendingOpen();
                (void)OpenProject(engine, path);
            }
        } else {
            layout_.Draw(ctx_);
        }
        EndImGuiFrame();
        if (projectOpen_) {
            DebugHotkeyScope scope{};
            scope.stats = true;
            const bool viewportDebug = ctx_.piePlaying ||
                                       (ctx_.viewportFocused && !ctx_.contentBrowserFocused &&
                                        !ImGui::GetIO().WantTextInput);
            scope.collision = viewportDebug;
            scope.navMesh = viewportDebug;
            scope.lights = viewportDebug;
            engine.HandleDebugHotkeys(scope);
            if (ctx_.showStats != engine.IsHudStatsVisible()) {
                engine.SetHudStatsVisible(ctx_.showStats);
            } else {
                ctx_.showStats = engine.IsHudStatsVisible();
            }
        }
        EditorPreferences::SaveIfChanged(ctx_, savedUserPrefs_);

        if (projectOpen_ && ctx_.piePlaying && (pieMode_ != nullptr || pieUsesHostSession_) &&
            !ctx_.piePaused && ctx_.pieNewWindow) {
            SyncPieHudViewportSize(engine, ctx_, &pieWindow_);
            if (pieUsesHostSession_) {
                pieSession_.HandleUiInput();
                engine.TickPlayAudio();
                pieSession_.Tick(deltaTime);
                engine.TickPlayHud(deltaTime);
                if (GameMode* active = pieSession_.Router().GetActive()) {
                    ctx_.world = &active->GetWorld();
                }
            } else if (pieMode_ != nullptr) {
                engine.TickPlayAudio();
                pieMode_->Tick(engine, deltaTime);
                engine.TickPlayHud(deltaTime);
            }
            if (ctx_.pieNetMode != EEditorPlayNetMode::Standalone) {
                engine.GetGameInstance().GetNetDriver().Poll();
            }
        }
        RenderPieWindow(engine);

        UpdateWindowTitle(engine);
        engine.GetWindow().SwapBuffers();
    }

    if (ctx_.piePlaying) {
        StopPie(engine);
    }
}

} // namespace leon::editor
