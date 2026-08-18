#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "FLeonTournamentWeaponPresets.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "Core/FApplication.hpp"

namespace Leon {

    ALeonTournamentPickup::ALeonTournamentPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APickup(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPickup");
    }

    void ALeonTournamentPickup::SetPickupActive(bool bInActive) {
        APickup::SetPickupActive(bInActive);
        if (HasComponent<FMeshComponent>())
            GetComponent<FMeshComponent>().bVisible = bInActive;
    }

    bool ALeonTournamentPickup::CanBePickedUp(APawn* InPawn) const {
        if (!APickup::CanBePickedUp(InPawn))
            return false;
        auto* ch = dynamic_cast<ALeonTournamentCharacter*>(InPawn);
        if (!ch || ch->IsDeadFrozen())
            return false;
        if (auto health = ch->GetHealthComponent(); health && health->IsDead())
            return false;
        return true;
    }

    bool ALeonTournamentPickup::GiveTo(APawn* InPawn) {
        auto* ch = dynamic_cast<ALeonTournamentCharacter*>(InPawn);
        if (!ch || !TryGiveTo(*ch))
            return false;
        UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_UIClick", GetActorLocation(), 0.85f, 2500.0f);
        return true;
    }

    ALeonTournamentWeaponPickup::ALeonTournamentWeaponPickup(entt::entity InHandle, UWorld* InWorld,
                                                             const std::string& InName)
        : ALeonTournamentPickup(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentWeaponPickup");
        SetRespawnDelay(15.0f);
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
        mesh.bVisible = IsPickupActive();
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
        SetRespawnDelay(15.0f);
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
        mesh.bVisible = IsPickupActive();
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
        : ALaunchPad(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentJumpPad");
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

    void ALeonTournamentJumpPad::OnLaunched(ACharacter* InCharacter) {
        (void)InCharacter;
        UGameplayStatics::PlaySoundAtLocation("/Game/Audio/SFX_UIClick", GetActorLocation(), 0.7f, 2200.0f);
    }

} // namespace Leon
