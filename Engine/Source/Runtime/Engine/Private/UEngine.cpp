#include "Engine/UEngine.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"
#include "Core/FProjectDescriptor.hpp"
#include "Core/FProjectPaths.hpp"
#include "Core/events/FKeyEvent.hpp"
#include "Core/events/FMouseEvent.hpp"
#include "Assets/FAssetPath.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Renderer/FPerspectiveCamera.hpp"
#include "RHI/FRenderCommand.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <utility>

namespace Leon {

    /**
     * @brief Active game viewport layer: ticks world, renders scene + debug gizmos + HUD/UI, dispatches UI input.
     *
     * Render order per frame:
     *   World 3D → Light gizmos (F2) → AHUD / UUserWidget / PrintString → (FApplication) F1 DebugOverlay → Present
     */
    class FGameViewportLayer : public FLayer {
    public:
        explicit FGameViewportLayer(const TRef<UWorld>& InWorld) : FLayer("GameViewportLayer"), World(InWorld) {}

        void SetWorld(const TRef<UWorld>& InWorld) { World = InWorld; }
        TRef<UWorld> GetWorld() const { return World; }

        void OnAttach() override {
            LE_CORE_INFO("FGameViewportLayer: Attached to active World '{0}'", World ? World->GetName() : "None");
            FUIRenderer::Init();
            UpdateCameraAspect();
            ApplyCursorFromPlayerController();
        }

        void OnDetach() override { LE_CORE_INFO("FGameViewportLayer: Detached from World"); }

        void OnUpdate(FTimestep InTs) override {
            UEngine::Get().ProcessPendingTravel();

            if (!World)
                return;

            FFrameProfiler::BeginFrame();
            FOnScreenDebugMessageManager::Get().Tick(InTs.GetSeconds());

            {
                FFrameProfiler::FScope game(&FFrameProfiler::Working().GameMs);
                World->Tick(InTs);
            }

            ApplyCursorFromPlayerController();
            DispatchUIMouseMove();

            FPerspectiveCamera activeCamera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            APlayerController* pc = World->GetFirstPlayerController();
            if (pc) {
                pc->GetPlayerViewPoint(activeCamera);
            }

            {
                FFrameProfiler::FScope render(&FFrameProfiler::Working().RenderMs);
                World->OnRender(activeCamera);
            }
            DrawLightGizmos(activeCamera);
            {
                FFrameProfiler::FScope ui(&FFrameProfiler::Working().UIMs);
                DrawHUDAndUI();
            }
            FFrameProfiler::EndFrame(InTs.GetMilliseconds());
        }

