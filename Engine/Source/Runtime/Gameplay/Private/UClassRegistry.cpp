#include "Gameplay/UClassRegistry.hpp"
#include "Gameplay/ACameraActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Gameplay/ADefaultPawn.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AHUD.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/AWorldSettings.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"

namespace Leon {

    UClassRegistry& UClassRegistry::Get() {
        static UClassRegistry instance;
        return instance;
    }

    UClassRegistry::UClassRegistry() {
        RegisterBuiltins();
    }

    void UClassRegistry::RegisterBuiltins() {
        RegisterClass<AActor>("AActor");
        RegisterClass<AActor>("Actor");

        RegisterClass<APawn>("APawn");
        RegisterClass<APawn>("Pawn");

        RegisterClass<ADefaultPawn>("ADefaultPawn");
        RegisterClass<ADefaultPawn>("DefaultPawn");

        RegisterClass<ACharacter>("ACharacter");
        RegisterClass<ACharacter>("Character");

        RegisterClass<APlayerController>("APlayerController");
        RegisterClass<APlayerController>("PlayerController");
        RegisterClass<APlayerController>("DefaultPlayerController");

        RegisterClass<APlayerState>("APlayerState");
        RegisterClass<APlayerState>("PlayerState");
        RegisterClass<APlayerState>("DefaultPlayerState");

        RegisterClass<AGameStateBase>("AGameStateBase");
        RegisterClass<AGameStateBase>("GameStateBase");
        RegisterClass<AGameStateBase>("DefaultGameState");

        RegisterClass<AGameModeBase>("AGameModeBase");
        RegisterClass<AGameModeBase>("GameModeBase");
        RegisterClass<AGameModeBase>("DefaultGameMode");

        RegisterClass<ACameraActor>("ACameraActor");
        RegisterClass<ACameraActor>("CameraActor");

        RegisterClass<APlayerCameraManager>("APlayerCameraManager");
        RegisterClass<APlayerCameraManager>("PlayerCameraManager");

        RegisterClass<AHUD>("AHUD");
        RegisterClass<AHUD>("HUD");

        RegisterClass<APlayerStart>("APlayerStart");
        RegisterClass<APlayerStart>("PlayerStart");

        RegisterClass<AWorldSettings>("AWorldSettings");
        RegisterClass<AWorldSettings>("WorldSettings");

        RegisterClass<ANavMeshBoundsVolume>("ANavMeshBoundsVolume");
        RegisterClass<ANavMeshBoundsVolume>("NavMeshBoundsVolume");
    }

    AActor* UClassRegistry::CreateActorOfClass(const std::string& InClassName, UWorld* InWorld,
                                               const std::string& InName) {
        auto it = Factories.find(InClassName);
        if (it != Factories.end()) {
            return it->second(InWorld, InName.empty() ? InClassName : InName);
        }
        return nullptr;
    }

    bool UClassRegistry::HasClass(const std::string& InClassName) const {
        return Factories.find(InClassName) != Factories.end();
    }

} // namespace Leon
