#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Audio/FAudioDevice.hpp"
#include "Audio/USoundWave.hpp"
#include "Core/FLog.hpp"
#include "Engine/UEngine.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/APawn.hpp"
#include "Gameplay/APlayerController.hpp"
#include "Gameplay/FOnScreenDebugMessage.hpp"
#include "Engine/UWorld.hpp"

#include <algorithm>
#include <cmath>

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

    bool UGameplayStatics::SpawnEmitterAtLocation(UWorld* InWorld, const FParticleEmitterSettings& InSettings,
                                                  const glm::vec3& InLocation) {
        if (!InWorld)
            return false;
        if (InSettings.bOneShot)
            return InWorld->SpawnTransientParticles(InSettings, InLocation);
        int32_t live = 0;
        for (auto entity : InWorld->GetRegistry().view<FParticleRenderComponent>()) {
            (void)entity;
            if (++live >= kMaxLiveParticleEmitters)
                return false;
        }
        AActor* actor = InWorld->SpawnActor<AActor>("ParticleEmitter");
        if (!actor)
            return false;
        actor->SetActorLocation(InLocation);
        auto emitter = actor->AddActorComponent<UParticleComponent>("Particles");
        emitter->SetEmitterSettings(InSettings);
        emitter->SetDestroyOwnerWhenDone(true);
        emitter->Activate(true);
        return true;
    }

    void UGameplayStatics::PlaySound2D(const std::string& InSoundPath, float InVolume) {
        if (InSoundPath.empty())
            return;
        auto wave = USoundWave::Load(InSoundPath);
        if (wave)
            FAudioDevice::Get().PlaySound2D(wave, InVolume);
    }

    void UGameplayStatics::PlayMusic2D(const std::string& InSoundPath, float InVolume) {
        if (InSoundPath.empty())
            return;
        auto wave = USoundWave::Load(InSoundPath);
        if (wave)
            FAudioDevice::Get().PlayMusic2D(wave, InVolume);
    }

    void UGameplayStatics::StopMusic() {
        FAudioDevice::Get().StopMusic();
    }

    void UGameplayStatics::PlaySoundAtLocation(const std::string& InSoundPath, const glm::vec3& InLocation,
                                               float InVolume, float InAttenuationRadius) {
        if (InSoundPath.empty())
            return;
        auto wave = USoundWave::Load(InSoundPath);
        if (wave)
            FAudioDevice::Get().PlaySoundAtLocation(wave, InLocation, InVolume, InAttenuationRadius);
    }

    bool UGameplayStatics::ApplyPointDamage(UWorld* InWorld, AActor* DamagedActor, float BaseDamage,
                                            const glm::vec3& HitFromDirection, const FHitResult& HitInfo,
                                            AActor* DamageInstigator, AActor* DamageCauser) {
        if (!InWorld || !DamagedActor || BaseDamage <= 0.0f)
            return false;
        if (InWorld->GetNetMode() == ENetMode::Client)
            return false;

        UHealthComponent* health = DamagedActor->FindComponentByClass<UHealthComponent>();
        if (!health || health->IsDead())
            return false;

        const float before = health->GetHealth();
        FDamageInfo info;
        info.DamageAmount = BaseDamage;
        info.DamageType = EDamageType::Point;
        info.Instigator = DamageInstigator;
        info.Causer = DamageCauser;
        info.HitActor = DamagedActor;
        info.HitComponent = static_cast<UActorComponent*>(HitInfo.Component);
        info.HitLocation = HitInfo.ImpactPoint;
        info.HitNormal = HitInfo.ImpactNormal;
        const float dirLen = glm::length(HitFromDirection);
        if (dirLen > 1e-5f)
            info.Impulse = (HitFromDirection / dirLen) * BaseDamage * 0.05f;

        health->ApplyDamage(info);
        if (health->GetHealth() >= before)
            return false;

        if (AGameModeBase* gm = GetGameMode(InWorld))
            gm->NotifyActorDamaged(DamagedActor, info);
        return true;
    }

    int32_t UGameplayStatics::ApplyRadialDamage(UWorld* InWorld, float BaseDamage, const glm::vec3& Origin,
                                                float DamageRadius, AActor* DamageInstigator, AActor* DamageCauser,
                                                float MinimumDamage, AActor* IgnoreActor) {
        if (!InWorld || BaseDamage <= 0.0f || DamageRadius <= 0.0f)
            return 0;
        if (InWorld->GetNetMode() == ENetMode::Client)
            return 0;

        int32_t hitCount = 0;
        const float radiusSq = DamageRadius * DamageRadius;
        AGameModeBase* gm = GetGameMode(InWorld);
        for (const auto& actorRef : InWorld->GetAllActors()) {
            AActor* actor = actorRef.get();
            if (!actor || actor == IgnoreActor || actor->IsPendingKill())
                continue;
            UHealthComponent* health = actor->FindComponentByClass<UHealthComponent>();
            if (!health || health->IsDead())
                continue;

            const glm::vec3 delta = actor->GetActorLocation() - Origin;
            const float distSq = glm::dot(delta, delta);
            if (distSq > radiusSq)
                continue;

            const float dist = std::sqrt(distSq);
            const float alpha = 1.0f - (dist / DamageRadius);
            const float amount = std::max(MinimumDamage, BaseDamage * alpha);
            if (amount <= 0.0f)
                continue;

            const float before = health->GetHealth();
            FDamageInfo info;
            info.DamageAmount = amount;
            info.DamageType = EDamageType::Radial;
            info.Instigator = DamageInstigator;
            info.Causer = DamageCauser;
            info.HitActor = actor;
            info.HitLocation = actor->GetActorLocation();
            info.HitNormal = dist > 1e-5f ? glm::normalize(delta) : glm::vec3(0.0f, 1.0f, 0.0f);
            if (dist > 1e-5f)
                info.Impulse = info.HitNormal * amount * 0.08f;

            health->ApplyDamage(info);
            if (health->GetHealth() >= before)
                continue;

            ++hitCount;
            if (gm)
                gm->NotifyActorDamaged(actor, info);
        }
        return hitCount;
    }

} // namespace Leon
