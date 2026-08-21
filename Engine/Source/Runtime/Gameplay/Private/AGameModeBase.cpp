#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AController.hpp"
#include "Core/FLog.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
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
        if (!World)
            return;
        const ENetMode NetMode = World->GetNetMode();
        if (NetMode == ENetMode::Client || NetMode == ENetMode::DedicatedServer)
            return;
        Login("Player_0");
    }

    bool AGameModeBase::PlayerCanRestart(AController* InPlayer) const {
        (void)InPlayer;
        return true;
    }

    void AGameModeBase::RestartPlayer(AController* NewPlayer) {
        if (!NewPlayer || !World || World->GetNetMode() == ENetMode::Client)
            return;
        if (!PlayerCanRestart(NewPlayer))
            return;

        glm::vec3 spawnLoc = DefaultSpawnLocation;
        glm::vec3 spawnRot = DefaultSpawnRotation;
        if (AActor* start = FindPlayerStart(NewPlayer)) {
            spawnLoc = start->GetActorLocation();
            spawnRot = start->GetActorRotation();
        }
        RestartPlayerAtTransform(NewPlayer, spawnLoc, spawnRot);
    }

    void AGameModeBase::RestartPlayerAtTransform(AController* NewPlayer, const glm::vec3& InLocation,
                                                 const glm::vec3& InRotation) {
        if (!NewPlayer || !World || World->GetNetMode() == ENetMode::Client)
            return;

        // Flow: RestartPlayer
        // 1. UnPossess and destroy the current pawn (controller + PlayerState persist).
        // 2. If DefaultPawnClass is empty/"None", leave the controller without a pawn.
        // 3. Otherwise spawn a new default pawn and Possess.
        if (APawn* oldPawn = NewPlayer->GetPawn()) {
            NewPlayer->UnPossess();
            World->DestroyActor(oldPawn);
        }
        if (DefaultPawnClass.empty() || DefaultPawnClass == "None")
            return;

        APawn* pawn = SpawnDefaultPawnAtTransform(InLocation, InRotation);
        if (pawn)
            NewPlayer->Possess(pawn);
    }

    APlayerStart* AGameModeBase::ChoosePlayerStart(AController* InPlayer) const {
        (void)InPlayer;
        if (!World)
            return nullptr;
        for (const auto& actorRef : World->GetAllActors()) {
            if (!actorRef || actorRef->IsPendingKill())
                continue;
            auto* start = dynamic_cast<APlayerStart*>(actorRef.get());
            if (!start || !start->IsEnabled())
                continue;
            if (start->GetPlayerStartTag() == "Dummy")
                continue;
            return start;
        }
        return nullptr;
    }

    AActor* AGameModeBase::FindPlayerStart(AController* InPlayer, const std::string& InIncomingName) const {
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
        if (APlayerStart* chosen = ChoosePlayerStart(InPlayer))
            return chosen;
        return nullptr;
    }

    APlayerController* AGameModeBase::Login(const std::string& InPlayerName) {
        if (!World)
            return nullptr;

        if (MaxPlayers > 0) {
            const int32_t Current = static_cast<int32_t>(World->GetPlayerControllers().size());
            if (Current >= MaxPlayers) {
                LE_CORE_WARN("AGameModeBase: Login rejected for '{0}' (MaxPlayers={1})", InPlayerName, MaxPlayers);
                return nullptr;
            }
        }

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
                if (AActor* start = FindPlayerStart(pc)) {
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

    void AGameModeBase::Logout(AController* Exiting) {
        if (!Exiting || !World || World->GetNetMode() == ENetMode::Client)
            return;

        // Flow: Logout
        // 1. UnPossess so the pawn is released (controller + PlayerState are about to go).
        // 2. Unregister PlayerState from GameState.
        // 3. Destroy HUD / camera manager, then the controller and PlayerState.
        APlayerState* playerState = Exiting->GetPlayerState();
        Exiting->UnPossess();
        if (GameState && playerState)
            GameState->RemovePlayerState(playerState);

        if (auto* pc = dynamic_cast<APlayerController*>(Exiting)) {
            if (AHUD* hud = pc->GetHUD())
                World->DestroyActor(hud);
            if (APlayerCameraManager* pcm = pc->GetPlayerCameraManager())
                World->DestroyActor(pcm);
        }

        World->DestroyActor(Exiting);
        if (playerState && !playerState->IsPendingKill())
            World->DestroyActor(playerState);
    }

    APawn* AGameModeBase::SpawnDefaultPawnAtTransform(const glm::vec3& InLocation, const glm::vec3& InRotation) {
        if (!World)
            return nullptr;

        APawn* pawn = nullptr;
        if (!DefaultPawnClass.empty() && DefaultPawnClass != "None") {
            pawn =
                dynamic_cast<APawn*>(UClassRegistry::Get().CreateActorOfClass(DefaultPawnClass, World, "DefaultPawn"));
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

    void AGameModeBase::NotifyActorDamaged(AActor* DamagedActor, const FDamageInfo& InInfo) {
        (void)DamagedActor;
        (void)InInfo;
    }

    void AGameModeBase::NotifyActorKilled(AActor* Victim, const FDamageInfo& InInfo) {
        (void)Victim;
        (void)InInfo;
    }

} // namespace Leon
