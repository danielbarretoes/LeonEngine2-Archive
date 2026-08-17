#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Core/FApplication.hpp"

#include <cmath>

namespace Leon {

    ALeonTournamentPickup::ALeonTournamentPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPickup");
    }

    void ALeonTournamentPickup::BeginPlay() {
        AActor::BeginPlay();
        HomeLocation = GetActorLocation();
        BuildVisual();
        SetPickupActive(true);
    }

    void ALeonTournamentPickup::SetPickupActive(bool bInActive) {
        bActive = bInActive;
        if (HasComponent<FMeshComponent>())
            GetComponent<FMeshComponent>().bVisible = bInActive;
        if (bInActive)
            SetActorLocation(HomeLocation);
    }

    void ALeonTournamentPickup::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (!World)
            return;

        if (!bActive) {
            RespawnRemaining -= DeltaSeconds;
            if (RespawnRemaining <= 0.0f)
                SetPickupActive(true);
            return;
        }

        BobPhase += DeltaSeconds * 2.4f;
        glm::vec3 loc = HomeLocation;
        loc.y += std::sin(BobPhase) * 0.12f;
        SetActorLocation(loc);
        SetActorRotation({0.0f, BobPhase * 45.0f, 0.0f});

        if (World->GetNetMode() == ENetMode::Client)
            return;

        for (const auto& actorRef : World->GetAllActors()) {
            auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actorRef.get());
            if (!ch || ch->IsPendingKill() || ch->IsDeadFrozen())
                continue;
            if (auto health = ch->GetHealthComponent(); health && health->IsDead())
                continue;
            const float dist = glm::length(ch->GetActorLocation() - GetActorLocation());
            if (dist > PickupRadius)
                continue;
            if (!TryGiveTo(*ch))
                continue;
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_UIClick", GetActorLocation(), 0.85f, 2500.0f);
            SetPickupActive(false);
            RespawnRemaining = RespawnDelay;
            break;
        }
    }

    ALeonTournamentWeaponPickup::ALeonTournamentWeaponPickup(entt::entity InHandle, UWorld* InWorld,
                                                             const std::string& InName)
        : ALeonTournamentPickup(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentWeaponPickup");
        RespawnDelay = 15.0f;
        SetWeaponId(ELeonTournamentWeaponId::Shotgun);
    }

    void ALeonTournamentWeaponPickup::SetWeaponId(ELeonTournamentWeaponId InId) {
        WeaponId = InId;
        const auto cfg = LeonTournamentWeaponPreset(InId);
        VisualColor = cfg.VisualColor;
        if (HasComponent<FMaterialComponent>()) {
            if (auto mat = GetComponent<FMaterialComponent>().MaterialInstance)
                mat->SetAlbedoColor(VisualColor);
        }
    }

    void ALeonTournamentWeaponPickup::BuildVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        auto va = FMeshPrimitives::CreateCylinder(0.18f, 0.12f, 0.55f, 12, true);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Cylinder";
        mesh.MeshRadius = 0.18f;
        mesh.MeshHeight = 0.55f;
        mesh.Mobility = EComponentMobility::Movable;
        mesh.bCastShadows = false;
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("WeaponPickupMat");
            inst->SetAlbedoColor(VisualColor);
            AddComponent<FMaterialComponent>(inst);
        }
    }

    bool ALeonTournamentWeaponPickup::TryGiveTo(ALeonTournamentCharacter& InCharacter) {
        return InCharacter.GiveWeapon(WeaponId);
    }

    ALeonTournamentHealthPickup::ALeonTournamentHealthPickup(entt::entity InHandle, UWorld* InWorld,
                                                             const std::string& InName)
        : ALeonTournamentPickup(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentHealthPickup");
        RespawnDelay = 15.0f;
        VisualColor = {0.15f, 0.95f, 0.35f};
    }

    void ALeonTournamentHealthPickup::BuildVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        auto va = FMeshPrimitives::CreateCube(0.45f);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Box";
        mesh.Mobility = EComponentMobility::Movable;
        mesh.bCastShadows = false;
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("HealthPickupMat");
            inst->SetAlbedoColor(VisualColor);
            AddComponent<FMaterialComponent>(inst);
        }
    }

    bool ALeonTournamentHealthPickup::TryGiveTo(ALeonTournamentCharacter& InCharacter) {
        auto health = InCharacter.GetHealthComponent();
        if (!health || health->IsDead())
            return false;
        if (health->GetHealth() >= health->GetMaxHealth() - 0.01f)
            return false;
        health->Heal(health->GetMaxHealth());
        return true;
    }

    ALeonTournamentJumpPad::ALeonTournamentJumpPad(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentJumpPad");
    }

    void ALeonTournamentJumpPad::BeginPlay() {
        AActor::BeginPlay();
        BuildVisual();
    }

    void ALeonTournamentJumpPad::BuildVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        auto va = FMeshPrimitives::CreateCube(1.6f);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Box";
        mesh.MeshSize = 1.6f;
        mesh.Mobility = EComponentMobility::Movable;
        mesh.bCastShadows = false;
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("JumpPadMat");
            inst->SetAlbedoColor({0.2f, 0.55f, 0.95f});
            AddComponent<FMaterialComponent>(inst);
        }
    }

    void ALeonTournamentJumpPad::Tick(float DeltaSeconds) {
        AActor::Tick(DeltaSeconds);
        if (!World || World->GetNetMode() == ENetMode::Client)
            return;

        for (auto it = RecentTriggers.begin(); it != RecentTriggers.end();) {
            it->second -= DeltaSeconds;
            if (it->second <= 0.0f)
                it = RecentTriggers.erase(it);
            else
                ++it;
        }

        for (const auto& actorRef : World->GetAllActors()) {
            auto* ch = dynamic_cast<ALeonTournamentCharacter*>(actorRef.get());
            if (!ch || ch->IsPendingKill() || ch->IsDeadFrozen())
                continue;
            if (RecentTriggers.count(ch))
                continue;
            const float dist = glm::length(ch->GetActorLocation() - GetActorLocation());
            if (dist > TriggerRadius)
                continue;
            ch->ApplyLaunchVelocity(PadVelocity);
            RecentTriggers[ch] = Cooldown;
            UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_UIClick", GetActorLocation(), 0.7f, 2200.0f);
        }
    }

} // namespace Leon
