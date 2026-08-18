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
    }

    void ALeonTournamentProjectile::AttachVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        const float radius =
            IsPellet() ? std::max(0.04f, Config.ProjectileRadius)
                       : std::max(0.08f, Config.ProjectileRadius > 0.0f ? Config.ProjectileRadius * 0.7f : 0.12f);
        auto va = FMeshPrimitives::CreateSphere(radius, IsPellet() ? 8 : 10, IsPellet() ? 8 : 10);
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
        RemainingBounces = std::max(0, InConfig.RicochetBounces);
        SetInstigatorPawn(InInstigator);

        const float speed = std::max(1.0f, InConfig.ProjectileSpeed);
        const float life = IsPellet() ? std::clamp(InConfig.Range / speed + 0.15f, 0.45f, 1.25f) : 4.0f;
        SetInitialLifeSpan(life);
        ArmDelay = IsPellet() ? 0.02f : 0.08f;

        if (ProjectileMovement) {
            ProjectileMovement->InitialSpeed = speed;
            ProjectileMovement->ProjectileGravityScale = InConfig.ProjectileGravityScale;
            ProjectileMovement->ProjectileRadius = InConfig.ProjectileRadius;
            ProjectileMovement->bRotationFollowsVelocity = true;
            ProjectileMovement->IgnoreActor = InInstigator;
            ProjectileMovement->bShouldBounce = IsPellet() && RemainingBounces > 0;
            ProjectileMovement->Bounciness = IsPellet() ? 0.9f : 0.35f;
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

        if (IsPellet()) {
            FParticleEmitterSettings streak;
            streak.Kind = EParticleKind::SpriteBurst;
            streak.BurstCount = 6;
            streak.Lifetime = 0.12f;
            streak.Size = 0.045f;
            streak.SizeEnd = 0.01f;
            streak.Color = {1.0f, 0.88f, 0.4f, 1.0f};
            streak.ColorEnd = {1.0f, 0.35f, 0.05f, 0.0f};
            streak.VelocityMin = {-0.15f, -0.15f, -0.15f};
            streak.VelocityMax = {0.15f, 0.15f, 0.15f};
            streak.Gravity = {0.0f, 0.0f, 0.0f};
            UGameplayStatics::SpawnEmitterAtLocation(World, streak, GetActorLocation());

            if (ProjectileMovement && glm::length(ProjectileMovement->GetVelocity()) > 1e-3f) {
                const glm::vec3 vel = ProjectileMovement->GetVelocity();
                const glm::vec3 back = GetActorLocation() - glm::normalize(vel) * 0.55f;
                FParticleEmitterSettings beam;
                beam.Kind = EParticleKind::Beam;
                beam.Lifetime = 0.05f;
                beam.Color = {1.0f, 0.9f, 0.45f, 0.75f};
                beam.ColorEnd = {1.0f, 0.45f, 0.1f, 0.0f};
                beam.BeamEnd = GetActorLocation();
                beam.BeamThickness = 0.012f;
                UGameplayStatics::SpawnEmitterAtLocation(World, beam, back);
            }
            return;
        }

        const bool bRocket = Config.ProjectileGravityScale < 0.1f;
        FParticleEmitterSettings trail;
        trail.Kind = EParticleKind::SpriteBurst;
        trail.BurstCount = bRocket ? 14 : 6;
        trail.Lifetime = bRocket ? 0.28f : 0.18f;
        trail.Size = bRocket ? 0.09f : 0.05f;
        trail.SizeEnd = bRocket ? 0.28f : 0.14f;
        trail.Color = {VisualColor.x, VisualColor.y, VisualColor.z, 0.9f};
        trail.ColorEnd = {VisualColor.x * 0.3f, VisualColor.y * 0.2f, VisualColor.z * 0.1f, 0.0f};
        trail.VelocityMin = {-0.55f, 0.0f, -0.55f};
        trail.VelocityMax = {0.55f, 0.85f, 0.55f};
        trail.Gravity = {0.0f, -2.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, trail, GetActorLocation());

        if (bRocket) {
            FParticleEmitterSettings smoke;
            smoke.Kind = EParticleKind::SpriteBurst;
            smoke.BurstCount = 8;
            smoke.Lifetime = 0.4f;
            smoke.Size = 0.12f;
            smoke.SizeEnd = 0.4f;
            smoke.Color = {0.3f, 0.25f, 0.2f, 0.55f};
            smoke.ColorEnd = {0.05f, 0.05f, 0.05f, 0.0f};
            smoke.VelocityMin = {-0.35f, 0.1f, -0.35f};
            smoke.VelocityMax = {0.35f, 1.0f, 0.35f};
            smoke.Gravity = {0.0f, 1.2f, 0.0f};
            UGameplayStatics::SpawnEmitterAtLocation(World, smoke, GetActorLocation());
        }
    }

    void ALeonTournamentProjectile::SpawnRicochetFx(const glm::vec3& InLocation, const glm::vec3& InNormal,
                                                    const glm::vec3& InOutDir) {
        if (!World)
            return;
        FParticleEmitterSettings spark;
        spark.Kind = EParticleKind::SpriteBurst;
        spark.BurstCount = 14;
        spark.Lifetime = 0.16f;
        spark.Size = 0.04f;
        spark.SizeEnd = 0.01f;
        spark.Color = {1.0f, 0.9f, 0.4f, 1.0f};
        spark.ColorEnd = {1.0f, 0.35f, 0.05f, 0.0f};
        spark.VelocityMin = InOutDir * 2.0f + glm::vec3(-1.2f, 0.2f, -1.2f);
        spark.VelocityMax = InOutDir * 7.0f + glm::vec3(1.2f, 2.5f, 1.2f);
        spark.Gravity = {0.0f, -10.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, spark, InLocation + InNormal * 0.02f);
    }

    void ALeonTournamentProjectile::ApplyPointDamage(ALeonTournamentCharacter* InTarget, const glm::vec3& InLocation,
                                                     const glm::vec3& InNormal, const glm::vec3& InImpulseDir) {
        if (!World || !InstigatorCharacter || !InTarget || World->GetNetMode() == ENetMode::Client)
            return;
        auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode());
        if (!gm)
            return;

        auto health = InTarget->GetHealthComponent();
        if (health && health->IsDead())
            return;

        FDamageInfo info;
        info.DamageAmount = Config.Damage;
        info.DamageType = EDamageType::Point;
        info.Instigator = InstigatorCharacter;
        info.Causer = CauserWeapon ? static_cast<AActor*>(CauserWeapon) : this;
        info.HitActor = InTarget;
        info.HitLocation = InLocation;
        info.HitNormal = InNormal;
        info.Impulse = InImpulseDir * Config.Knockback;
        if (gm->ApplyAuthoritativeDamage(*InstigatorCharacter, *InTarget, info)) {
            const bool bKill = InTarget->GetHealthComponent() && InTarget->GetHealthComponent()->IsDead();
            if (InTarget != InstigatorCharacter)
                InstigatorCharacter->PulseHitConfirm(bKill);
        }
        InTarget->ApplyLaunchVelocity(InImpulseDir * Config.Knockback + glm::vec3(0.0f, 0.35f, 0.0f));
    }

    void ALeonTournamentProjectile::ImpactPellet(const glm::vec3& InLocation, const glm::vec3& InNormal,
                                                 ALeonTournamentCharacter* InDirectHit) {
        if (HasExploded() || !World)
            return;
        MarkExploded();

        FParticleEmitterSettings impact;
        impact.Kind = EParticleKind::SpriteBurst;
        impact.BurstCount = InDirectHit ? 16 : 10;
        impact.Lifetime = 0.14f;
        impact.Size = 0.05f;
        impact.SizeEnd = 0.015f;
        if (InDirectHit) {
            impact.Color = {0.9f, 0.15f, 0.12f, 1.0f};
            impact.ColorEnd = {0.35f, 0.02f, 0.02f, 0.0f};
        } else {
            impact.Color = {1.0f, 0.85f, 0.4f, 1.0f};
            impact.ColorEnd = {0.45f, 0.3f, 0.1f, 0.0f};
        }
        impact.VelocityMin = {-1.2f, 0.2f, -1.2f};
        impact.VelocityMax = {1.2f, 2.2f, 1.2f};
        impact.Gravity = {0.0f, -8.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, impact, InLocation);

        if (InDirectHit) {
            glm::vec3 dir = InDirectHit->GetActorLocation() - InLocation;
            if (glm::length(dir) < 1e-4f && ProjectileMovement)
                dir = ProjectileMovement->GetVelocity();
            if (glm::length(dir) < 1e-4f)
                dir = -InNormal;
            dir = glm::normalize(dir);
            ApplyPointDamage(InDirectHit, InLocation, InNormal, dir);
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_HitConfirm", InLocation, 0.55f, 2200.0f);
        }
    }

    void ALeonTournamentProjectile::Explode(const glm::vec3& InLocation, const glm::vec3& InNormal,
                                            ALeonTournamentCharacter* InDirectHit) {
        if (HasExploded() || !World)
            return;
        MarkExploded();

        const bool bGrenade = Config.ProjectileGravityScale > 0.1f;
        FParticleEmitterSettings burst;
        burst.Kind = EParticleKind::SpriteBurst;
        burst.BurstCount = bGrenade ? 42 : 64;
        burst.Lifetime = bGrenade ? 0.45f : 0.55f;
        burst.Size = bGrenade ? 0.1f : 0.16f;
        burst.SizeEnd = bGrenade ? 0.55f : 0.85f;
        burst.Color = bGrenade ? glm::vec4(0.45f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 0.55f, 0.12f, 1.0f);
        burst.ColorEnd = {0.2f, 0.05f, 0.02f, 0.0f};
        const float splash = std::max(2.0f, Config.SplashRadius);
        burst.VelocityMin = {-splash * 1.1f, 0.5f, -splash * 1.1f};
        burst.VelocityMax = {splash * 1.1f, splash * 1.6f, splash * 1.1f};
        burst.Gravity = {0.0f, -8.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, burst, InLocation);

        FParticleEmitterSettings flash;
        flash.Kind = EParticleKind::SpriteBurst;
        flash.BurstCount = bGrenade ? 10 : 22;
        flash.Lifetime = bGrenade ? 0.12f : 0.2f;
        flash.Size = bGrenade ? 0.25f : 0.4f;
        flash.SizeEnd = bGrenade ? 0.7f : 1.35f;
        flash.Color = {1.0f, 0.9f, 0.5f, 1.0f};
        flash.ColorEnd = {1.0f, 0.4f, 0.05f, 0.0f};
        flash.VelocityMin = {-0.35f, -0.35f, -0.35f};
        flash.VelocityMax = {0.35f, 0.35f, 0.35f};
        flash.Gravity = {0.0f, 0.0f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, flash, InLocation);

        if (!bGrenade) {
            FParticleEmitterSettings ring;
            ring.Kind = EParticleKind::SpriteBurst;
            ring.BurstCount = 36;
            ring.Lifetime = 0.35f;
            ring.Size = 0.08f;
            ring.SizeEnd = 0.22f;
            ring.Color = {1.0f, 0.65f, 0.2f, 0.9f};
            ring.ColorEnd = {0.4f, 0.08f, 0.02f, 0.0f};
            ring.VelocityMin = {-splash, 0.05f, -splash};
            ring.VelocityMax = {splash, 1.5f, splash};
            ring.Gravity = {0.0f, -3.0f, 0.0f};
            UGameplayStatics::SpawnEmitterAtLocation(World, ring, InLocation);
        }

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
                const float falloff =
                    Config.SplashRadius > 0.05f
                        ? (1.0f - std::clamp(dist / std::max(0.01f, Config.SplashRadius), 0.0f, 1.0f))
                        : 1.0f;
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

            if (Config.SplashRadius > 0.05f) {
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
    }

    void ALeonTournamentProjectile::NotifyHit(const FHitResult& InHit) {
        if (HasExploded())
            return;
        auto* target = dynamic_cast<ALeonTournamentCharacter*>(InHit.Actor);
        if (target == InstigatorCharacter)
            return;

        if (IsPellet()) {
            if (target) {
                ImpactPellet(InHit.Location, InHit.Normal, target);
                return;
            }
            if (RemainingBounces > 0 && ProjectileMovement) {
                --RemainingBounces;
                glm::vec3 outDir = ProjectileMovement->GetVelocity();
                if (glm::length(InHit.Normal) > 1e-4f)
                    outDir = glm::reflect(glm::length(outDir) > 1e-4f ? glm::normalize(outDir)
                                                                     : glm::vec3(0.0f, 0.0f, 1.0f),
                                          glm::normalize(InHit.Normal));
                else if (glm::length(outDir) > 1e-4f)
                    outDir = glm::normalize(outDir);
                else
                    outDir = glm::vec3(0.0f, 1.0f, 0.0f);
                SpawnRicochetFx(InHit.Location, InHit.Normal, outDir);
                return;
            }
            ImpactPellet(InHit.Location, InHit.Normal, nullptr);
            return;
        }

        Explode(InHit.Location, InHit.Normal, target);
    }

    void ALeonTournamentProjectile::OnLifeSpanExpired() {
        if (IsPellet()) {
            if (!HasExploded() && World) {
                MarkExploded();
            }
            return;
        }
        Explode(GetActorLocation(), glm::vec3(0.0f, 1.0f, 0.0f), nullptr);
    }

    void ALeonTournamentProjectile::Tick(float DeltaSeconds) {
        AProjectile::Tick(DeltaSeconds);
        if (GetLocalRole() == ENetRole::SimulatedProxy) {
            TrailCooldown -= DeltaSeconds;
            if (TrailCooldown <= 0.0f) {
                SpawnTrail();
                TrailCooldown = IsPellet() ? 0.018f : 0.04f;
            }
            return;
        }
        if (HasExploded()) {
            if (World)
                World->DestroyActor(this);
            return;
        }
        TrailCooldown -= DeltaSeconds;
        if (TrailCooldown <= 0.0f) {
            SpawnTrail();
            if (IsPellet())
                TrailCooldown = 0.018f;
            else
                TrailCooldown = Config.ProjectileGravityScale < 0.1f ? 0.025f : 0.04f;
        }
        if (IsPellet() && ProjectileMovement)
            ProjectileMovement->bShouldBounce = RemainingBounces > 0;
    }

} // namespace Leon
