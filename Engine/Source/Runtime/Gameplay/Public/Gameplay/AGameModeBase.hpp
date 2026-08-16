#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/APlayerState.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Unreal Engine aligned GameModeBase defining match rules and player login flow.
     */
    class AGameModeBase : public AActor {
    public:
        AGameModeBase() = default;
        AGameModeBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "GameModeBase");
        ~AGameModeBase() override = default;

        virtual void InitGame();
        virtual void StartPlay();

        virtual APlayerController* Login(const std::string& InPlayerName = "Player_0");
        virtual APawn* SpawnDefaultPawnAtTransform(const glm::vec3& InLocation, const glm::vec3& InRotation);

        virtual AActor* FindPlayerStart(const std::string& InIncomingName = "") const;
        virtual APlayerStart* ChoosePlayerStart() const;

        std::string DefaultPawnClass = "ADefaultPawn";
        std::string PlayerControllerClass = "APlayerController";
        std::string HUDClass = "AHUD";
        std::string GameStateClass = "AGameStateBase";
        std::string PlayerStateClass = "APlayerState";

        glm::vec3 DefaultSpawnLocation{0.0f, 3.5f, 10.5f};
        glm::vec3 DefaultSpawnRotation{0.0f, -90.0f, 0.0f};

        AGameStateBase* GetGameState() const { return GameState; }

    protected:
        AGameStateBase* GameState = nullptr;
        int32_t NextPlayerId = 0;
    };

} // namespace Leon