        void OnEvent(FEvent& InEvent) override {
            FEventDispatcher dispatcher(InEvent);

            dispatcher.Dispatch<FWindowResizeEvent>([this](FWindowResizeEvent& e) {
                if (e.GetWidth() > 0 && e.GetHeight() > 0) {
                    float aspect = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
                    if (World) {
                        APlayerController* pc = World->GetFirstPlayerController();
                        if (pc && pc->GetPlayerCameraManager()) {
                            pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                        }
                        if (FWorldRenderer* renderer = World->GetWorldRenderer()) {
                            renderer->OnViewportResize(e.GetWidth(), e.GetHeight());
                        }
                    }
                }
                return false;
            });

            dispatcher.Dispatch<FMouseButtonPressedEvent>(
                [this](FMouseButtonPressedEvent& e) { return HandleUIMouseButton(e.GetMouseButton(), true); });

            dispatcher.Dispatch<FMouseButtonReleasedEvent>(
                [this](FMouseButtonReleasedEvent& e) { return HandleUIMouseButton(e.GetMouseButton(), false); });

            dispatcher.Dispatch<FKeyPressedEvent>([this](FKeyPressedEvent& e) {
                if (e.IsRepeat() || !World)
                    return false;

                // F1/F2 are owned by FApplication (diagnostics HUD / gizmo toggle).
                // F3-F12 remain render debug views on the scene renderer.
                auto* renderer = World->GetWorldRenderer();
                if (!renderer)
                    return false;

                switch (e.GetKeyCode()) {
                case Key::F3: {
                    bool bWire = !renderer->IsWireframeEnabled();
                    renderer->SetWireframeEnabled(bWire);
                    LE_CORE_INFO("[RENDER DEBUG] Wireframe: {0}", bWire ? "ENABLED" : "DISABLED");
                    return true;
                }
                case Key::F4: {
                    renderer->SetDebugMode(14);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Unlit / Albedo (Base Color)");
                    return true;
                }
                case Key::F5: {
                    renderer->SetDebugMode(11);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: World Normals (TBN Perturbed)");
                    return true;
                }
                case Key::F6: {
                    int currentMode = renderer->GetDebugMode();
                    int nextMode = 16;
                    const char* name = "Roughness";
                    if (currentMode == 16) {
                        nextMode = 15;
                        name = "Metallic";
                    } else if (currentMode == 15) {
                        nextMode = 18;
                        name = "Ambient Occlusion (AO)";
                    }
                    renderer->SetDebugMode(nextMode);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Material Channel ({0})", name);
                    return true;
                }
                case Key::F7: {
                    // Cycle lighting isolation: Dynamic → Baked → Lightmap → LM UV → Dyn+Baked
                    int currentMode = renderer->GetDebugMode();
                    int nextMode = 10;
                    const char* name = "Dynamic Lighting Only (Lo)";
                    if (currentMode == 10) {
                        nextMode = 31;
                        name = "Baked Lighting Only";
                    } else if (currentMode == 31) {
                        nextMode = 32;
                        name = "Lightmap Irradiance (raw)";
                    } else if (currentMode == 32) {
                        nextMode = 33;
                        name = "Lightmap UV (atlas)";
                    } else if (currentMode == 33) {
                        nextMode = 34;
                        name = "Dynamic + Baked (no IBL)";
                    } else if (currentMode == 34) {
                        nextMode = 10;
                        name = "Dynamic Lighting Only (Lo)";
                    }
                    renderer->SetDebugMode(nextMode);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: {0}", name);
                    return true;
                }
                case Key::F8: {
                    renderer->SetDebugMode(9);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Specular IBL & Environment Reflections");
                    return true;
                }
                case Key::F9: {
                    renderer->SetDebugMode(25);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Cascaded Shadow Maps (CSM) False-Color Slices");
                    return true;
                }
                case Key::F10: {
                    renderer->SetDebugMode(24);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Shadow Occlusion Mask");
                    return true;
                }
                case Key::F11: {
                    renderer->SetDebugMode(13);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Real-Time Planar Reflections Buffer");
                    return true;
                }
                case Key::F12: {
                    renderer->SetDebugMode(0);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Lit / Standard PBR Composite");
                    return true;
                }
                default:
                    break;
                }
                return false;
            });
        }

    private:
        void UpdateCameraAspect() {
            if (!FApplication::HasInstance())
                return;
            auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() == 0 || window.GetHeight() == 0 || !World)
                return;
            float aspect = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
            APlayerController* pc = World->GetFirstPlayerController();
            if (pc && pc->GetPlayerCameraManager()) {
                pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
            }
        }

        void ApplyCursorFromPlayerController() {
            if (!World || !FApplication::HasInstance())
                return;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc)
                return;

