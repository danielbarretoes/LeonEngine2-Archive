#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Core/FLog.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    void UGameplayStatics::OpenLevel(UWorld* InWorldContext, const std::string& InLevelName) {
        (void)InWorldContext;
        if (InLevelName.empty()) {
            LE_CORE_ERROR("UGameplayStatics::OpenLevel: empty level name");
            return;
        }

        // Flow: UI / gameplay → UEngine travel request → safe-frame reload
        UEngine::Get().RequestTravel(InLevelName);
    }

    void UGameplayStatics::PrintString(const std::string& InMessage, float InDuration, const glm::vec4& InColor,
                                       int64_t InKey, const glm::vec2& InPosition) {
        FOnScreenDebugMessageManager::Get().AddMessage(InMessage, InDuration, InColor, InKey, InPosition);
    }

    APlayerController* UGameplayStatics::GetPlayerController(UWorld* InWorldContext, int32_t InPlayerIndex) {
        if (!InWorldContext)
            return nullptr;
        const auto& controllers = InWorldContext->GetPlayerControllers();
        if (InPlayerIndex < 0 || static_cast<size_t>(InPlayerIndex) >= controllers.size()) {
            return nullptr;
        }
        return controllers[static_cast<size_t>(InPlayerIndex)];
    }

    AGameModeBase* UGameplayStatics::GetGameMode(UWorld* InWorldContext) {
        return InWorldContext ? InWorldContext->GetGameMode() : nullptr;
    }

    AGameStateBase* UGameplayStatics::GetGameState(UWorld* InWorldContext) {
        return InWorldContext ? InWorldContext->GetGameState() : nullptr;
    }

    APawn* UGameplayStatics::GetPlayerPawn(UWorld* InWorldContext, int32_t InPlayerIndex) {
        APlayerController* pc = GetPlayerController(InWorldContext, InPlayerIndex);
        return pc ? pc->GetPawn() : nullptr;
    }

    UParticleComponent* UGameplayStatics::SpawnEmitterAtLocation(UWorld* InWorld,
                                                                 const FParticleEmitterSettings& InSettings,
                                                                 const glm::vec3& InLocation) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>("ParticleEmitter");
        if (!actor)
            return nullptr;
        actor->SetActorLocation(InLocation);
        auto emitter = actor->AddActorComponent<UParticleComponent>("Particles");
        emitter->SetEmitterSettings(InSettings);
        emitter->SetDestroyOwnerWhenDone(true);
        emitter->Activate(true);
        return emitter.get();
    }

} // namespace Leon
