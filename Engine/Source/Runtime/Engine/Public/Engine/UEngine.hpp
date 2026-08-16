#pragma once

#include "Core/FApplication.hpp"
#include "Core/FConfigFile.hpp"
#include "Core/FInputSettings.hpp"
#include "Gameplay/UObject.hpp"
#include "Engine/UGameInstance.hpp"
#include "Engine/UWorld.hpp"

#include <string>

namespace Leon {

    class AGameModeBase;
    class FGameViewportLayer;

    /**
     * @brief Startup / travel configuration applied when spawning AGameModeBase.
     */
    struct FGameModeConfig {
        std::string GameModeClass = "AGameModeBase";
        std::string DefaultPawnClass = "ADefaultPawn";
        std::string PlayerControllerClass = "APlayerController";
        std::string HUDClass = "AHUD";
        std::string GameStateClass = "AGameStateBase";
        std::string PlayerStateClass = "APlayerState";
    };

    /**
     * @brief Unreal Engine aligned UEngine runtime singleton.
     *
     * Drives initialization, config loading, GameInstance, World loading, map travel, and the engine loop.
     */
    class UEngine : public UObject {
    public:
        UEngine();
        ~UEngine() override;

        static UEngine& Get();

        static int Run(FApplicationCommandLineArgs InArgs,
                       const std::string& InProjectOrConfigPath = "Projects/Sandbox/Sandbox.lproject");

        TRef<UWorld> GetWorld() const { return ActiveWorld; }
        TRef<UGameInstance> GetGameInstance() const { return GameInstance; }

        const std::string& GetCurrentMapName() const { return CurrentMapName; }
        const FGameModeConfig& GetGameModeConfig() const { return GameModeConfig; }
        const FInputSettings& GetInputSettings() const { return InputSettings; }

        /**
         * @brief Queues a map travel for the next safe frame (UGameplayStatics::OpenLevel).
         * @param InLevelName Virtual path such as "/Game/Maps/NightScene".
         */
        void RequestTravel(const std::string& InLevelName);

        /**
         * @brief Executes a pending travel if one was requested. Safe to call each frame.
         */
        void ProcessPendingTravel();

        bool HasPendingTravel() const { return bPendingTravel; }

    private:
        int InternalRun(FApplicationCommandLineArgs InArgs, const std::string& InConfigPath);

        void ApplyGameModeConfig(AGameModeBase* InGameMode) const;
        bool LoadMapIntoActiveWorld(const std::string& InVirtualMapPath);
        void TravelToMap(const std::string& InVirtualMapPath);

        TRef<UGameInstance> GameInstance;
        TRef<UWorld> ActiveWorld;

        FGameModeConfig GameModeConfig;
        FInputSettings InputSettings;
        std::string CurrentMapName;
        std::string PendingTravelMap;
        bool bPendingTravel = false;

        // Non-owning pointer to the active viewport layer so travel can rebind the world.
        FGameViewportLayer* ViewportLayer = nullptr;
    };

} // namespace Leon
