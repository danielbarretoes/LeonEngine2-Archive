#include "Gameplay/AGameModeBase.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    AGameModeBase::AGameModeBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AGameModeBase");
    }

    void AGameModeBase::InitGame() {
        LE_CORE_ASSERT(World != nullptr, "AGameModeBase requires valid UWorld!");

        if (World->GetGameState()) {
            GameState = World->GetGameState();
            return;
        }
        if (GameState)
            return;

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
        Login("Player_0");
    }

    APlayerStart* AGameModeBase::ChoosePlayerStart() const {
        if (!World)
            return nullptr;
        for (const auto& actorRef : World->GetAllActors()) {
            if (!actorRef || actorRef->IsPendingKill())
                continue;
            if (auto* start = dynamic_cast<APlayerStart*>(actorRef.get()))
                return start;
        }
        return nullptr;
    }

    AActor* AGameModeBase::FindPlayerStart(const std::string& InIncomingName) const {
        if (!World)
            return nullptr;
        if (!InIncomingName.empty()) {
            for (const auto& actorRef : World->GetAllActors()) {
                if (!actorRef || actorRef->IsPendingKill())
                    continue;
                auto* start = dynamic_cast<APlayerStart*>(actorRef.get());
                if (start && (start->GetPlayerStartTag() == InIncomingName || start->GetName() == InIncomingName))
                    return start;
            }
        }
        if (APlayerStart* chosen = ChoosePlayerStart())
            return chosen;
        return nullptr;
    }

    APlayerController* AGameModeBase::Login(const std::string& InPlayerName) {
        if (!World)
            return nullptr;

        APlayerController* pc = nullptr;
        if (!PlayerControllerClass.empty()) {
            pc = dynamic_cast<APlayerController*>(
                UClassRegistry::Get().CreateActorOfClass(PlayerControllerClass, World, "PlayerController"));
        }
        if (!pc) {
            pc = World->SpawnActor<APlayerController>("PlayerController");
        }

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
            ps->SetPlayerId(NextPlayerId++);
            if (pc) {
                pc->SetPlayerState(ps);
            }
            if (GameState) {
                GameState->AddPlayerState(ps);
            }
        }

        if (pc) {
            World->AddPlayerController(pc);

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

            if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
                glm::vec3 spawnLoc = DefaultSpawnLocation;
                glm::vec3 spawnRot = DefaultSpawnRotation;
                if (AActor* start = FindPlayerStart()) {
                    spawnLoc = start->GetActorLocation();
                    spawnRot = start->GetActorRotation();
                }
                APawn* pawn = SpawnDefaultPawnAtTransform(spawnLoc, spawnRot);
                if (pawn) {
                    pc->Possess(pawn);
                    LE_CORE_INFO("AGameModeBase: PlayerController possessed '{0}' at ({1}, {2}, {3})", pawn->GetName(),
                                 spawnLoc.x, spawnLoc.y, spawnLoc.z);
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
