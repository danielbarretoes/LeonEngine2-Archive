#pragma once

#include "gameplay/AActor.hpp"
#include "gameplay/AGameStateBase.hpp"
#include "gameplay/APawn.hpp"
#include "gameplay/APlayerController.hpp"
#include "gameplay/APlayerState.hpp"

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

        std::string DefaultPawnClass = "ADefaultPawn";
        std::string PlayerControllerClass = "APlayerController";
        std::string GameStateClass = "AGameStateBase";
        std::string PlayerStateClass = "APlayerState";

        glm::vec3 DefaultSpawnLocation{0.0f, 3.5f, 10.5f};
        glm::vec3 DefaultSpawnRotation{0.0f, -90.0f, 0.0f};

        AGameStateBase* GetGameState() const { return m_GameState; }

    protected:
        AGameStateBase* m_GameState = nullptr;
    };

} // namespace Leon
