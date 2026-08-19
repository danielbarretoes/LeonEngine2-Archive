#include "Engine/UEngine.hpp"
#include "FGameViewportLayer.hpp"
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
#include "Renderer/FIBLGenerator.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/FRenderCommand.hpp"
#include "Renderer/FWorldRenderer.hpp"
#include "UMG/FUIRenderer.hpp"
#include "Audio/FAudioDevice.hpp"
#include "Engine/Components.hpp"
#include "Engine/FMapSerializer.hpp"
#include "Lightmass/FLightmass.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <utility>

namespace Leon {

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

    void UEngine::BindSession(const TRef<UGameInstance>& InGI, const TRef<UWorld>& InWorld) {
        GameInstance = InGI;
        ActiveWorld = InWorld;
        if (GameInstance)
            GameInstance->SetWorld(ActiveWorld);
        if (ActiveWorld && GameInstance)
            ActiveWorld->SetNetMode(GameInstance->GetNetMode());
    }

    void UEngine::SetProjectRendererConfig(uint32_t InShadowMapResolution, bool bInEnablePlanarReflection,
                                           uint32_t InCascadeCount, float InShadowDistance,
                                           EPlanarReflectionQuality InPlanarQuality, bool bInSSAOEnabled,
                                           bool bInBloomEnabled, bool bInFXAAEnabled,
                                           EShadowFilterMode InShadowFilter) {
        ProjectShadowMapResolution = InShadowMapResolution > 0 ? InShadowMapResolution : 2048;
        bProjectEnablePlanarReflection = bInEnablePlanarReflection;
        ProjectCascadeCount = InCascadeCount > 0 ? std::min(InCascadeCount, 4u) : 4;
        ProjectShadowDistance = InShadowDistance > 0.0f ? InShadowDistance : 100.0f;
        ProjectPlanarReflectionQuality = InPlanarQuality;
        ProjectPlanarReflectionResolutionScale = PlanarReflectionScaleFor(InPlanarQuality);
        bProjectSSAOEnabled = bInSSAOEnabled;
        bProjectBloomEnabled = bInBloomEnabled;
        bProjectFXAAEnabled = bInFXAAEnabled;
        ProjectShadowFilter = InShadowFilter;
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
        FLightmass::RefreshRuntimeLightmapTrust(*InWorld);
        return true;
    }

    bool UEngine::LoadMapIntoActiveWorld(const std::string& InVirtualMapPath) {
        if (!LoadMapIntoWorld(ActiveWorld, InVirtualMapPath))
            return false;
        CurrentMapName = InVirtualMapPath;
        return true;
    }

