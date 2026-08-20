#include "ALeonTournamentPickup.hpp"
#include "ALeonTournamentCharacter.hpp"
#include "FLeonTournamentWeaponPresets.hpp"
#include "Gameplay/UHealthComponent.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Assets/UAssetManager.hpp"
#include "Renderer/FMaterialInstance.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "Engine/Components.hpp"
#include "Core/FApplication.hpp"

namespace Leon {

    namespace {
        void ConfigurePickupMesh(FMeshComponent& InMesh, bool bActive) {
            InMesh.Mobility = EComponentMobility::Movable;
            InMesh.bCastShadows = false;
            InMesh.bVisible = bActive;
            InMesh.bVisibleInReflection = false;
        }

        void StylePickupMaterial(FMaterialInstance& InMat, const glm::vec3& InColor) {
            InMat.SetAlbedoColor(InColor);
            InMat.SetMetallic(0.42f);
            InMat.SetRoughness(0.28f);
            InMat.SetEmissiveColor(InColor);
            InMat.SetEmissiveIntensity(2.2f);
        }
    } // namespace

    ALeonTournamentPickup::ALeonTournamentPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APickup(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPickup");
    }

    void ALeonTournamentPickup::SetPickupActive(bool bInActive) {
        APickup::SetPickupActive(bInActive);
        if (HasComponent<FMeshComponent>()) {
            auto& mesh = GetComponent<FMeshComponent>();
            mesh.bVisible = bInActive;
            mesh.bVisibleInReflection = false;
        }
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
            if (auto mat = GetComponent<FMaterialComponent>().MaterialInstance) {
                mat->SetAlbedoColor(VisualColor);
                mat->SetEmissiveColor(VisualColor);
            }
        }
    }

    void ALeonTournamentWeaponPickup::BuildVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        auto va = FMeshPrimitives::CreatePyramid(0.42f, 0.72f, 0.42f);
        auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Pyramid";
        mesh.MeshWidth = 0.42f;
        mesh.MeshHeight = 0.72f;
        mesh.MeshDepth = 0.42f;
        ConfigurePickupMesh(mesh, IsPickupActive());
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("WeaponPickupMat");
            StylePickupMaterial(*inst, VisualColor);
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
        auto va = FMeshPrimitives::CreateCube(0.42f);
        auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Box";
        mesh.MeshSize = 0.42f;
        ConfigurePickupMesh(mesh, IsPickupActive());
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("HealthPickupMat");
            StylePickupMaterial(*inst, VisualColor);
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
        auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
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
