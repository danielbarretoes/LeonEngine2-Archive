#include "ALeonTournamentProjectile.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentWeapon.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Renderer/FRenderingMath.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    ALeonTournamentProjectile::ALeonTournamentProjectile(entt::entity InHandle, UWorld* InWorld,
                                                         const std::string& InName)
        : AProjectile(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentProjectile");
        bDestroyOnHit = false;
    }

    void ALeonTournamentProjectile::BeginPlay() {
        AProjectile::BeginPlay();
        AttachVisual();
    }

    void ALeonTournamentProjectile::AttachVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        const float radius = std::max(0.08f, Config.ProjectileRadius > 0.0f ? Config.ProjectileRadius * 0.7f : 0.12f);
        auto va = FMeshPrimitives::CreateSphere(radius, 10, 10);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Sphere";
        mesh.MeshRadius = radius;
        mesh.Mobility = EComponentMobility::Movable;
        mesh.bCastShadows = false;
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("ProjectileMat");
            inst->SetAlbedoColor(VisualColor);
            AddComponent<FMaterialComponent>(inst);
        }
    }

    void ALeonTournamentProjectile::Launch(ALeonTournamentCharacter* InInstigator, ALeonTournamentWeapon* InCauser,
                                           const glm::vec3& InDir, const FLeonTournamentWeaponConfig& InConfig) {
        InstigatorCharacter = InInstigator;
        CauserWeapon = InCauser;
        Config = InConfig;
        VisualColor = InConfig.VisualColor;
        SetInstigatorPawn(InInstigator);
        SetInitialLifeSpan(4.0f);
        if (ProjectileMovement) {
            ProjectileMovement->InitialSpeed = InConfig.ProjectileSpeed;
            ProjectileMovement->ProjectileGravityScale = InConfig.ProjectileGravityScale;
            ProjectileMovement->ProjectileRadius = InConfig.ProjectileRadius;
            ProjectileMovement->bRotationFollowsVelocity = true;
            ProjectileMovement->IgnoreActor = InInstigator;
        }
        if (CollisionComponent)
            CollisionComponent->SetSphereRadius(InConfig.ProjectileRadius);
        InitVelocity(InDir);
        AttachVisual();
        if (HasComponent<FMaterialComponent>()) {
            if (auto mat = GetComponent<FMaterialComponent>().MaterialInstance)
                mat->SetAlbedoColor(VisualColor);
        }
    }

    void ALeonTournamentProjectile::SpawnTrail() {
        if (!World)
            return;
        FParticleEmitterSettings trail;
        trail.Kind = EParticleKind::SpriteBurst;
        trail.BurstCount = Config.ProjectileGravityScale > 0.1f ? 6 : 8;
        trail.Lifetime = 0.18f;
        trail.Size = 0.05f;
        trail.SizeEnd = 0.14f;
        trail.Color = {VisualColor.x, VisualColor.y, VisualColor.z, 0.9f};
        trail.ColorEnd = {VisualColor.x * 0.3f, VisualColor.y * 0.2f, VisualColor.z * 0.1f, 0.0f};
        trail.VelocityMin = {-0.4f, 0.0f, -0.4f};
        trail.VelocityMax = {0.4f, 0.6f, 0.4f};
        trail.Gravity = {0.0f, -2.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, trail, GetActorLocation());
    }

    void ALeonTournamentProjectile::Explode(const glm::vec3& InLocation, const glm::vec3& InNormal,
                                            ALeonTournamentCharacter* InDirectHit) {
        if (HasExploded() || !World)
            return;
        MarkExploded();

        const bool bGrenade = Config.ProjectileGravityScale > 0.1f;
        FParticleEmitterSettings burst;
        burst.Kind = EParticleKind::SpriteBurst;
        burst.BurstCount = bGrenade ? 42 : 32;
        burst.Lifetime = bGrenade ? 0.45f : 0.35f;
        burst.Size = 0.1f;
        burst.SizeEnd = bGrenade ? 0.55f : 0.4f;
        burst.Color = bGrenade ? glm::vec4(0.45f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.55f, 0.12f, 1.0f);
        burst.ColorEnd = {0.2f, 0.05f, 0.02f, 0.0f};
        burst.VelocityMin = {-5.0f, 0.5f, -5.0f};
        burst.VelocityMax = {5.0f, 8.0f, 5.0f};
        burst.Gravity = {0.0f, -8.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, burst, InLocation);

        FParticleEmitterSettings flash;
        flash.Kind = EParticleKind::SpriteBurst;
        flash.BurstCount = 10;
        flash.Lifetime = 0.12f;
        flash.Size = 0.25f;
        flash.SizeEnd = 0.7f;
        flash.Color = {1.0f, 0.9f, 0.5f, 1.0f};
        flash.ColorEnd = {1.0f, 0.4f, 0.05f, 0.0f};
        flash.VelocityMin = {-0.2f, -0.2f, -0.2f};
        flash.VelocityMax = {0.2f, 0.2f, 0.2f};
        flash.Gravity = {0.0f, 0.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, flash, InLocation);
        UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InLocation, 1.2f, 5500.0f);

        auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode());
        if (gm && InstigatorCharacter && World->GetNetMode() != ENetMode::Client) {
            auto apply = [&](ALeonTournamentCharacter* target, float amount, EDamageType type) {
                if (!target || target->IsPendingKill())
                    return;

                glm::vec3 away = target->GetActorLocation() - InLocation;
                const float dist = glm::length(away);
                if (dist < 1e-3f)
                    away = glm::vec3(0.0f, 1.0f, 0.0f);
                else
                    away /= dist;
                away.y = std::max(away.y, 0.4f);
                away = glm::normalize(away);
                const float falloff = 1.0f - std::clamp(dist / std::max(0.01f, Config.SplashRadius), 0.0f, 1.0f);
                const float selfBoost = (target == InstigatorCharacter) ? 1.4f : 1.0f;
                const float strength = (Config.Knockback + amount * 0.12f) * falloff * selfBoost;
                const glm::vec3 impulse = away * strength;

                auto health = target->GetHealthComponent();
                const bool bAlreadyDead = health && health->IsDead();
                if (!bAlreadyDead) {
                    FDamageInfo info;
                    info.DamageAmount = amount;
                    info.DamageType = type;
                    info.Instigator = InstigatorCharacter;
                    info.Causer = CauserWeapon ? static_cast<AActor*>(CauserWeapon) : this;
                    info.HitActor = target;
                    info.HitLocation = InLocation;
                    info.HitNormal = InNormal;
                    info.Impulse = impulse;
                    if (gm->ApplyAuthoritativeDamage(*InstigatorCharacter, *target, info)) {
                        const bool bKill = target->GetHealthComponent() && target->GetHealthComponent()->IsDead();
                        if (target != InstigatorCharacter)
                            InstigatorCharacter->PulseHitConfirm(bKill);
                    }
                }

                target->ApplyLaunchVelocity(impulse);
            };

            if (InDirectHit)
                apply(InDirectHit, Config.Damage, EDamageType::Point);

            for (const auto& actorRef : World->GetAllActors()) {
                auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actorRef.get());
                if (!ch || ch == InDirectHit)
                    continue;
                const float dist = glm::length(ch->GetActorLocation() - InLocation);
                if (dist > Config.SplashRadius)
                    continue;
                const float falloff = 1.0f - dist / std::max(0.01f, Config.SplashRadius);
                apply(ch, Config.SplashDamage * falloff, EDamageType::Radial);
            }
        }
    }

    void ALeonTournamentProjectile::NotifyHit(const FHitResult& InHit) {
        if (HasExploded())
            return;
        auto* target = dynamic_cast<ALeonTournamentCharacter*>(InHit.Actor);
        if (target == InstigatorCharacter)
            return;
        Explode(InHit.Location, InHit.Normal, target);
    }

    void ALeonTournamentProjectile::OnLifeSpanExpired() {
        Explode(GetActorLocation(), glm::vec3(0.0f, 1.0f, 0.0f), nullptr);
    }

    void ALeonTournamentProjectile::Tick(float DeltaSeconds) {
        AProjectile::Tick(DeltaSeconds);
        if (HasExploded()) {
            if (World)
                World->DestroyActor(this);
            return;
        }
        TrailCooldown -= DeltaSeconds;
        if (TrailCooldown <= 0.0f) {
            SpawnTrail();
            TrailCooldown = 0.04f;
        }
    }

} // namespace Leon