    int UEngine::InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InProjectOrConfigPath) {
        // Shipping packages place Engine/Assets beside the exe; adopt that cwd so relative
        // engine asset paths resolve when launched from Explorer (cwd != package root).
        if (InArgs.Args && InArgs.Count > 0 && InArgs.Args[0]) {
            if (FProjectPaths::AdoptPackagedWorkingDirectory(InArgs.Args[0])) {
                LE_CORE_INFO("UEngine: Adopted packaged working directory (Engine/Assets beside exe)");
            }
        }

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
        uint32_t windowWidth = static_cast<uint32_t>(engineConfig.GetInt(
            "/Script/Engine.DisplaySettings", "WindowWidth", static_cast<int>(FWindowDisplayPolicy::DefaultWidth)));
        uint32_t windowHeight = static_cast<uint32_t>(engineConfig.GetInt(
            "/Script/Engine.DisplaySettings", "WindowHeight", static_cast<int>(FWindowDisplayPolicy::DefaultHeight)));
        bool bVSync = engineConfig.GetBool("/Script/Engine.DisplaySettings", "VSync", true);
        bool bFullscreen = engineConfig.GetBool("/Script/Engine.DisplaySettings", "Fullscreen", false);

        // Renderer project defaults (map skybox Exposure/SunIntensity are not overwritten)
        ProjectShadowMapResolution =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.RendererSettings", "ShadowMapResolution", 2048));
        bProjectEnablePlanarReflection =
            engineConfig.GetBool("/Script/Engine.RendererSettings", "EnablePlanarReflection", true);
        ProjectPlanarReflectionQuality = ParsePlanarReflectionQuality(
            engineConfig.GetString("/Script/Engine.RendererSettings", "PlanarReflectionQuality", "Epic"));
        if (engineConfig.HasKey("/Script/Engine.RendererSettings", "PlanarReflectionResolutionScale")) {
            ProjectPlanarReflectionResolutionScale = ClampPlanarReflectionResolutionScale(
                engineConfig.GetFloat("/Script/Engine.RendererSettings", "PlanarReflectionResolutionScale", 1.0f));
        } else {
            ProjectPlanarReflectionResolutionScale = PlanarReflectionScaleFor(ProjectPlanarReflectionQuality);
        }
        ProjectCascadeCount =
            static_cast<uint32_t>(engineConfig.GetInt("/Script/Engine.RendererSettings", "CascadeCount", 4));
        ProjectShadowDistance = engineConfig.GetFloat("/Script/Engine.RendererSettings", "ShadowDistance", 100.0f);
        bProjectSSAOEnabled = engineConfig.GetBool("/Script/Engine.RendererSettings", "EnableSSAO", true);
        ProjectSSAORadius = engineConfig.GetFloat("/Script/Engine.RendererSettings", "SSAORadius", 0.5f);
        ProjectSSAOIntensity = engineConfig.GetFloat("/Script/Engine.RendererSettings", "SSAOIntensity", 1.0f);
        ProjectSSAOBias = engineConfig.GetFloat("/Script/Engine.RendererSettings", "SSAOBias", 0.025f);
        bProjectBloomEnabled = engineConfig.GetBool("/Script/Engine.RendererSettings", "EnableBloom", true);
        bProjectFXAAEnabled = engineConfig.GetBool("/Script/Engine.RendererSettings", "EnableFXAA", true);
        ProjectShadowFilter =
            ParseShadowFilterMode(engineConfig.GetString("/Script/Engine.RendererSettings", "ShadowFilter", "PCF3x3"));
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
        if (const std::string widthArg = readArgValue("--width=", "--width"); !widthArg.empty())
            windowWidth = static_cast<uint32_t>(std::max(1, std::atoi(widthArg.c_str())));
        if (const std::string heightArg = readArgValue("--height=", "--height"); !heightArg.empty())
            windowHeight = static_cast<uint32_t>(std::max(1, std::atoi(heightArg.c_str())));
        {
            unsigned int constrainedW = windowWidth;
            unsigned int constrainedH = windowHeight;
            FWindowDisplayPolicy::ConstrainClientSize(constrainedW, constrainedH);
            windowWidth = constrainedW;
            windowHeight = constrainedH;
        }
        if (const std::string cascadeArg = readArgValue("--cascade-count=", "--cascade-count"); !cascadeArg.empty())
            ProjectCascadeCount = static_cast<uint32_t>(std::max(0, std::atoi(cascadeArg.c_str())));
        for (int i = 1; i < InArgs.Count; ++i) {
            const char* raw = InArgs.Args ? InArgs.Args[i] : nullptr;
            if (!raw)
                continue;
            std::string arg = raw;
            if (arg == "--no-vsync")
                bVSync = false;
        }

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
                                                ProjectCascadeCount, ProjectShadowDistance,
                                                ProjectPlanarReflectionQuality, ProjectPlanarReflectionResolutionScale);
        ActiveWorld->SetProjectSSAODefaults(bProjectSSAOEnabled, ProjectSSAORadius, ProjectSSAOIntensity,
                                            ProjectSSAOBias);
        ActiveWorld->SetProjectPostProcessToggles(bProjectBloomEnabled, bProjectFXAAEnabled);
        ActiveWorld->SetProjectShadowFilter(ProjectShadowFilter);
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

        // 7. Initialize UI + audio before BeginPlay
        FUIRenderer::Init();
        FAudioDevice::Get().Init();

        ActiveWorld->InitWorld();
        ActiveWorld->BeginPlay();

        // 8. Push Viewport Layer and Execute Engine Loop
        auto* viewportLayer = new FGameViewportLayer(ActiveWorld);
        ViewportLayer = viewportLayer;
        app->PushLayer(viewportLayer);
        app->Run();

        // 9. Shutdown & Cleanup — release world/GPU while the GL context is still alive
        if (ViewportLayer)
            ViewportLayer->SetWorld(nullptr);
        ViewportLayer = nullptr;
        if (ActiveWorld) {
            ActiveWorld->EndPlay();
            ActiveWorld->Clear();
        }
        if (GameInstance)
            GameInstance->SetWorld(nullptr);
        ActiveWorld = nullptr;

        FIBLGenerator::ReleaseStaticCaches();
        FMeshPrimitives::ReleaseStaticCaches();

        FAudioDevice::Get().Shutdown();
        FUIRenderer::Shutdown();
        if (GameInstance) {
            GameInstance->Shutdown();
        }

        return 0;
    }

} // namespace Leon
