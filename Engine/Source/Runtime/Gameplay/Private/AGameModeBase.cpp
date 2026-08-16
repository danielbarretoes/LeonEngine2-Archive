#include "Gameplay/AGameModeBase.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    AGameModeBase::AGameModeBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void AGameModeBase::InitGame() {
        LE_CORE_ASSERT(World != nullptr, "AGameModeBase requires valid UWorld!");

        // 1. Instantiate and Initialize GameState
        if (!GameStateClass.empty()) {
            GameState = dynamic_cast<AGameStateBase*>(
                UClassRegistry::Get().CreateActorOfClass(GameStateClass, World, "GameState"));
            if (!GameState) {
                GameState = World->SpawnActor<AGameStateBase>("GameState");
            }
            if (GameState) {
                World->SetGameState(GameState);
                LE_CORE_INFO("AGameModeBase: Initialized GameState '{0}'", GameState->GetName());
            }
        }
    }

    void AGameModeBase::StartPlay() {
        // FLog in default player
        Login("Player_0");
    }

    APlayerController* AGameModeBase::Login(const std::string& InPlayerName) {
        if (!World)
            return nullptr;

        // 1. Spawn PlayerController
        APlayerController* pc = nullptr;
        if (!PlayerControllerClass.empty()) {
            pc = dynamic_cast<APlayerController*>(
                UClassRegistry::Get().CreateActorOfClass(PlayerControllerClass, World, "PlayerController"));
        }
        if (!pc) {
            pc = World->SpawnActor<APlayerController>("PlayerController");
        }

        // 2. Spawn PlayerState
        APlayerState* ps = nullptr;
        if (!PlayerStateClass.empty()) {
            ps = dynamic_cast<APlayerState*>(
                UClassRegistry::Get().CreateActorOfClass(PlayerStateClass, World, "PlayerState"));
        }
        if (!ps) {
            ps = World->SpawnActor<APlayerState>("PlayerState");
        }

        if (ps) {
            ps->SetPlayerName(InPlayerName);
            if (pc) {
                pc->SetPlayerState(ps);
            }
            if (GameState) {
                GameState->AddPlayerState(ps);
            }
        }

        if (pc) {
            World->AddPlayerController(pc);

            // 3. Spawn AHUD
            if (!HUDClass.empty() && HUDClass != "None") {
                AHUD* hud = dynamic_cast<AHUD*>(UClassRegistry::Get().CreateActorOfClass(HUDClass, World, "HUD"));
                if (!hud) {
                    hud = World->SpawnActor<AHUD>("HUD");
                }
                if (hud) {
                    hud->SetPlayerController(pc);
                    pc->SetHUD(hud);
                    LE_CORE_INFO("AGameModeBase: Spawned HUD '{0}' (Class: {1}) for PlayerController", hud->GetName(),
                                 HUDClass);
                }
            }

            // 4. Spawn Default Pawn if class is configured
            if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
                APawn* pawn = SpawnDefaultPawnAtTransform(DefaultSpawnLocation, DefaultSpawnRotation);
                if (pawn) {
                    pc->Possess(pawn);
                    LE_CORE_INFO("AGameModeBase: PlayerController possessed '{0}' at ({1}, {2}, {3})", pawn->GetName(),
                                 DefaultSpawnLocation.x, DefaultSpawnLocation.y, DefaultSpawnLocation.z);
                }
            } else {
                LE_CORE_INFO(
                    "AGameModeBase: No DefaultPawnClass configured. PlayerController will rely on level CameraActor.");
            }
        }

        return pc;
    }

    APawn* AGameModeBase::SpawnDefaultPawnAtTransform(const glm::vec3& InLocation, const glm::vec3& InRotation) {
        if (!World)
            return nullptr;

        APawn* pawn = nullptr;
        if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
            pawn = dynamic_cast<APawn*>(
                UClassRegistry::Get().CreateActorOfClass(DefaultPawnClass, World, "DefaultPawn"));
        }
        if (!pawn && DefaultPawnClass != "None") {
            pawn = World->SpawnActor<ADefaultPawn>("DefaultPawn");
        }

        if (pawn) {
            pawn->SetActorLocation(InLocation);
            pawn->SetActorRotation(InRotation);
        }

        return pawn;
    }

} // namespace Leon
