#include "core/UEngine.hpp"
#include "core/Log.hpp"
#include "gameplay/ADefaultPawn.hpp"
#include "gameplay/AGameModeBase.hpp"
#include "gameplay/APlayerCameraManager.hpp"
#include "gameplay/APlayerController.hpp"
#include "gameplay/UClassRegistry.hpp"
#include "renderer/AssetManager.hpp"
#include "renderer/PerspectiveCamera.hpp"
#include "renderer/RenderCommand.hpp"
#include "renderer/SceneRenderer.hpp"
#include "world/MapSerializer.hpp"

#include <filesystem>
#include <fstream>

namespace Leon {

    class FGameViewportLayer : public FLayer {
    public:
        explicit FGameViewportLayer(const TRef<UWorld>& InWorld)
            : FLayer("GameViewportLayer"), m_World(InWorld) {}

        void OnAttach() override {
            LE_CORE_INFO("FGameViewportLayer: Attached to active World '{0}'", m_World ? m_World->GetName() : "None");
            auto& window = FApplication::Get().GetWindow();
            if (window.GetWidth() > 0 && window.GetHeight() > 0) {
                float aspect = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
                APlayerController* pc = m_World->GetFirstPlayerController();
                if (pc && pc->GetPlayerCameraManager()) {
                    pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                }
            }
        }

        void OnDetach() override {
            LE_CORE_INFO("FGameViewportLayer: Detached from World");
        }

        void OnUpdate(FTimestep InTs) override {
            if (!m_World) return;

            // 1. Tick entire world simulation and gameplay framework
            m_World->Tick(InTs);

            // 2. Resolve Player Camera via APlayerCameraManager / APlayerController
            FPerspectiveCamera activeCamera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            APlayerController* pc = m_World->GetFirstPlayerController();
            if (pc) {
                pc->GetPlayerViewPoint(activeCamera);
            }

            // 3. Render scene from the resolved viewpoint
            m_World->OnRender(activeCamera);
        }

        void OnEvent(FEvent& InEvent) override {
            FEventDispatcher dispatcher(InEvent);
            dispatcher.Dispatch<FWindowResizeEvent>([this](FWindowResizeEvent& e) {
                if (e.GetWidth() > 0 && e.GetHeight() > 0) {
                    float aspect = static_cast<float>(e.GetWidth()) / static_cast<float>(e.GetHeight());
                    APlayerController* pc = m_World->GetFirstPlayerController();
                    if (pc && pc->GetPlayerCameraManager()) {
                        pc->GetPlayerCameraManager()->SetAspectRatio(aspect);
                    }
                }
                return false;
            });
        }

    private:
        TRef<UWorld> m_World;
    };

    static UEngine* s_EngineInstance = nullptr;

    UEngine::UEngine() : UObject("Engine") {
        s_EngineInstance = this;
    }

    UEngine::~UEngine() {
        s_EngineInstance = nullptr;
    }

    UEngine& UEngine::Get() {
        LE_CORE_ASSERT(s_EngineInstance != nullptr, "UEngine not instantiated!");
        return *s_EngineInstance;
    }

    int UEngine::Run(FApplicationCommandLineArgs InArgs, const std::string& InConfigPath) {
        UEngine engine;
        return engine.InternalRun(InArgs, InConfigPath);
    }

