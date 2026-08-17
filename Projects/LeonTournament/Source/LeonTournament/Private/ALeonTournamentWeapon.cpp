#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentProjectile.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Gameplay/UParticleComponent.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"
#include "Renderer/FRenderingMath.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace Leon {

    namespace {
        std::mt19937& WeaponRng() {
            static thread_local std::mt19937 rng{std::random_device{}()};
            return rng;
        }

        void EnsureWeaponMesh(AActor& InActor, const FLeonTournamentWeaponConfig& InConfig) {
            if (InActor.HasComponent<FMeshComponent>()) {
                if (InActor.HasComponent<FMaterialComponent>()) {
                    if (auto mat = InActor.GetComponent<FMaterialComponent>().MaterialInstance)
                        mat->SetAlbedoColor(InConfig.VisualColor);
                }
                return;
            }
            if (!FApplication::HasInstance())
                return;
            auto va = FMeshPrimitives::CreateCylinder(InConfig.VisualRadius, InConfig.VisualRadius * 0.7f,
                                                      InConfig.VisualLength, 10, true);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!va || !shader)
                return;
            auto& mesh = InActor.AddComponent<FMeshComponent>(va, shader);
            mesh.MeshType = "Cylinder";
            mesh.MeshRadius = InConfig.VisualRadius;
            mesh.MeshHeight = InConfig.VisualLength;
            mesh.Mobility = EComponentMobility::Movable;
            mesh.bCastShadows = false;
            if (auto parent = UAssetManager::GetDefaultMaterial()) {
                auto inst = parent->CreateInstance("WeaponMat");
                inst->SetAlbedoColor(InConfig.VisualColor);
                InActor.AddComponent<FMaterialComponent>(inst);
            }
        }
    } // namespace

    ALeonTournamentWeapon::ALeonTournamentWeapon(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentWeapon");
        Config = LeonTournamentWeaponPreset(WeaponId);
        CurrentAmmo = Config.MagazineSize;
        CurrentSpreadDeg = Config.BaseSpreadDeg;
    }

    ALeonTournamentRifle::ALeonTournamentRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentRifle");
        SetWeaponId(ELeonTournamentWeaponId::Rifle);
    }

    ALeonTournamentShotgun::ALeonTournamentShotgun(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentShotgun");
        SetWeaponId(ELeonTournamentWeaponId::Shotgun);
    }

    ALeonTournamentRocketLauncher::ALeonTournamentRocketLauncher(entt::entity InHandle, UWorld* InWorld,
                                                                 const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentRocketLauncher");
        SetWeaponId(ELeonTournamentWeaponId::Rocket);
    }

    ALeonTournamentLaserRifle::ALeonTournamentLaserRifle(entt::entity InHandle, UWorld* InWorld,
                                                         const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentLaserRifle");
        SetWeaponId(ELeonTournamentWeaponId::Laser);
    }

    ALeonTournamentGrenadeLauncher::ALeonTournamentGrenadeLauncher(entt::entity InHandle, UWorld* InWorld,
                                                                   const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentGrenadeLauncher");
        SetWeaponId(ELeonTournamentWeaponId::Grenade);
    }

    ALeonTournamentFlamethrower::ALeonTournamentFlamethrower(entt::entity InHandle, UWorld* InWorld,
                                                             const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentFlamethrower");
        SetWeaponId(ELeonTournamentWeaponId::Flamethrower);
    }

    void ALeonTournamentWeapon::SetWeaponId(ELeonTournamentWeaponId InId) {
        WeaponId = InId;
        SetConfig(LeonTournamentWeaponPreset(InId));
    }

    void ALeonTournamentWeapon::SetConfig(const FLeonTournamentWeaponConfig& InConfig) {
        Config = InConfig;
        CurrentAmmo = Config.MagazineSize;
        CurrentSpreadDeg = Config.BaseSpreadDeg;
    }

    float ALeonTournamentWeapon::GetSpreadAlpha() const {
        const float range = std::max(1e-4f, Config.MaxSpreadDeg - Config.BaseSpreadDeg);
        return std::clamp((CurrentSpreadDeg - Config.BaseSpreadDeg) / range, 0.0f, 1.0f);
    }

    void ALeonTournamentWeapon::AddShotBloom() {
        CurrentSpreadDeg = std::min(Config.MaxSpreadDeg, CurrentSpreadDeg + Config.SpreadPerShotDeg);
    }

    glm::vec3 ALeonTournamentWeapon::ApplyAimSpread(const glm::vec3& InForward, float InHalfAngleDeg) const {
        const float halfRad = glm::radians(std::max(0.0f, InHalfAngleDeg));
        if (halfRad < 1e-5f)
            return glm::normalize(InForward);

        glm::vec3 right, up;
        StableViewBasis(InForward, right, up);
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        std::uniform_real_distribution<float> angle(0.0f, 6.2831853f);
        const float r = std::sqrt(unit(WeaponRng()));
        const float phi = angle(WeaponRng());
        const float offset = std::tan(halfRad) * r;
        return glm::normalize(InForward + right * (std::cos(phi) * offset) + up * (std::sin(phi) * offset));
    }

    void ALeonTournamentWeapon::AttachVisual() {
        EnsureWeaponMesh(*this, Config);
        bVisualReady = HasComponent<FMeshComponent>();
        SetVisualHidden(bVisualHidden);
    }

    void ALeonTournamentWeapon::SetVisualHidden(bool bHidden) {
        bVisualHidden = bHidden;
        if (HasComponent<FMeshComponent>())
            GetComponent<FMeshComponent>().bVisible = !bHidden;
    }

    glm::vec3 ALeonTournamentWeapon::GetMuzzleLocation() const {
        return GetActorLocation();
    }

    bool ALeonTournamentWeapon::CanFire() const {
        if (!OwnerCharacter || bReloading || CurrentAmmo <= 0 || FireCooldown > 0.0f)
            return false;
        if (auto health = OwnerCharacter->GetHealthComponent()) {
            if (health->IsDead())
                return false;
        }
        return true;
    }

    bool ALeonTournamentWeapon::NeedsReload() const {
        if (!OwnerCharacter || bReloading || CurrentAmmo > 0)
            return false;
        if (auto health = OwnerCharacter->GetHealthComponent()) {
            if (health->IsDead())
                return false;
        }
        return CurrentAmmo < Config.MagazineSize;
    }

    void ALeonTournamentWeapon::SetFireHeld(bool bHeld) {
        const bool bWasHeld = bFireHeld;
        bFireHeld = bHeld;
        if (bFireHeld && NeedsReload()) {
            StartReload();
        } else if (bFireHeld && !bWasHeld && CurrentAmmo <= 0) {
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_EmptyClick", 0.55f);
        }
    }

    void ALeonTournamentWeapon::SpawnFireEffects(const glm::vec3& InMuzzle, const glm::vec3& InTracerStart,
                                                 const glm::vec3& InTraceEnd, bool bHitWorld, bool bHitCharacter) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;

        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = WeaponId == ELeonTournamentWeaponId::Shotgun ? 16 : 10;
        muzzle.Lifetime = 0.07f;
        muzzle.Size = 0.05f;
        muzzle.SizeEnd = 0.12f;
        muzzle.Color = {1.0f, 0.82f, 0.35f, 1.0f};
        muzzle.ColorEnd = {1.0f, 0.4f, 0.05f, 0.0f};
        muzzle.VelocityMin = {-0.15f, -0.05f, -0.15f};
        muzzle.VelocityMax = {0.15f, 0.25f, 0.15f};
        muzzle.Gravity = {0.0f, 0.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, muzzle, InMuzzle))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings tracer;
        tracer.Kind = EParticleKind::Beam;
        tracer.Lifetime = 0.055f;
        tracer.Color = {1.0f, 0.88f, 0.4f, 0.75f};
        tracer.ColorEnd = {1.0f, 0.55f, 0.15f, 0.0f};
        tracer.BeamEnd = InTraceEnd;
        tracer.BeamThickness = WeaponId == ELeonTournamentWeaponId::Shotgun ? 0.01f : 0.014f;
        if (UGameplayStatics::SpawnEmitterAtLocation(World, tracer, InTracerStart))
            ++LastVfxSpawnCount;

        if (bHitWorld || bHitCharacter) {
            FParticleEmitterSettings impact;
            impact.Kind = EParticleKind::SpriteBurst;
            impact.BurstCount = bHitCharacter ? 12 : 8;
            impact.Lifetime = 0.12f;
            impact.Size = 0.04f;
            impact.SizeEnd = 0.01f;
            if (bHitCharacter) {
                impact.Color = {0.85f, 0.12f, 0.12f, 1.0f};
                impact.ColorEnd = {0.4f, 0.02f, 0.02f, 0.0f};
            } else {
                impact.Color = {0.85f, 0.8f, 0.55f, 1.0f};
                impact.ColorEnd = {0.45f, 0.4f, 0.3f, 0.0f};
            }
            impact.VelocityMin = {-0.8f, 0.2f, -0.8f};
            impact.VelocityMax = {0.8f, 1.6f, 0.8f};
            impact.Gravity = {0.0f, -8.0f, 0.0f};
            if (UGameplayStatics::SpawnEmitterAtLocation(World, impact, InTraceEnd))
                ++LastVfxSpawnCount;
        }

        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 0.95f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 0.9f, 4500.0f);
        if (bHitCharacter)
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_HitConfirm", InTraceEnd, 0.8f, 2800.0f);
    }

    void ALeonTournamentWeapon::SpawnRocketLaunchEffects(const glm::vec3& InMuzzle) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;
        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = 28;
        muzzle.Lifetime = 0.16f;
        muzzle.Size = 0.1f;
        muzzle.SizeEnd = 0.32f;
        muzzle.Color = {1.0f, 0.55f, 0.15f, 1.0f};
        muzzle.ColorEnd = {0.8f, 0.15f, 0.05f, 0.0f};
        muzzle.VelocityMin = {-0.45f, -0.1f, -0.45f};
        muzzle.VelocityMax = {0.45f, 0.55f, 0.45f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, muzzle, InMuzzle))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 16;
        smoke.Lifetime = 0.35f;
        smoke.Size = 0.12f;
        smoke.SizeEnd = 0.4f;
        smoke.Color = {0.35f, 0.28f, 0.22f, 0.7f};
        smoke.ColorEnd = {0.08f, 0.06f, 0.05f, 0.0f};
        smoke.VelocityMin = {-0.3f, 0.1f, -0.3f};
        smoke.VelocityMax = {0.3f, 1.2f, 0.3f};
        smoke.Gravity = {0.0f, 1.5f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, smoke, InMuzzle))
            ++LastVfxSpawnCount;

        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.0f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.0f, 5000.0f);
    }

    void ALeonTournamentWeapon::SpawnLaserEffects(const glm::vec3& InMuzzle, const glm::vec3& InTraceEnd,
                                                  bool bHitCharacter) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;
        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = 18;
        muzzle.Lifetime = 0.12f;
        muzzle.Size = 0.07f;
        muzzle.SizeEnd = 0.22f;
        muzzle.Color = {0.35f, 0.95f, 1.0f, 1.0f};
        muzzle.ColorEnd = {0.05f, 0.35f, 1.0f, 0.0f};
        muzzle.VelocityMin = {-0.2f, -0.1f, -0.2f};
        muzzle.VelocityMax = {0.2f, 0.3f, 0.2f};
        muzzle.Gravity = {0.0f, 0.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, muzzle, InMuzzle))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings beam;
        beam.Kind = EParticleKind::Beam;
        beam.Lifetime = 0.16f;
        beam.Color = {0.45f, 1.0f, 1.0f, 0.95f};
        beam.ColorEnd = {0.1f, 0.4f, 1.0f, 0.0f};
        beam.BeamEnd = InTraceEnd;
        beam.BeamThickness = 0.045f;
        if (UGameplayStatics::SpawnEmitterAtLocation(World, beam, InMuzzle))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings impact;
        impact.Kind = EParticleKind::SpriteBurst;
        impact.BurstCount = bHitCharacter ? 22 : 14;
        impact.Lifetime = 0.2f;
        impact.Size = 0.06f;
        impact.SizeEnd = 0.02f;
        impact.Color = bHitCharacter ? glm::vec4(1.0f, 0.2f, 0.35f, 1.0f) : glm::vec4(0.5f, 0.9f, 1.0f, 1.0f);
        impact.ColorEnd = {0.1f, 0.2f, 0.8f, 0.0f};
        impact.VelocityMin = {-1.4f, 0.2f, -1.4f};
        impact.VelocityMax = {1.4f, 2.2f, 1.4f};
        impact.Gravity = {0.0f, -6.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, impact, InTraceEnd))
            ++LastVfxSpawnCount;

        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.15f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.1f, 5200.0f);
        if (bHitCharacter)
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_HitConfirm", InTraceEnd, 0.9f, 3000.0f);
    }

    void ALeonTournamentWeapon::SpawnShotgunBlastEffects(const glm::vec3& InMuzzle, const glm::vec3& InAimDir) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;
        const glm::vec3 n = glm::length(InAimDir) > 1e-5f ? glm::normalize(InAimDir) : glm::vec3(0.0f, 0.0f, 1.0f);

        FParticleEmitterSettings flash;
        flash.Kind = EParticleKind::SpriteBurst;
        flash.BurstCount = 28;
        flash.Lifetime = 0.1f;
        flash.Size = 0.08f;
        flash.SizeEnd = 0.22f;
        flash.Color = {1.0f, 0.85f, 0.35f, 1.0f};
        flash.ColorEnd = {1.0f, 0.35f, 0.05f, 0.0f};
        flash.VelocityMin = n * 2.0f + glm::vec3(-1.5f, -0.4f, -1.5f);
        flash.VelocityMax = n * 8.0f + glm::vec3(1.5f, 1.5f, 1.5f);
        flash.Gravity = {0.0f, -2.0f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, flash, InMuzzle))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 14;
        smoke.Lifetime = 0.35f;
        smoke.Size = 0.1f;
        smoke.SizeEnd = 0.35f;
        smoke.Color = {0.35f, 0.3f, 0.25f, 0.65f};
        smoke.ColorEnd = {0.08f, 0.07f, 0.06f, 0.0f};
        smoke.VelocityMin = n * 0.5f + glm::vec3(-0.5f, 0.2f, -0.5f);
        smoke.VelocityMax = n * 2.5f + glm::vec3(0.5f, 1.2f, 0.5f);
        smoke.Gravity = {0.0f, 1.5f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, smoke, InMuzzle + n * 0.15f))
            ++LastVfxSpawnCount;

        for (int i = 0; i < 6; ++i) {
            const glm::vec3 dir = ApplyAimSpread(n, CurrentSpreadDeg + Config.PelletSpreadDeg);
            FParticleEmitterSettings tracer;
            tracer.Kind = EParticleKind::Beam;
            tracer.Lifetime = 0.07f;
            tracer.Color = {1.0f, 0.9f, 0.45f, 0.7f};
            tracer.ColorEnd = {1.0f, 0.45f, 0.1f, 0.0f};
            tracer.BeamEnd = InMuzzle + dir * 4.5f;
            tracer.BeamThickness = 0.014f;
            UGameplayStatics::SpawnEmitterAtLocation(World, tracer, InMuzzle + dir * 0.1f);
        }

        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 1.05f);
        else
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_RifleFire", InMuzzle, 1.0f, 4800.0f);
    }

    void ALeonTournamentWeapon::SpawnFlameEffects(const glm::vec3& InMuzzle, const glm::vec3& InDir) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;
        const glm::vec3 n = glm::length(InDir) > 1e-5f ? glm::normalize(InDir) : glm::vec3(0.0f, 0.0f, 1.0f);
        // Lifetime * forward speed ≈ Config.Range (2 m) so the spray visually matches damage reach.
        const float reach = std::max(0.5f, Config.Range);
        const float life = 0.42f;
        const float speedMin = reach / life * 0.85f;
        const float speedMax = reach / life * 1.15f;

        FParticleEmitterSettings flame;
        flame.Kind = EParticleKind::SpriteBurst;
        flame.BurstCount = 48;
        flame.Lifetime = life;
        flame.Size = 0.18f;
        flame.SizeEnd = 0.55f;
        flame.Color = {1.0f, 0.55f, 0.08f, 1.0f};
        flame.ColorEnd = {0.35f, 0.05f, 0.0f, 0.0f};
        flame.VelocityMin = n * speedMin + glm::vec3(-1.8f, -0.35f, -1.8f);
        flame.VelocityMax = n * speedMax + glm::vec3(1.8f, 2.0f, 1.8f);
        flame.Gravity = {0.0f, 1.8f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, flame, InMuzzle + n * 0.2f))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings core;
        core.Kind = EParticleKind::SpriteBurst;
        core.BurstCount = 22;
        core.Lifetime = life * 0.85f;
        core.Size = 0.12f;
        core.SizeEnd = 0.38f;
        core.Color = {1.0f, 0.85f, 0.25f, 1.0f};
        core.ColorEnd = {1.0f, 0.25f, 0.02f, 0.0f};
        core.VelocityMin = n * (speedMin * 0.9f) + glm::vec3(-0.9f, -0.15f, -0.9f);
        core.VelocityMax = n * (speedMax * 1.05f) + glm::vec3(0.9f, 1.1f, 0.9f);
        core.Gravity = {0.0f, 1.2f, 0.0f};
        if (UGameplayStatics::SpawnEmitterAtLocation(World, core, InMuzzle + n * 0.15f))
            ++LastVfxSpawnCount;

        FParticleEmitterSettings smoke;
        smoke.Kind = EParticleKind::SpriteBurst;
        smoke.BurstCount = 18;
        smoke.Lifetime = 0.55f;
        smoke.Size = 0.22f;
        smoke.SizeEnd = 0.65f;
        smoke.Color = {0.28f, 0.18f, 0.1f, 0.6f};
        smoke.ColorEnd = {0.05f, 0.05f, 0.05f, 0.0f};
        smoke.VelocityMin = n * (speedMin * 0.55f) + glm::vec3(-1.0f, 0.5f, -1.0f);
        smoke.VelocityMax = n * (speedMax * 0.75f) + glm::vec3(1.0f, 2.2f, 1.0f);
        smoke.Gravity = {0.0f, 2.4f, 0.0f};
        UGameplayStatics::SpawnEmitterAtLocation(World, smoke, InMuzzle + n * 0.35f);

        if (OwnerCharacter && OwnerCharacter->IsLocallyControlled())
            UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleFire", 0.35f);
    }

    bool ALeonTournamentWeapon::ApplyHitscanDamage(const glm::vec3& InOrigin, const glm::vec3& InDir, float InDamage,
                                                   glm::vec3& OutTraceEnd, bool& OutHitWorld, bool& OutHitCharacter,
                                                   int32_t InMaxBounces) {
        OutHitWorld = false;
        OutHitCharacter = false;
        OutTraceEnd = InOrigin + InDir * Config.Range;
        if (!World)
            return false;

        glm::vec3 origin = InOrigin;
        glm::vec3 dir = glm::length(InDir) > 1e-5f ? glm::normalize(InDir) : glm::vec3(0.0f, 0.0f, 1.0f);
        float remaining = Config.Range;
        bool bDamaged = false;

        for (int bounce = 0; bounce <= InMaxBounces && remaining > 0.02f; ++bounce) {
            UWorld::FHitResult hit;
            const glm::vec3 end = origin + dir * remaining;
            const bool bHit =
                World->LineTraceByChannel(origin, end, ECollisionChannel::Visibility, OwnerCharacter, hit);
            if (FDebugRenderer::IsTraceCaptureEnabled()) {
                FDebugRenderer::RecordLineTrace(origin, end, bHit && hit.bBlockingHit, hit.Location, hit.Normal,
                                                static_cast<uint8_t>(ECollisionChannel::Visibility));
            }

            if (!(bHit && hit.bBlockingHit)) {
                OutTraceEnd = end;
                break;
            }

            OutTraceEnd = hit.Location;
            const float traveled = glm::length(hit.Location - origin);
            remaining = std::max(0.0f, remaining - traveled);

            auto* target = dynamic_cast<ALeonTournamentCharacter*>(hit.Actor);
            if (target && target != OwnerCharacter) {
                OutHitCharacter = true;
                FDamageInfo info;
                info.DamageAmount = InDamage;
                info.DamageType = EDamageType::Point;
                info.Instigator = OwnerCharacter;
                info.Causer = this;
                info.HitActor = target;
                info.HitLocation = hit.Location;
                info.HitNormal = hit.Normal;
                info.Impulse = dir * Config.Knockback;
                if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode())) {
                    if (gm->ApplyAuthoritativeDamage(*OwnerCharacter, *target, info)) {
                        bDamaged = true;
                        const bool bKill = target->GetHealthComponent() && target->GetHealthComponent()->IsDead();
                        OwnerCharacter->PulseHitConfirm(bKill);
                    }
                }
                target->ApplyLaunchVelocity(dir * Config.Knockback + glm::vec3(0.0f, 0.4f, 0.0f));
                break;
            }

            OutHitWorld = true;
            if (bounce >= InMaxBounces)
                break;

            const glm::vec3 n =
                glm::length(hit.Normal) > 1e-5f ? glm::normalize(hit.Normal) : glm::vec3(0.0f, 1.0f, 0.0f);
            dir = glm::normalize(glm::reflect(dir, n));
            origin = hit.Location + n * 0.03f;

            FParticleEmitterSettings spark;
            spark.Kind = EParticleKind::SpriteBurst;
            spark.BurstCount = 6;
            spark.Lifetime = 0.1f;
            spark.Size = 0.03f;
            spark.SizeEnd = 0.01f;
            spark.Color = {1.0f, 0.9f, 0.45f, 1.0f};
            spark.ColorEnd = {1.0f, 0.4f, 0.05f, 0.0f};
            spark.VelocityMin = dir * 1.5f + glm::vec3(-0.5f, 0.1f, -0.5f);
            spark.VelocityMax = dir * 4.0f + glm::vec3(0.5f, 1.2f, 0.5f);
            spark.Gravity = {0.0f, -6.0f, 0.0f};
            UGameplayStatics::SpawnEmitterAtLocation(World, spark, hit.Location);

            FParticleEmitterSettings bounceBeam;
            bounceBeam.Kind = EParticleKind::Beam;
            bounceBeam.Lifetime = 0.05f;
            bounceBeam.Color = {1.0f, 0.88f, 0.4f, 0.55f};
            bounceBeam.ColorEnd = {1.0f, 0.55f, 0.15f, 0.0f};
            bounceBeam.BeamEnd = origin + dir * std::min(remaining, 8.0f);
            bounceBeam.BeamThickness = 0.01f;
            UGameplayStatics::SpawnEmitterAtLocation(World, bounceBeam, origin);
        }

        return bDamaged;
    }

    bool ALeonTournamentWeapon::FireHitscan() {
        glm::vec3 origin, aimDir;
        OwnerCharacter->GetAimRay(origin, aimDir);
        AddShotBloom();
        if (Config.RecoilPitchDeg > 0.0f) {
            std::uniform_real_distribution<float> kick(0.55f, 1.0f);
            OwnerCharacter->SetControlPitch(OwnerCharacter->GetControlPitch() + Config.RecoilPitchDeg * kick(WeaponRng()));
        }

        const int pellets = std::max(1, Config.PelletCount);
        bool anyHitChar = false;
        bool anyHitWorld = false;
        glm::vec3 primaryEnd = origin + aimDir * Config.Range;
        const glm::vec3 muzzle = OwnerCharacter->GetMuzzleSocketLocation();

        for (int i = 0; i < pellets; ++i) {
            const float cone = CurrentSpreadDeg + Config.PelletSpreadDeg;
            glm::vec3 dir = ApplyAimSpread(aimDir, cone);
            glm::vec3 traceEnd;
            bool hitWorld = false;
            bool hitChar = false;
            ApplyHitscanDamage(origin, dir, Config.Damage, traceEnd, hitWorld, hitChar, Config.RicochetBounces);
            if (i == 0)
                primaryEnd = traceEnd;
            anyHitChar = anyHitChar || hitChar;
            anyHitWorld = anyHitWorld || hitWorld;
            if (pellets > 1 && i > 0) {
                const glm::vec3 tracerStart = origin + dir * 0.25f;
                FParticleEmitterSettings tracer;
                tracer.Kind = EParticleKind::Beam;
                tracer.Lifetime = 0.04f;
                tracer.Color = {1.0f, 0.88f, 0.4f, 0.45f};
                tracer.ColorEnd = {1.0f, 0.55f, 0.15f, 0.0f};
                tracer.BeamEnd = traceEnd;
                tracer.BeamThickness = 0.008f;
                UGameplayStatics::SpawnEmitterAtLocation(World, tracer, tracerStart);
            }
        }

        const glm::vec3 tracerStart = origin + aimDir * 0.35f;
        if (WeaponId == ELeonTournamentWeaponId::Laser)
            SpawnLaserEffects(muzzle, primaryEnd, anyHitChar);
        else
            SpawnFireEffects(muzzle, tracerStart, primaryEnd, anyHitWorld, anyHitChar);
        return true;
    }

    bool ALeonTournamentWeapon::FireProjectile() {
        glm::vec3 origin, aimDir;
        OwnerCharacter->GetAimRay(origin, aimDir);
        AddShotBloom();
        if (Config.RecoilPitchDeg > 0.0f) {
            std::uniform_real_distribution<float> kick(0.7f, 1.0f);
            OwnerCharacter->SetControlPitch(OwnerCharacter->GetControlPitch() + Config.RecoilPitchDeg * kick(WeaponRng()));
        }

        const glm::vec3 muzzle = OwnerCharacter->GetMuzzleSocketLocation();
        const int shots = std::max(1, Config.PelletCount);
        const float cone = CurrentSpreadDeg + Config.PelletSpreadDeg;
        bool anySpawned = false;
        for (int i = 0; i < shots; ++i) {
            glm::vec3 dir = (shots > 1) ? ApplyAimSpread(aimDir, cone) : ApplyAimSpread(aimDir, CurrentSpreadDeg);
            auto* proj = World->SpawnActor<ALeonTournamentProjectile>(shots > 1 ? "Pellet" : "Projectile");
            if (!proj)
                continue;
            proj->SetActorLocation(origin + dir * 0.45f);
            proj->Launch(OwnerCharacter, this, dir, Config);
            anySpawned = true;
        }
        if (!anySpawned)
            return false;

        if (WeaponId == ELeonTournamentWeaponId::Shotgun)
            SpawnShotgunBlastEffects(muzzle, aimDir);
        else
            SpawnRocketLaunchEffects(muzzle);
        return true;
    }

    bool ALeonTournamentWeapon::FireFlame() {
        glm::vec3 origin, aimDir;
        OwnerCharacter->GetAimRay(origin, aimDir);
        AddShotBloom();
        const glm::vec3 muzzle = OwnerCharacter->GetMuzzleSocketLocation();
        SpawnFlameEffects(muzzle, aimDir);

        const int rays = std::max(1, Config.PelletCount);
        bool anyHit = false;
        for (int i = 0; i < rays; ++i) {
            glm::vec3 dir = ApplyAimSpread(aimDir, CurrentSpreadDeg + Config.FlameConeDeg);
            glm::vec3 traceEnd;
            bool hitWorld = false;
            bool hitChar = false;
            if (ApplyHitscanDamage(origin, dir, Config.Damage, traceEnd, hitWorld, hitChar))
                anyHit = true;
        }
        (void)anyHit;
        return true;
    }

    bool ALeonTournamentWeapon::ServerFire() {
        if (!World || World->GetNetMode() == ENetMode::Client)
            return false;
        if (!CanFire() || !OwnerCharacter)
            return false;
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode())) {
            auto* gs = gm->GetGameState();
            if (gs && gs->GetMatchState() != ELeonTournamentMatchState::Playing)
                return false;
        }

        --CurrentAmmo;
        FireCooldown = Config.FireRate > 0.0f ? 1.0f / Config.FireRate : 0.1f;
        bFiring = true;

        if (CurrentAmmo <= 0)
            StartReload();

        if (Config.FireMode == ELeonTournamentFireMode::Projectile)
            return FireProjectile();
        if (Config.FireMode == ELeonTournamentFireMode::Flame)
            return FireFlame();
        return FireHitscan();
    }

    bool ALeonTournamentWeapon::StartReload() {
        if (World && World->GetNetMode() == ENetMode::Client)
            return false;
        if (bReloading || CurrentAmmo >= Config.MagazineSize)
            return false;
        if (OwnerCharacter && OwnerCharacter->GetHealthComponent() && OwnerCharacter->GetHealthComponent()->IsDead())
            return false;
        bReloading = true;
        ReloadRemaining = Config.ReloadTime;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleReload", 0.7f);
        return true;
    }

    void ALeonTournamentWeapon::CancelReload() {
        bReloading = false;
        ReloadRemaining = 0.0f;
    }

    void ALeonTournamentWeapon::ResetMagazine() {
        CancelReload();
        CurrentAmmo = Config.MagazineSize;
        FireCooldown = 0.0f;
        CurrentSpreadDeg = Config.BaseSpreadDeg;
        bFiring = false;
        bFireHeld = false;
    }

    void ALeonTournamentWeapon::ApplyReplicatedState(int32_t InAmmo, bool bInReloading) {
        CurrentAmmo = InAmmo;
        bReloading = bInReloading;
        if (!bReloading)
            ReloadRemaining = 0.0f;
    }

    void ALeonTournamentWeapon::UpdateFirstPersonVisual() {
        if (!OwnerCharacter || OwnerCharacter->IsPendingKill())
            return;
        glm::vec3 camOrigin, look;
        OwnerCharacter->GetAimRay(camOrigin, look);
        (void)camOrigin;
        const glm::vec3 muzzle = OwnerCharacter->GetMuzzleSocketLocation();
        const float kick = bFiring ? 0.03f : 0.0f;
        SetActorLocation(muzzle + look * (0.12f - kick));
        SetActorRotation(Leon::EulerAligningLocalY(look));
        SetVisualHidden(bVisualHidden);
    }

    void ALeonTournamentWeapon::Tick(float DeltaSeconds) {
        if (FireCooldown > 0.0f)
            FireCooldown = std::max(0.0f, FireCooldown - DeltaSeconds);
        if (CurrentSpreadDeg > Config.BaseSpreadDeg)
            CurrentSpreadDeg =
                std::max(Config.BaseSpreadDeg, CurrentSpreadDeg - Config.SpreadRecoveryPerSec * DeltaSeconds);

        const bool bAuthority = !World || World->GetNetMode() != ENetMode::Client;
        if (bAuthority && CurrentAmmo <= 0 && !bReloading)
            StartReload();
        if (bAuthority && bReloading) {
            ReloadRemaining -= DeltaSeconds;
            if (ReloadRemaining <= 0.0f) {
                CurrentAmmo = Config.MagazineSize;
                bReloading = false;
                ReloadRemaining = 0.0f;
            }
        }

        bFiring = false;
        if (bAuthority && bFireHeld && CanFire())
            ServerFire();

        UpdateFirstPersonVisual();
    }

} // namespace Leon
