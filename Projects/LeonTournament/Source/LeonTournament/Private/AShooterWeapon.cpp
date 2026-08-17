#include "AShooterWeapon.hpp"
#include "AShooterCharacter.hpp"
#include "AShooterGameMode.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Gameplay/UHealthComponent.hpp"
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
            auto va = FMeshPrimitives::CreateCylinder(0.04f, 0.04f, 0.55f, 12, true);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (!va || !shader)
                return;
            auto& mesh = InActor.AddComponent<FMeshComponent>(va, shader);
            mesh.MeshType = "Cylinder";
            mesh.MeshRadius = 0.04f;
            mesh.MeshHeight = 0.55f;
            mesh.Mobility = EComponentMobility::Movable;
            if (auto parent = UAssetManager::GetDefaultMaterial()) {
                auto inst = parent->CreateInstance("WeaponMat");
                inst->SetAlbedoColor({0.12f, 0.12f, 0.14f});
                InActor.AddComponent<FMaterialComponent>(inst);
            }
        }
    } // namespace

    AShooterWeapon::AShooterWeapon(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AShooterWeapon");
        CurrentAmmo = Config.MagazineSize;
    }

    AShooterRifle::AShooterRifle(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AShooterWeapon(InHandle, InWorld, InName) {
        SetClass("AShooterRifle");
    }

    void AShooterWeapon::SetConfig(const FShooterRifleConfig& InConfig) {
        Config = InConfig;
        CurrentAmmo = Config.MagazineSize;
    }

    void AShooterWeapon::AttachVisual() {
        EnsureWeaponMesh(*this);
        bVisualReady = HasComponent<FMeshComponent>();
    }

    bool AShooterWeapon::CanFire() const {
        if (!OwnerCharacter || bReloading || CurrentAmmo <= 0 || FireCooldown > 0.0f)
            return false;
        if (auto health = OwnerCharacter->GetHealthComponent()) {
            if (health->IsDead())
                return false;
        }
        if (OwnerCharacter->IsSprinting())
            return false;
        return true;
    }

    bool AShooterWeapon::ServerFire() {
        if (!World || World->GetNetMode() == ENetMode::Client)
            return false;
        if (!CanFire() || !OwnerCharacter)
            return false;
        if (auto* gm = dynamic_cast<AShooterGameMode*>(World->GetGameMode())) {
            auto* gs = gm->GetShooterGameState();
            if (gs && gs->GetMatchState() != EShooterMatchState::Playing)
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
        if (bHit && hit.bBlockingHit && hit.Actor) {
            auto* target = dynamic_cast<AShooterCharacter*>(hit.Actor);
            if (target && target != OwnerCharacter) {
                FDamageInfo info;
                info.DamageAmount = Config.Damage;
                info.DamageType = EDamageType::Point;
                info.Instigator = OwnerCharacter;
                info.Causer = this;
                info.HitActor = target;
                info.HitLocation = hit.Location;
                info.HitNormal = hit.Normal;
                if (auto* gm = dynamic_cast<AShooterGameMode*>(World->GetGameMode())) {
                    if (gm->ApplyAuthoritativeDamage(*OwnerCharacter, *target, info)) {
                        bKill = target->GetHealthComponent() && target->GetHealthComponent()->IsDead();
                        OwnerCharacter->PulseHitConfirm(bKill);
                    }
                }
            }
        }
        return true;
    }

    bool AShooterWeapon::StartReload() {
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

    void AShooterWeapon::CancelReload() {
        bReloading = false;
        ReloadRemaining = 0.0f;
    }

    void AShooterWeapon::ResetMagazine() {
        CancelReload();
        CurrentAmmo = Config.MagazineSize;
        FireCooldown = 0.0f;
        bFiring = false;
        bFireHeld = false;
    }

    void AShooterWeapon::ApplyReplicatedState(int32_t InAmmo, bool bInReloading) {
        CurrentAmmo = InAmmo;
        bReloading = bInReloading;
        if (!bReloading)
            ReloadRemaining = 0.0f;
    }

    void AShooterWeapon::Tick(float DeltaSeconds) {
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

        if (OwnerCharacter && !OwnerCharacter->IsPendingKill()) {
            glm::vec3 origin, dir;
            OwnerCharacter->GetAimRay(origin, dir);
            glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
            if (glm::length(right) < 1e-4f)
                right = glm::vec3(1, 0, 0);
            glm::vec3 loc = OwnerCharacter->GetActorLocation() + dir * 0.45f + right * 0.28f + glm::vec3(0, -0.15f, 0);
            SetActorLocation(loc);
            SetActorRotation(Leon::EulerLookingAlong(dir));
        }
    }

} // namespace Leon
