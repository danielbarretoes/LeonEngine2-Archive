#include "gameplay/AGameModeBase.hpp"
#include "core/Log.hpp"
#include "gameplay/ADefaultPawn.hpp"
#include "gameplay/UClassRegistry.hpp"
#include "world/UWorld.hpp"

namespace Leon {

    AGameModeBase::AGameModeBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void AGameModeBase::InitGame() {
        LE_CORE_ASSERT(m_World != nullptr, "AGameModeBase requires valid UWorld!");

        // 1. Instantiate and Initialize GameState
        if (!GameStateClass.empty()) {
            m_GameState = dynamic_cast<AGameStateBase*>(
                UClassRegistry::Get().CreateActorOfClass(GameStateClass, m_World, "GameState"));
            if (!m_GameState) {
                m_GameState = m_World->SpawnActor<AGameStateBase>("GameState");
            }
            if (m_GameState) {
                m_World->SetGameState(m_GameState);
                LE_CORE_INFO("AGameModeBase: Initialized GameState '{0}'", m_GameState->GetName());
            }
        }
    }

    void AGameModeBase::StartPlay() {
        // Log in default player
        Login("Player_0");
    }

    APlayerController* AGameModeBase::Login(const std::string& InPlayerName) {
        if (!m_World) return nullptr;

        // 1. Spawn PlayerController
        APlayerController* pc = nullptr;
        if (!PlayerControllerClass.empty()) {
            pc = dynamic_cast<APlayerController*>(
                UClassRegistry::Get().CreateActorOfClass(PlayerControllerClass, m_World, "PlayerController"));
        }
        if (!pc) {
            pc = m_World->SpawnActor<APlayerController>("PlayerController");
        }

        // 2. Spawn PlayerState
        APlayerState* ps = nullptr;
        if (!PlayerStateClass.empty()) {
            ps = dynamic_cast<APlayerState*>(
                UClassRegistry::Get().CreateActorOfClass(PlayerStateClass, m_World, "PlayerState"));
        }
        if (!ps) {
            ps = m_World->SpawnActor<APlayerState>("PlayerState");
        }

        if (ps) {
            ps->SetPlayerName(InPlayerName);
            if (pc) {
                pc->SetPlayerState(ps);
            }
            if (m_GameState) {
                m_GameState->AddPlayerState(ps);
            }
        }

        if (pc) {
            m_World->AddPlayerController(pc);

            // 3. Spawn Default Pawn if class is configured
            if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
                APawn* pawn = SpawnDefaultPawnAtTransform(DefaultSpawnLocation, DefaultSpawnRotation);
                if (pawn) {
                    pc->Possess(pawn);
                    LE_CORE_INFO("AGameModeBase: PlayerController possessed '{0}' at ({1}, {2}, {3})",
                                 pawn->GetName(), DefaultSpawnLocation.x, DefaultSpawnLocation.y, DefaultSpawnLocation.z);
                }
            } else {
                LE_CORE_INFO("AGameModeBase: No DefaultPawnClass configured. PlayerController will rely on level CameraActor.");
            }
        }

        return pc;
    }

    APawn* AGameModeBase::SpawnDefaultPawnAtTransform(const glm::vec3& InLocation, const glm::vec3& InRotation) {
        if (!m_World) return nullptr;

        APawn* pawn = nullptr;
        if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
            pawn = dynamic_cast<APawn*>(
                UClassRegistry::Get().CreateActorOfClass(DefaultPawnClass, m_World, "DefaultPawn"));
        }
        if (!pawn && DefaultPawnClass != "None") {
            pawn = m_World->SpawnActor<ADefaultPawn>("DefaultPawn");
        }

        if (pawn) {
            pawn->SetActorLocation(InLocation);
            pawn->SetActorRotation(InRotation);
        }

        return pawn;
    }

} // namespace Leon
