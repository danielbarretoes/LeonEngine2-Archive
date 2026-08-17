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
#include "Gameplay/ABlockingVolume.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"
#include "Gameplay/APhysicsVolume.hpp"
#include "Gameplay/AProjectile.hpp"

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
        RegisterClass<APawn>("APawn");
        RegisterClass<ADefaultPawn>("ADefaultPawn");
        RegisterClass<ACharacter>("ACharacter");
        RegisterClass<APlayerController>("APlayerController");
        RegisterClass<APlayerState>("APlayerState");
        RegisterClass<AGameStateBase>("AGameStateBase");
        RegisterClass<AGameModeBase>("AGameModeBase");
        RegisterClass<ACameraActor>("ACameraActor");
        RegisterClass<APlayerCameraManager>("APlayerCameraManager");
        RegisterClass<AHUD>("AHUD");
        RegisterClass<APlayerStart>("APlayerStart");
        RegisterClass<AWorldSettings>("AWorldSettings");
        RegisterClass<ANavMeshBoundsVolume>("ANavMeshBoundsVolume");
        RegisterClass<ABlockingVolume>("ABlockingVolume");
        RegisterClass<APhysicsVolume>("APhysicsVolume");
        RegisterClass<AProjectile>("AProjectile");
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
