#include "ALeonTournamentWeapon.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "ALeonTournamentGameMode.hpp"
#include "Gameplay/AGameModeBase.hpp"
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

namespace Leon {

    namespace {
        void EnsureWeaponMesh(AActor& InActor) {
            if (InActor.HasComponent<FMeshComponent>())
                return;
            if (!FApplication::HasInstance())
                return;
            auto va = FMeshPrimitives::CreateCylinder(0.035f, 0.025f, 0.42f, 10, true);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!va || !shader)
                return;
            auto& mesh = InActor.AddComponent<FMeshComponent>(va, shader);
            mesh.MeshType = "Cylinder";
            mesh.MeshRadius = 0.035f;
            mesh.MeshHeight = 0.42f;
            mesh.Mobility = EComponentMobility::Movable;
            mesh.bCastShadows = false;
            if (auto parent = UAssetManager::GetDefaultMaterial()) {
                auto inst = parent->CreateInstance("WeaponMat");
                inst->SetAlbedoColor({0.12f, 0.12f, 0.14f});
                InActor.AddComponent<FMaterialComponent>(inst);
            }
        }
    } // namespace

    ALeonTournamentWeapon::ALeonTournamentWeapon(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentWeapon");
        CurrentAmmo = Config.MagazineSize;
    }

    ALeonTournamentRifle::ALeonTournamentRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : ALeonTournamentWeapon(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentRifle");
    }

    void ALeonTournamentWeapon::SetConfig(const FLeonTournamentRifleConfig& InConfig) {
        Config = InConfig;
        CurrentAmmo = Config.MagazineSize;
    }

    void ALeonTournamentWeapon::AttachVisual() {
        EnsureWeaponMesh(*this);
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

    void ALeonTournamentWeapon::SpawnFireEffects(const glm::vec3& InMuzzle, const glm::vec3& InTraceEnd, bool bHitWorld,
                                                 bool bHitCharacter) {
        LastVfxSpawnCount = 0;
        if (!World)
            return;

        FParticleEmitterSettings muzzle;
        muzzle.Kind = EParticleKind::SpriteBurst;
        muzzle.BurstCount = 10;
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
        tracer.Lifetime = 0.06f;
        tracer.Color = {1.0f, 0.85f, 0.35f, 0.85f};
        tracer.ColorEnd = {1.0f, 0.55f, 0.15f, 0.0f};
        tracer.BeamEnd = InTraceEnd;
        tracer.BeamThickness = 0.018f;
        if (UGameplayStatics::SpawnEmitterAtLocation(World, tracer, InMuzzle))
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

        glm::vec3 origin, dir;
        OwnerCharacter->GetAimRay(origin, dir);
        UWorld::FHitResult hit;
        const glm::vec3 end = origin + dir * Config.Range;
        const bool bHit = World->LineTraceByChannel(origin, end, ECollisionChannel::Visibility, OwnerCharacter, hit);
        if (FDebugRenderer::IsTraceCaptureEnabled()) {
            FDebugRenderer::RecordLineTrace(origin, end, bHit && hit.bBlockingHit, hit.Location, hit.Normal,
                                            static_cast<uint8_t>(ECollisionChannel::Visibility));
        }
        bool bKill = false;
        bool bDamaged = false;
        glm::vec3 traceEnd = end;
        bool bHitWorld = false;
        bool bHitCharacter = false;
        if (bHit && hit.bBlockingHit) {
            traceEnd = hit.Location;
            auto* target = dynamic_cast<ALeonTournamentCharacter*>(hit.Actor);
            if (target && target != OwnerCharacter) {
                bHitCharacter = true;
                FDamageInfo info;
                info.DamageAmount = Config.Damage;
                info.DamageType = EDamageType::Point;
                info.Instigator = OwnerCharacter;
                info.Causer = this;
                info.HitActor = target;
                info.HitLocation = hit.Location;
                info.HitNormal = hit.Normal;
                if (auto* gm = dynamic_cast<ALeonTournamentGameMode*>(World->GetGameMode())) {
                    if (gm->ApplyAuthoritativeDamage(*OwnerCharacter, *target, info)) {
                        bDamaged = true;
                        bKill = target->GetHealthComponent() && target->GetHealthComponent()->IsDead();
                        OwnerCharacter->PulseHitConfirm(bKill);
                    }
                }
            } else {
                bHitWorld = true;
            }
        }
        (void)bDamaged;
        SpawnFireEffects(GetMuzzleLocation(), traceEnd, bHitWorld, bHitCharacter);
        return true;
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
        bFireHeld = false;
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
        glm::vec3 origin, dir;
        OwnerCharacter->GetAimRay(origin, dir);
        glm::vec3 right, up;
        StableViewBasis(dir, right, up);
        const float kick = bFiring ? 0.03f : 0.0f;
        glm::vec3 loc = origin + dir * (0.38f - kick) + right * 0.18f + up * (-0.14f);
        SetActorLocation(loc);
        SetActorRotation(Leon::EulerLookingAlong(dir));
        SetVisualHidden(bVisualHidden);
    }

    void ALeonTournamentWeapon::Tick(float DeltaSeconds) {
        if (FireCooldown > 0.0f)
            FireCooldown = std::max(0.0f, FireCooldown - DeltaSeconds);

        const bool bAuthority = !World || World->GetNetMode() != ENetMode::Client;
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
