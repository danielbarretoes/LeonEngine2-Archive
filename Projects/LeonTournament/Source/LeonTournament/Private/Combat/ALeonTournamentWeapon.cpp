#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "ALeonTournamentProjectile.hpp"
#include "FLeonTournamentWeaponPresets.hpp"
#include "FLeonTournamentWeaponVfx.hpp"
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
#include <functional>
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
        : AWeaponBase(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentWeapon");
        Config = LeonTournamentWeaponPreset(WeaponId);
        SetConfig(Config);
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

    void ALeonTournamentWeapon::SetOwnerCharacter(ALeonTournamentCharacter* InOwner) {
        OwnerCharacter = InOwner;
        SetOwnerPawn(InOwner);
    }

    void ALeonTournamentWeapon::SetWeaponId(ELeonTournamentWeaponId InId) {
        WeaponId = InId;
        SetConfig(LeonTournamentWeaponPreset(InId));
    }

    void ALeonTournamentWeapon::SetConfig(const FLeonTournamentWeaponConfig& InConfig) {
        Config = InConfig;
        SetAmmoCapacity(Config.MagazineSize);
        SetFireRate(Config.FireRate);
        SetReloadTime(Config.ReloadTime);
        CurrentAmmo = AmmoCapacity;
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
        if (!AWeaponBase::CanFire() || !OwnerCharacter)
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
        const FLeonTournamentWeaponVfxContext ctx{World, OwnerCharacter, WeaponId, &Config};
        LastVfxSpawnCount = FLeonTournamentWeaponVfx::SpawnFireEffects(ctx, InMuzzle, InTracerStart, InTraceEnd,
                                                                       bHitWorld, bHitCharacter);
    }

    void ALeonTournamentWeapon::SpawnRocketLaunchEffects(const glm::vec3& InMuzzle) {
        const FLeonTournamentWeaponVfxContext ctx{World, OwnerCharacter, WeaponId, &Config};
        LastVfxSpawnCount = FLeonTournamentWeaponVfx::SpawnRocketLaunchEffects(ctx, InMuzzle);
    }

    void ALeonTournamentWeapon::SpawnLaserEffects(const glm::vec3& InMuzzle, const glm::vec3& InTraceEnd,
                                                  bool bHitCharacter) {
        const FLeonTournamentWeaponVfxContext ctx{World, OwnerCharacter, WeaponId, &Config};
        LastVfxSpawnCount = FLeonTournamentWeaponVfx::SpawnLaserEffects(ctx, InMuzzle, InTraceEnd, bHitCharacter);
    }

    void ALeonTournamentWeapon::SpawnShotgunBlastEffects(const glm::vec3& InMuzzle, const glm::vec3& InAimDir) {
        const FLeonTournamentWeaponVfxContext ctx{World, OwnerCharacter, WeaponId, &Config};
        LastVfxSpawnCount = FLeonTournamentWeaponVfx::SpawnShotgunBlastEffects(
            ctx, InMuzzle, InAimDir, CurrentSpreadDeg,
            [this](const glm::vec3& forward, float halfAngleDeg) { return ApplyAimSpread(forward, halfAngleDeg); });
    }

    void ALeonTournamentWeapon::SpawnFlameEffects(const glm::vec3& InMuzzle, const glm::vec3& InDir) {
        const FLeonTournamentWeaponVfxContext ctx{World, OwnerCharacter, WeaponId, &Config};
        LastVfxSpawnCount = FLeonTournamentWeaponVfx::SpawnFlameEffects(ctx, InMuzzle, InDir);
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
            OwnerCharacter->SetControlPitch(OwnerCharacter->GetControlPitch() +
                                            Config.RecoilPitchDeg * kick(WeaponRng()));
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
            OwnerCharacter->SetControlPitch(OwnerCharacter->GetControlPitch() +
                                            Config.RecoilPitchDeg * kick(WeaponRng()));
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
        if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World ? World->GetGameMode() : nullptr)) {
            auto* gs = gm->GetGameState();
            if (gs && gs->GetMatchState() != ELeonTournamentMatchState::Playing)
                return false;
        }
        if (!AWeaponBase::ServerFire())
            return false;
        bFiring = true;

        if (Config.FireMode == ELeonTournamentFireMode::Projectile)
            return FireProjectile();
        if (Config.FireMode == ELeonTournamentFireMode::Flame)
            return FireFlame();
        return FireHitscan();
    }

    bool ALeonTournamentWeapon::StartReload() {
        if (OwnerCharacter && OwnerCharacter->GetHealthComponent() && OwnerCharacter->GetHealthComponent()->IsDead())
            return false;
        if (!AWeaponBase::StartReload())
            return false;
        UGameplayStatics::PlaySound2D("/Game/Audio/SFX_RifleReload", 0.7f);
        return true;
    }

    void ALeonTournamentWeapon::ResetMagazine() {
        AWeaponBase::ResetMagazine();
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
        AWeaponBase::Tick(DeltaSeconds);
        if (CurrentSpreadDeg > Config.BaseSpreadDeg)
            CurrentSpreadDeg =
                std::max(Config.BaseSpreadDeg, CurrentSpreadDeg - Config.SpreadRecoveryPerSec * DeltaSeconds);

        const bool bAuthority = !IsClientNetMode();
        bFiring = false;
        if (bAuthority && bFireHeld && CanFire())
            ServerFire();

        UpdateFirstPersonVisual();
    }

} // namespace Leon