    int UEngine::InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InConfigPath) {
        LE_CORE_INFO("==================================================");
        LE_CORE_INFO("       LeonEngine2 - Unreal Architecture          ");
        LE_CORE_INFO("==================================================");

        // 1. Load DefaultEngine.ini
        FConfigFile config;
        std::string configPath = InConfigPath;
        if (!std::filesystem::exists(configPath)) {
            LE_CORE_WARN("UEngine: Config '{0}' not found, checking local directory...", configPath);
            if (std::filesystem::exists("Config/DefaultEngine.ini")) {
                configPath = "Config/DefaultEngine.ini";
            }
        }

        if (std::filesystem::exists(configPath)) {
            config.Load(configPath);
            LE_CORE_INFO("UEngine: Successfully parsed config from '{0}'", configPath);
        } else {
            LE_CORE_WARN("UEngine: DefaultEngine.ini not found. Using built-in defaults.");
        }

        // Display & Window Settings
        std::string windowTitle = config.GetString("/Script/Engine.DisplaySettings", "WindowTitle",
                                                   config.GetString("Display", "WindowTitle", "LeonEngine2"));
        uint32_t windowWidth = static_cast<uint32_t>(config.GetInt("/Script/Engine.DisplaySettings", "WindowWidth",
                                                                    config.GetInt("Display", "WindowWidth", 1280)));
        uint32_t windowHeight = static_cast<uint32_t>(config.GetInt("/Script/Engine.DisplaySettings", "WindowHeight",
                                                                     config.GetInt("Display", "WindowHeight", 720)));
        bool bVSync = config.GetBool("/Script/Engine.DisplaySettings", "VSync",
                                     config.GetBool("Display", "VSync", true));
        bool bFullscreen = config.GetBool("/Script/Engine.DisplaySettings", "Fullscreen",
                                          config.GetBool("Display", "Fullscreen", false));

        // Maps & Game Mode Settings
        std::string mapPath = config.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap",
                                               config.GetString("Game", "StartupMap", "Projects/Sandbox/Content/Maps/NightScene.lmap"));
        std::string gameModeClass = config.GetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode",
                                                     config.GetString("Game", "DefaultGameMode", "AGameModeBase"));

        std::string defaultPawnClass = config.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass",
                                                        config.GetString("Game", "DefaultPawnClass", "ADefaultPawn"));
        std::string playerControllerClass = config.GetString("/Script/Engine.GameModeBase", "PlayerControllerClass",
                                                             config.GetString("Game", "PlayerControllerClass", "APlayerController"));
        std::string gameStateClass = config.GetString("/Script/Engine.GameModeBase", "GameStateClass",
                                                      config.GetString("Game", "GameStateClass", "AGameStateBase"));
        std::string playerStateClass = config.GetString("/Script/Engine.GameModeBase", "PlayerStateClass",
                                                        config.GetString("Game", "PlayerStateClass", "APlayerState"));

        // 2. Initialize Application Specification
        FApplicationProps appProps;
        appProps.Name = windowTitle;
        appProps.CommandLineArgs = InArgs;
        appProps.WindowWidth = windowWidth;
        appProps.WindowHeight = windowHeight;

        auto app = CreateScope<FApplication>(appProps);
        app->GetWindow().SetVSync(bVSync);

        // 3. Create UGameInstance & UWorld
        m_GameInstance = CreateRef<UGameInstance>("GameInstance");
        m_ActiveWorld = UWorld::Create("MainWorld");
        m_GameInstance->SetWorld(m_ActiveWorld);
        m_GameInstance->Init();

        // 4. Load Map (.lmap)
        if (std::filesystem::exists(mapPath)) {
            MapSerializer serializer(m_ActiveWorld);
            if (serializer.Deserialize(mapPath)) {
                LE_CORE_INFO("UEngine: Loaded map '{0}' with {1} actors", mapPath, m_ActiveWorld->GetAllActors().size());
            } else {
                LE_CORE_ERROR("UEngine: Failed to parse map '{0}'", mapPath);
            }
        } else {
            LE_CORE_WARN("UEngine: Map path '{0}' does not exist on disk. Proceeding with blank world.", mapPath);
        }

        // 5. Instantiate and Configure GameMode
        AGameModeBase* gameMode = nullptr;
        if (!gameModeClass.empty() && UClassRegistry::Get().HasClass(gameModeClass)) {
            gameMode = dynamic_cast<AGameModeBase*>(
                UClassRegistry::Get().CreateActorOfClass(gameModeClass, m_ActiveWorld.get(), "GameMode"));
        }
        if (!gameMode) {
            gameMode = m_ActiveWorld->SpawnActor<AGameModeBase>("GameMode");
        }

        if (gameMode) {
            gameMode->DefaultPawnClass = defaultPawnClass;
            gameMode->PlayerControllerClass = playerControllerClass;
            gameMode->GameStateClass = gameStateClass;
            gameMode->PlayerStateClass = playerStateClass;
            m_ActiveWorld->SetGameMode(gameMode);
        }

        // 6. Initialize Gameplay Simulation
        m_ActiveWorld->InitWorld();
        m_ActiveWorld->BeginPlay();

        // 7. Push Viewport Layer and Execute Engine Loop
        app->PushLayer(new FGameViewportLayer(m_ActiveWorld));
        app->Run();

        // 8. Shutdown & Cleanup
        m_ActiveWorld->EndPlay();
        m_GameInstance->Shutdown();

        return 0;
    }

} // namespace Leon
