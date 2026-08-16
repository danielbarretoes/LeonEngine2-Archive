#include "Engine/UEngine.hpp"
#include "Core/FInput.hpp"
#include "Core/FLog.hpp"
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

            FOnScreenDebugMessageManager::Get().Tick(InTs.GetSeconds());

            World->Tick(InTs);

            ApplyCursorFromPlayerController();
            DispatchUIMouseMove();

            FPerspectiveCamera activeCamera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            APlayerController* pc = World->GetFirstPlayerController();
            if (pc) {
                pc->GetPlayerViewPoint(activeCamera);
            }

            World->OnRender(activeCamera);
            DrawLightGizmos(activeCamera);
            DrawHUDAndUI();
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
                    renderer->SetDebugMode(10);
                    LE_CORE_INFO("[RENDER DEBUG] Mode: Direct Lighting Only (Cook-Torrance Lo)");
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
            if (!FApplication::Get().IsLightGizmosEnabled())
                return;

            FDebugRenderer::BeginScene(InCamera);
            auto& reg = World->GetRegistry();

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

            FDebugRenderer::EndScene();
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

    int UEngine::Run(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        UEngine engine;
        return engine.InternalRun(InArgs, InProjectOrConfigPath);
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

    void UEngine::ApplyGameModeConfig(AGameModeBase* InGameMode) const {
        if (!InGameMode)
            return;
        InGameMode->DefaultPawnClass = GameModeConfig.DefaultPawnClass;
        InGameMode->PlayerControllerClass = GameModeConfig.PlayerControllerClass;
        InGameMode->HUDClass = GameModeConfig.HUDClass;
        InGameMode->GameStateClass = GameModeConfig.GameStateClass;
        InGameMode->PlayerStateClass = GameModeConfig.PlayerStateClass;
    }

    bool UEngine::LoadMapIntoActiveWorld(const std::string& InVirtualMapPath) {
        if (!ActiveWorld)
            return false;

        std::string physicalMapPath = FProjectPaths::ResolveVirtualPath(InVirtualMapPath);
        if (!std::filesystem::exists(physicalMapPath)) {
            LE_CORE_ERROR("UEngine: Map '{0}' resolved to '{1}' not found", InVirtualMapPath, physicalMapPath);
            return false;
        }

        FMapSerializer serializer(ActiveWorld);
        if (!serializer.Deserialize(physicalMapPath)) {
            LE_CORE_ERROR("UEngine: Failed to parse map '{0}'", physicalMapPath);
            return false;
        }

        CurrentMapName = InVirtualMapPath;
        LE_CORE_INFO("UEngine: Loaded map '{0}' ({1}) with {2} actors", InVirtualMapPath, physicalMapPath,
                     ActiveWorld->GetAllActors().size());
        return true;
    }

    void UEngine::TravelToMap(const std::string& InVirtualMapPath) {
        LE_CORE_INFO("UEngine: Traveling to '{0}'...", InVirtualMapPath);

        // Flow: destroy current world → new UWorld → load .lmap → GameMode → login → BeginPlay
        if (ActiveWorld) {
            ActiveWorld->EndPlay();
            ActiveWorld->Clear();
        }

        FOnScreenDebugMessageManager::Get().Clear();
        FUIRenderer::Shutdown();

        ActiveWorld = UWorld::Create("MainWorld");
        if (GameInstance) {
            GameInstance->SetWorld(ActiveWorld);
        }
        if (ViewportLayer) {
            ViewportLayer->SetWorld(ActiveWorld);
        }

        LoadMapIntoActiveWorld(InVirtualMapPath);

        AGameModeBase* gameMode = nullptr;
        if (!GameModeConfig.GameModeClass.empty() && UClassRegistry::Get().HasClass(GameModeConfig.GameModeClass)) {
            gameMode = dynamic_cast<AGameModeBase*>(UClassRegistry::Get().CreateActorOfClass(
                GameModeConfig.GameModeClass, ActiveWorld.get(), "GameMode"));
        }
        if (!gameMode) {
            gameMode = ActiveWorld->SpawnActor<AGameModeBase>("GameMode");
        }
        ApplyGameModeConfig(gameMode);
        ActiveWorld->SetGameMode(gameMode);

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
            }
        }

        LE_CORE_INFO("UEngine: Travel complete — map '{0}' is live", InVirtualMapPath);
    }

    int UEngine::InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        LE_CORE_INFO("==================================================");
        LE_CORE_INFO("       LeonEngine2 - Unreal Architecture          ");
        LE_CORE_INFO("==================================================");

        // 1. Resolve Project Descriptor (.lproject) & Project Paths
        std::string projectOrConfigPath = InProjectOrConfigPath;
        FProjectDescriptor projectDesc;

        if (projectOrConfigPath.empty() || std::filesystem::is_directory(projectOrConfigPath)) {
            std::string candidateDir = projectOrConfigPath.empty() ? "." : projectOrConfigPath;
            for (const auto& entry : std::filesystem::directory_iterator(candidateDir)) {
                if (entry.path().extension() == ".lproject") {
                    projectOrConfigPath = entry.path().string();
                    break;
                }
            }
        }

        if (std::filesystem::exists(projectOrConfigPath) &&
            projectOrConfigPath.rfind(".lproject") == projectOrConfigPath.length() - 9) {
            if (projectDesc.Load(projectOrConfigPath)) {
                LE_CORE_INFO("UEngine: Loaded project descriptor '{0}' (Project: {1}, EngineVersion: {2})",
                             projectOrConfigPath, projectDesc.ProjectName, projectDesc.EngineVersion);
            }
            FProjectPaths::SetProjectRoot(projectOrConfigPath);
        } else {
            FProjectPaths::SetProjectRoot(projectOrConfigPath);
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

        std::string rawMapPath =
            engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap",
                                   projectDesc.DefaultMap.empty() ? "/Game/Maps/MainShowcase" : projectDesc.DefaultMap);

        GameModeConfig.GameModeClass =
            engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode",
                                   projectDesc.DefaultGameMode.empty() ? "AGameModeBase" : projectDesc.DefaultGameMode);

        // Prefer Sandbox GameMode section, then Engine.GameModeBase
        auto readGameClass = [&](const char* key, const std::string& fallback) {
            std::string v = gameConfig.GetString("/Script/Sandbox.SandboxGameMode", key, "");
            if (v.empty()) {
                v = gameConfig.GetString("/Script/Engine.GameModeBase", key,
                                         engineConfig.GetString("/Script/Engine.GameModeBase", key, fallback));
            }
            return v;
        };

        GameModeConfig.DefaultPawnClass = readGameClass("DefaultPawnClass", "ADefaultPawn");
        GameModeConfig.PlayerControllerClass = readGameClass("PlayerControllerClass", "APlayerController");
        GameModeConfig.HUDClass = readGameClass("HUDClass", "AHUD");
        GameModeConfig.GameStateClass = readGameClass("GameStateClass", "AGameStateBase");
        GameModeConfig.PlayerStateClass = readGameClass("PlayerStateClass", "APlayerState");

        // Allow explicit Sandbox GameMode class override
        std::string sandboxGM = gameConfig.GetString("/Script/Sandbox.SandboxGameMode", "GameModeClass", "");
        if (!sandboxGM.empty()) {
            GameModeConfig.GameModeClass = sandboxGM;
        }

        // 3. Initialize FApplication
        FApplicationProps appProps;
        appProps.Name = windowTitle;
        appProps.CommandLineArgs = InArgs;
        appProps.WindowWidth = windowWidth;
        appProps.WindowHeight = windowHeight;

        auto app = CreateScope<FApplication>(appProps);
        app->GetWindow().SetVSync(bVSync);

        // 4. Create UGameInstance & UWorld
        GameInstance = CreateRef<UGameInstance>("GameInstance");
        ActiveWorld = UWorld::Create("MainWorld");
        GameInstance->SetWorld(ActiveWorld);
        GameInstance->Init();

        // 5. Load Map (.lmap)
        LoadMapIntoActiveWorld(rawMapPath);

        // 6. Instantiate and Configure GameMode
        AGameModeBase* gameMode = nullptr;
        if (!GameModeConfig.GameModeClass.empty() && UClassRegistry::Get().HasClass(GameModeConfig.GameModeClass)) {
            gameMode = dynamic_cast<AGameModeBase*>(UClassRegistry::Get().CreateActorOfClass(
                GameModeConfig.GameModeClass, ActiveWorld.get(), "GameMode"));
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
