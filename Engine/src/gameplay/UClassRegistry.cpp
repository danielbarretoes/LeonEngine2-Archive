#include "gameplay/UClassRegistry.hpp"
#include "gameplay/ACameraActor.hpp"
#include "gameplay/ADefaultPawn.hpp"
#include "gameplay/AGameModeBase.hpp"
#include "gameplay/AGameStateBase.hpp"
#include "gameplay/APawn.hpp"
#include "gameplay/APlayerCameraManager.hpp"
#include "gameplay/APlayerController.hpp"
#include "gameplay/APlayerState.hpp"

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
    }

    AActor* UClassRegistry::CreateActorOfClass(const std::string& InClassName, UWorld* InWorld, const std::string& InName) {
        auto it = m_Factories.find(InClassName);
        if (it != m_Factories.end()) {
            return it->second(InWorld, InName.empty() ? InClassName : InName);
        }
        return nullptr;
    }

    bool UClassRegistry::HasClass(const std::string& InClassName) const {
        return m_Factories.find(InClassName) != m_Factories.end();
    }

} // namespace Leon
