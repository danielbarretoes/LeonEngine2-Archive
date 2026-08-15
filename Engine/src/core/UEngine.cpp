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

#include "asset/AssetPath.hpp"
#include "core/ProjectDescriptor.hpp"
#include "core/ProjectPaths.hpp"

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

    int UEngine::Run(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        UEngine engine;
        return engine.InternalRun(InArgs, InProjectOrConfigPath);
    }

    int UEngine::InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        LE_CORE_INFO("==================================================");
        LE_CORE_INFO("       LeonEngine2 - Unreal Architecture          ");
        LE_CORE_INFO("==================================================");

        // 1. Resolve Project Descriptor (.lproject) & Project Paths
        std::string projectOrConfigPath = InProjectOrConfigPath;
        FProjectDescriptor projectDesc;

        // Auto-discover project file if path is a directory or empty
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

        FAssetManager::SetContentRoot(FProjectPaths::ProjectContentDir());
        LE_CORE_INFO("UEngine: Project Root: '{0}', Content Root: '{1}'",
                     FProjectPaths::ProjectDir(), FProjectPaths::ProjectContentDir());

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
            LE_CORE_INFO("UEngine: Successfully parsed '{0}'", inputIniPath);
        }

        // Display & Window Settings (from DefaultEngine.ini)
        std::string windowTitle = engineConfig.GetString("/Script/Engine.DisplaySettings", "WindowTitle",
                                                         projectDesc.ProjectName.empty() ? "LeonEngine2" : projectDesc.ProjectName);
        uint32_t windowWidth = static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowWidth", 1280));
        uint32_t windowHeight = static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.DisplaySettings", "WindowHeight", 720));
        bool bVSync = engineConfig.GetBool("/Script/Engine.DisplaySettings", "VSync", true);

        // Map & GameMode Settings (Hierarchy: DefaultEngine.ini -> DefaultGame.ini -> Project Descriptor -> Fallback)
        std::string rawMapPath = engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GameDefaultMap",
                                                        projectDesc.DefaultMap.empty() ? "/Game/Maps/MainShowcase" : projectDesc.DefaultMap);
        std::string physicalMapPath = FProjectPaths::ResolveVirtualPath(rawMapPath);

        std::string gameModeClass = engineConfig.GetString("/Script/EngineSettings.GameMapsSettings", "GlobalDefaultGameMode",
                                                           projectDesc.DefaultGameMode.empty() ? "AGameModeBase" : projectDesc.DefaultGameMode);

        std::string defaultPawnClass = gameConfig.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass",
                                                            engineConfig.GetString("/Script/Engine.GameModeBase", "DefaultPawnClass", "ADefaultPawn"));
        std::string playerControllerClass = gameConfig.GetString("/Script/Engine.GameModeBase", "PlayerControllerClass",
                                                                 engineConfig.GetString("/Script/Engine.GameModeBase", "PlayerControllerClass", "APlayerController"));
        std::string gameStateClass = gameConfig.GetString("/Script/Engine.GameModeBase", "GameStateClass",
                                                          engineConfig.GetString("/Script/Engine.GameModeBase", "GameStateClass", "AGameStateBase"));
        std::string playerStateClass = gameConfig.GetString("/Script/Engine.GameModeBase", "PlayerStateClass",
                                                            engineConfig.GetString("/Script/Engine.GameModeBase", "PlayerStateClass", "APlayerState"));

        // 3. Initialize Application Specification
        FApplicationProps appProps;
        appProps.Name = windowTitle;
        appProps.CommandLineArgs = InArgs;
        appProps.WindowWidth = windowWidth;
        appProps.WindowHeight = windowHeight;

        auto app = CreateScope<FApplication>(appProps);
        app->GetWindow().SetVSync(bVSync);

        // 4. Create UGameInstance & UWorld
        m_GameInstance = CreateRef<UGameInstance>("GameInstance");
        m_ActiveWorld = UWorld::Create("MainWorld");
        m_GameInstance->SetWorld(m_ActiveWorld);
        m_GameInstance->Init();

        // 5. Load Map (.lmap)
        if (std::filesystem::exists(physicalMapPath)) {
            MapSerializer serializer(m_ActiveWorld);
            if (serializer.Deserialize(physicalMapPath)) {
                LE_CORE_INFO("UEngine: Loaded map '{0}' ({1}) with {2} actors",
                             rawMapPath, physicalMapPath, m_ActiveWorld->GetAllActors().size());
            } else {
                LE_CORE_ERROR("UEngine: Failed to parse map '{0}'", physicalMapPath);
            }
        } else {
            LE_CORE_WARN("UEngine: Map path '{0}' resolved to '{1}' not found on disk. Proceeding with blank world.",
                         rawMapPath, physicalMapPath);
        }

        // 6. Instantiate and Configure GameMode
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

        // 7. Initialize Gameplay Simulation
        m_ActiveWorld->InitWorld();
        m_ActiveWorld->BeginPlay();

        // 8. Push Viewport Layer and Execute Engine Loop
        app->PushLayer(new FGameViewportLayer(m_ActiveWorld));
        app->Run();

        // 9. Shutdown & Cleanup
        m_ActiveWorld->EndPlay();
        m_GameInstance->Shutdown();

        return 0;
    }

} // namespace Leon