            bool bShowCursor = pc->ShouldShowMouseCursor();
            if (pc->GetInputMode() == EInputMode::GameOnly) {
                bShowCursor = false;
            }
            FApplication::Get().GetWindow().SetCursorVisible(bShowCursor);
        }

        void DispatchUIMouseMove() {
            if (!World)
                return;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc || !pc->IsUIInputAllowed())
                return;

            AHUD* hud = pc->GetHUD();
            if (!hud)
                return;

            auto [mx, my] = FInput::GetMousePosition();
            hud->OnMouseMove({mx, my});
        }

        bool HandleUIMouseButton(int InButton, bool bPressed) {
            if (!World)
                return false;
            APlayerController* pc = World->GetFirstPlayerController();
            if (!pc || !pc->IsUIInputAllowed())
                return false;

            AHUD* hud = pc->GetHUD();
            if (!hud)
                return false;

            auto [mx, my] = FInput::GetMousePosition();
            glm::vec2 pos{mx, my};
            if (bPressed) {
                return hud->OnMouseButtonDown(InButton, pos);
            }
            return hud->OnMouseButtonUp(InButton, pos);
        }

        void DrawLightGizmos(const FPerspectiveCamera& InCamera) {
            if (!World || !FApplication::HasInstance())
                return;
            const bool bGizmos = FApplication::Get().IsLightGizmosEnabled();
            const bool bGameplay = FApplication::Get().IsGameplayDebugEnabled();
            FDebugRenderer::SetTraceCaptureEnabled(bGameplay);
            if (!bGizmos && !bGameplay)
                return;

            FDebugRenderer::BeginScene(InCamera);
            auto& reg = World->GetRegistry();

            if (bGizmos) {
                auto dirView = reg.view<UDirectionalLightComponent, FTransformComponent>();
                for (auto entity : dirView) {
                    auto [dirComp, transform] = dirView.get<UDirectionalLightComponent, FTransformComponent>(entity);
                    if (dirComp.bEnabled) {
                        FDebugRenderer::DrawDirectionalLightGizmo(dirComp.Light, transform.Translation, 2.5f);
                    }
                }

                auto pointView = reg.view<UPointLightComponent, FTransformComponent>();
                for (auto entity : pointView) {
                    auto [pointComp, transform] = pointView.get<UPointLightComponent, FTransformComponent>(entity);
                    if (pointComp.bEnabled) {
                        FDebugRenderer::DrawPointLightGizmo(pointComp.Light);
                    }
                }

                auto spotView = reg.view<USpotLightComponent, FTransformComponent>();
                for (auto entity : spotView) {
                    auto [spotComp, transform] = spotView.get<USpotLightComponent, FTransformComponent>(entity);
                    if (spotComp.bEnabled) {
                        FDebugRenderer::DrawSpotLightGizmo(spotComp.Light);
                    }
                }
            }

            if (bGameplay)
                FDebugRenderer::DrawQueuedTraces();

            FDebugRenderer::EndScene();
            FDebugRenderer::ClearQueuedTraces();
        }

        void DrawHUDAndUI() {
            if (!World)
                return;

            APlayerController* pc = World->GetFirstPlayerController();
            if (pc) {
                if (AHUD* hud = pc->GetHUD()) {
                    hud->DrawHUD();
                    return;
                }
            }

            if (!FApplication::HasInstance())
                return;
            auto& window = FApplication::Get().GetWindow();
            uint32_t w = window.GetWidth();
            uint32_t h = window.GetHeight();
            if (w == 0 || h == 0)
                return;

            FUIRenderer::Begin(w, h);
            FOnScreenDebugMessageManager::Get().Draw(static_cast<float>(w), static_cast<float>(h));
            FUIRenderer::End();
        }

        TRef<UWorld> World;
    };

    static UEngine* EngineInstance = nullptr;
    static UEngine::FGameInstanceFactory GGameInstanceFactory;
    static std::string GStartupMapOverride;
    static std::string GStartupGameModeOverride;

    void UEngine::SetGameInstanceFactory(FGameInstanceFactory InFactory) {
        GGameInstanceFactory = std::move(InFactory);
    }

    void UEngine::SetStartupOverrides(const std::string& InMapPath, const std::string& InGameModeClass) {
        GStartupMapOverride = InMapPath;
        GStartupGameModeOverride = InGameModeClass;
    }

    UEngine::UEngine() : UObject("Engine") {
        EngineInstance = this;
    }

    UEngine::~UEngine() {
        EngineInstance = nullptr;
    }

    UEngine& UEngine::Get() {
        LE_CORE_ASSERT(EngineInstance != nullptr, "UEngine not instantiated!");
        return *EngineInstance;
    }

    bool UEngine::HasInstance() {
        return EngineInstance != nullptr;
    }

    int UEngine::Run(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        std::string projectPath = InProjectOrConfigPath;

        // Resolve from argv when the caller did not pass an explicit path
        if (projectPath.empty() && InArgs.Args) {
            for (int i = 1; i < InArgs.Count; ++i) {
                const char* a = InArgs.Args[i];
                if (!a)
                    continue;
                std::string arg(a);
                if (arg.rfind("--project=", 0) == 0) {
                    projectPath = arg.substr(10);
                    break;
                }
                if (arg.rfind("-project=", 0) == 0) {
                    projectPath = arg.substr(9);
                    break;
                }
                if (arg == "--project" || arg == "-project") {
                    if (i + 1 < InArgs.Count && InArgs.Args[i + 1]) {
                        projectPath = InArgs.Args[i + 1];
                        break;
                    }
                }
                if (arg.size() > 9 && arg.rfind(".lproject") == arg.size() - 9) {
                    projectPath = arg;
                    break;
                }
            }
        }

        if (projectPath.empty()) {
            // Last resort: discover any .lproject from cwd / Engine sibling
            projectPath = FProjectPaths::LocateProjectFile("");
        }

        if (projectPath.empty()) {
            // Log may not be initialized yet
            fprintf(stderr, "UEngine::Run: no .lproject specified. Pass --project=<path> or InProjectOrConfigPath.\n");
            return 1;
        }

        UEngine engine;
        return engine.InternalRun(InArgs, projectPath);
    }

    FGameModeConfig UEngine::BuildGameModeConfig(const FConfigFile& InEngineConfig, const FConfigFile& InGameConfig,
                                                 const FProjectDescriptor& InProjectDesc) {
        FGameModeConfig config;

        const std::string name = InProjectDesc.ProjectName.empty() ? "Game" : InProjectDesc.ProjectName;
        const std::string projectSection = "/Script/" + name + ".GameMode";

        auto readGameClass = [&](const char* key, const std::string& fallback) {
            std::string v = InGameConfig.GetString(projectSection, key, "");
            if (v.empty()) {
                v = InGameConfig.GetString("/Script/Engine.GameModeBase", key,
                                           InEngineConfig.GetString("/Script/Engine.GameModeBase", key, fallback));
            }
            return v;
        };

        config.GameModeClass = InEngineConfig.GetString(
            "/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode",
            InProjectDesc.DefaultGameMode.empty() ? "AGameModeBase" : InProjectDesc.DefaultGameMode);

        config.DefaultPawnClass = readGameClass("DefaultPawnClass", "ADefaultPawn");
        config.PlayerControllerClass = readGameClass("PlayerControllerClass", "APlayerController");
        config.HUDClass = readGameClass("HUDClass", "AHUD");
        config.GameStateClass = readGameClass("GameStateClass", "AGameStateBase");
        config.PlayerStateClass = readGameClass("PlayerStateClass", "APlayerState");

        std::string gmOverride = InGameConfig.GetString(projectSection, "GameModeClass", "");
        if (!gmOverride.empty())
            config.GameModeClass = gmOverride;

        return config;
    }

    std::string UEngine::ResolveStartupMap(const FConfigFile& InEngineConfig, const FProjectDescriptor& InProjectDesc) {
        return InEngineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap",
                                        InProjectDesc.DefaultMap.empty() ? "/Game/Maps/Empty"
                                                                         : InProjectDesc.DefaultMap);
    }

    void UEngine::RequestTravel(const std::string& InLevelName) {
        PendingTravelMap = InLevelName;
        bPendingTravel = true;
        LE_CORE_INFO("UEngine: Travel requested to '{0}'", InLevelName);
    }

    void UEngine::ProcessPendingTravel() {
        if (!bPendingTravel)
            return;
        bPendingTravel = false;
        std::string target = PendingTravelMap;
        PendingTravelMap.clear();
        TravelToMap(target);
    }

    void UEngine::BindSession(const TRef<UGameInstance>& InGI, const TRef<UWorld>& InWorld) {
        GameInstance = InGI;
        ActiveWorld = InWorld;
        if (GameInstance)
            GameInstance->SetWorld(ActiveWorld);
        if (ActiveWorld && GameInstance)
            ActiveWorld->SetNetMode(GameInstance->GetNetMode());
    }

    void UEngine::ApplyGameModeConfig(AGameModeBase* InGameMode) const {
        if (!InGameMode)
            return;
        InGameMode->DefaultPawnClass = GameModeConfig.DefaultPawnClass;
        InGameMode->PlayerControllerClass = GameModeConfig.PlayerControllerClass;
        InGameMode->HUDClass = GameModeConfig.HUDClass;
        InGameMode->GameStateClass = GameModeConfig.GameStateClass;
        InGameMode->PlayerStateClass = GameModeConfig.PlayerStateClass;
    }

    bool UEngine::LoadMapIntoWorld(const TRef<UWorld>& InWorld, const std::string& InVirtualMapPath) {
        if (!InWorld)
            return false;

        std::string physicalMapPath = FProjectPaths::ResolveVirtualPath(InVirtualMapPath);
        if (!std::filesystem::exists(physicalMapPath)) {
            LE_CORE_ERROR("UEngine: Map '{0}' resolved to '{1}' not found", InVirtualMapPath, physicalMapPath);
            return false;
        }

        FMapSerializer serializer(InWorld);
        if (!serializer.Deserialize(physicalMapPath)) {
            LE_CORE_ERROR("UEngine: Failed to parse map '{0}'", physicalMapPath);
            return false;
        }

        LE_CORE_INFO("UEngine: Loaded map '{0}' ({1}) with {2} actors", InVirtualMapPath, physicalMapPath,
                     InWorld->GetAllActors().size());
        return true;
    }

    bool UEngine::LoadMapIntoActiveWorld(const std::string& InVirtualMapPath) {
        if (!LoadMapIntoWorld(ActiveWorld, InVirtualMapPath))
            return false;
        CurrentMapName = InVirtualMapPath;
        return true;
    }

    bool UEngine::TravelToMap(const std::string& InVirtualMapPath) {
        LE_CORE_INFO("UEngine: Traveling to '{0}'...", InVirtualMapPath);

        // Flow: load into a new UWorld first. Only on success: EndPlay+release old world, rebind, InitWorld/BeginPlay.
        auto newWorld = UWorld::Create("MainWorld");
        newWorld->SetProjectRendererDefaults(ProjectShadowMapResolution, bProjectEnablePlanarReflection,
                                             ProjectCascadeCount, ProjectShadowDistance);
        if (GameInstance)
            newWorld->SetNetMode(GameInstance->GetNetMode());

        if (!LoadMapIntoWorld(newWorld, InVirtualMapPath)) {
            LE_CORE_ERROR("UEngine: Travel aborted; keeping current world (map '{0}' failed to load)",
                          InVirtualMapPath);
            return false;
        }

        if (ActiveWorld) {
            ActiveWorld->EndPlay();
            ActiveWorld->Clear();
        }

        if (ViewportLayer) {
            FOnScreenDebugMessageManager::Get().Clear();
            FUIRenderer::Shutdown();
        }

        ActiveWorld = newWorld;
        CurrentMapName = InVirtualMapPath;
        if (GameInstance) {
            GameInstance->SetWorld(ActiveWorld);
            GameInstance->SetTravelURL(InVirtualMapPath);
        }
        if (ViewportLayer) {
            ViewportLayer->SetWorld(ActiveWorld);
        }

        UAssetManager::UnloadUnused();

        AGameModeBase* gameMode = nullptr;
        if (ActiveWorld->GetNetMode() != ENetMode::Client) {
            if (!GameModeConfig.GameModeClass.empty() && UClassRegistry::Get().HasClass(GameModeConfig.GameModeClass)) {
                gameMode = dynamic_cast<AGameModeBase*>(UClassRegistry::Get().CreateActorOfClass(
                    GameModeConfig.GameModeClass, ActiveWorld.get(), "GameMode"));
            }
            if (!gameMode) {
                gameMode = ActiveWorld->SpawnActor<AGameModeBase>("GameMode");
            }
            ApplyGameModeConfig(gameMode);
            ActiveWorld->SetGameMode(gameMode);
        }

        if (ViewportLayer)
            FUIRenderer::Init();
        ActiveWorld->InitWorld();
        ActiveWorld->BeginPlay();

        if (ViewportLayer && FApplication::HasInstance()) {
            auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() > 0 && window.GetHeight() > 0) {
                float aspect = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
                APlayerController* pc = ActiveWorld->GetFirstPlayerController();
                if (pc && pc->GetPlayerCameraManager()) {
                    pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                }
                if (FWorldRenderer* renderer = ActiveWorld->GetWorldRenderer()) {
                    renderer->OnViewportResize(window.GetWidth(), window.GetHeight());
                }
            }
        }

        LE_CORE_INFO("UEngine: Travel complete — map '{0}' is live", InVirtualMapPath);
        return true;
    }

    int UEngine::InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        LE_CORE_INFO("==================================================");
        LE_CORE_INFO("       LeonEngine2 - Unreal Architecture          ");
        LE_CORE_INFO("==================================================");

        // 1. Resolve Project Descriptor (.lproject) & Project Paths
        std::string projectOrConfigPath = FProjectPaths::LocateProjectFile(InProjectOrConfigPath);
        if (projectOrConfigPath.empty()) {
            projectOrConfigPath = InProjectOrConfigPath;
        }

        FProjectDescriptor projectDesc;
        if (std::filesystem::exists(projectOrConfigPath) &&
            projectOrConfigPath.rfind(".lproject") == projectOrConfigPath.length() - 9) {
            if (projectDesc.Load(projectOrConfigPath)) {
                LE_CORE_INFO("UEngine: Loaded project descriptor '{0}' (Project: {1}, EngineVersion: {2})",
                             projectOrConfigPath, projectDesc.ProjectName, projectDesc.EngineVersion);
            }
            FProjectPaths::SetProjectRoot(projectOrConfigPath);
        } else {
            LE_CORE_ERROR("UEngine: Could not locate project file from hint '{0}'", InProjectOrConfigPath);
            return 1;
        }

        UAssetManager::SetContentRoot(FProjectPaths::ProjectContentDir());
        LE_CORE_INFO("UEngine: Project Root: '{0}', Content Root: '{1}'", FProjectPaths::ProjectDir(),
                     FProjectPaths::ProjectContentDir());

        // 2. Load Multi-INI Configuration Hierarchy
        FConfigFile engineConfig;
        FConfigFile gameConfig;
        FConfigFile inputConfig;

        std::string engineIniPath = FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultEngine.ini");
        if (std::filesystem::exists(engineIniPath)) {
            engineConfig.Load(engineIniPath);
            LE_CORE_INFO("UEngine: Successfully parsed '{0}'", engineIniPath);
        }

        std::string gameIniPath = FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultGame.ini");
        if (std::filesystem::exists(gameIniPath)) {
            gameConfig.Load(gameIniPath);
            LE_CORE_INFO("UEngine: Successfully parsed '{0}'", gameIniPath);
        }

        std::string inputIniPath = FAssetPath::Combine(FProjectPaths::ProjectConfigDir(), "DefaultInput.ini");
        if (std::filesystem::exists(inputIniPath)) {
            inputConfig.Load(inputIniPath);
            InputSettings.LoadFromConfig(inputConfig);
            FInputSettings::Set(InputSettings);
            LE_CORE_INFO("UEngine: Successfully parsed '{0}'", inputIniPath);
        } else {
            FInputSettings::Set(InputSettings);
        }

        std::string windowTitle =
            engineConfig.GetString("/Script/Engine.DisplaySettings", "WindowTitle",
                                   projectDesc.ProjectName.empty() ? "LeonEngine2" : projectDesc.ProjectName);
        uint32_t windowWidth =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowWidth", 1280));
        uint32_t windowHeight =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowHeight", 720));
        bool bVSync = engineConfig.GetBool("/Script/Engine.DisplaySettings", "VSync", true);
        bool bFullscreen = engineConfig.GetBool("/Script/Engine.DisplaySettings", "Fullscreen", false);

        // Renderer project defaults (map skybox Exposure/SunIntensity are not overwritten)
        ProjectShadowMapResolution =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.RendererSettings", "ShadowMapResolution", 2048));
        bProjectEnablePlanarReflection =
            engineConfig.GetBool("/Script/Engine.RendererSettings", "EnablePlanarReflection", true);
        ProjectCascadeCount =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.RendererSettings", "CascadeCount", 4));
        ProjectShadowDistance = engineConfig.GetFloat("/Script/Engine.RendererSettings", "ShadowDistance", 100.0f);
        if (engineConfig.HasKey("/Script/Engine.RendererSettings", "Exposure") ||
            engineConfig.HasKey("/Script/Engine.RendererSettings", "SunIntensity")) {
            LE_CORE_INFO("UEngine: RendererSettings Exposure/SunIntensity are map-owned; INI values are not applied "
                         "over .lmap skybox/lights");
        }

        std::string rawMapPath = ResolveStartupMap(engineConfig, projectDesc);
        GameModeConfig = BuildGameModeConfig(engineConfig, gameConfig, projectDesc);
        if (!GStartupMapOverride.empty())
            rawMapPath = GStartupMapOverride;
        if (!GStartupGameModeOverride.empty())
            GameModeConfig.GameModeClass = GStartupGameModeOverride;
        auto readArgValue = [&](const char* keyEquals, const char* keyBare) -> std::string {
            const std::string prefix = keyEquals;
            for (int i = 1; i < InArgs.Count; ++i) {
                const char* raw = InArgs.Args ? InArgs.Args[i] : nullptr;
                if (!raw)
                    continue;
                std::string arg = raw;
                if (arg.rfind(prefix, 0) == 0)
                    return arg.substr(prefix.size());
                if (arg == keyBare && i + 1 < InArgs.Count && InArgs.Args[i + 1])
                    return InArgs.Args[i + 1];
            }
            return {};
        };
        if (const std::string mapArg = readArgValue("--map=", "--map"); !mapArg.empty())
            rawMapPath = mapArg;
        if (const std::string gmArg = readArgValue("--gamemode=", "--gamemode"); !gmArg.empty())
            GameModeConfig.GameModeClass = gmArg;

        // 3. Initialize FApplication
        FApplicationProps appProps;
        appProps.Name = windowTitle;
        appProps.CommandLineArgs = InArgs;
        appProps.WindowWidth = windowWidth;
        appProps.WindowHeight = windowHeight;

        auto app = CreateScope<FApplication>(appProps);
        app->GetWindow().SetVSync(bVSync);
        if (bFullscreen) {
            app->GetWindow().SetFullscreen(true);
        }

        // 4. Create UGameInstance & UWorld
        if (GGameInstanceFactory)
            GameInstance = GGameInstanceFactory();
        if (!GameInstance)
            GameInstance = CreateRef<UGameInstance>("GameInstance");
        ActiveWorld = UWorld::Create("MainWorld");
        ActiveWorld->SetProjectRendererDefaults(ProjectShadowMapResolution, bProjectEnablePlanarReflection,
                                                ProjectCascadeCount, ProjectShadowDistance);
        GameInstance->SetWorld(ActiveWorld);
        GameInstance->Init();

        // 5. Load Map (.lmap) — fatal on initial boot if missing
        if (!LoadMapIntoActiveWorld(rawMapPath)) {
            LE_CORE_ERROR("UEngine: Fatal — default map '{0}' failed to load", rawMapPath);
            return 1;
        }

        // 6. Instantiate and Configure GameMode
        AGameModeBase* gameMode = nullptr;
        if (!GameModeConfig.GameModeClass.empty() && UClassRegistry::Get().HasClass(GameModeConfig.GameModeClass)) {
            gameMode = dynamic_cast<AGameModeBase*>(
                UClassRegistry::Get().CreateActorOfClass(GameModeConfig.GameModeClass, ActiveWorld.get(), "GameMode"));
        }
        if (!gameMode) {
            gameMode = ActiveWorld->SpawnActor<AGameModeBase>("GameMode");
        }
        ApplyGameModeConfig(gameMode);
        ActiveWorld->SetGameMode(gameMode);

        // 7. Initialize UI renderer before BeginPlay so widget MeasureString works
        FUIRenderer::Init();

        ActiveWorld->InitWorld();
        ActiveWorld->BeginPlay();

        // 8. Push Viewport Layer and Execute Engine Loop
        auto* viewportLayer = new FGameViewportLayer(ActiveWorld);
        ViewportLayer = viewportLayer;
        app->PushLayer(viewportLayer);
        app->Run();

        // 9. Shutdown & Cleanup
        ViewportLayer = nullptr;
        if (ActiveWorld) {
            ActiveWorld->EndPlay();
        }
        FUIRenderer::Shutdown();
        if (GameInstance) {
            GameInstance->Shutdown();
        }

        return 0;
    }

} // namespace Leon
