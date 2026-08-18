#include "ASandboxDemoPickup.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Gameplay/UGameplayStatics.hpp"
#include "Renderer/FMeshPrimitives.hpp"

namespace Leon {

    namespace {
        void ConfigurePickupMesh(FMeshComponent& InMesh, bool bActive) {
            InMesh.Mobility = EComponentMobility::Movable;
            InMesh.bCastShadows = false;
            InMesh.bVisible = bActive;
            InMesh.bVisibleInReflection = false;
        }
    } // namespace

    ASandboxDemoPickup::ASandboxDemoPickup(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APickup(InHandle, InWorld, InName) {
        SetClass("ASandboxDemoPickup");
        SetRespawnDelay(8.0f);
        SetPickupRadius(1.4f);
    }

    void ASandboxDemoPickup::SetPickupActive(bool bInActive) {
        APickup::SetPickupActive(bInActive);
        if (HasComponent<FMeshComponent>()) {
            auto& mesh = GetComponent<FMeshComponent>();
            mesh.bVisible = bInActive;
            mesh.bVisibleInReflection = false;
        }
    }

    bool ASandboxDemoPickup::GiveTo(APawn* InPawn) {
        if (!InPawn)
            return false;
        PrintString("Collected sandbox pickup", 2.5f);
        return true;
    }

    void ASandboxDemoPickup::BuildVisual() {
        if (HasComponent<FMeshComponent>() || !FApplication::HasInstance())
            return;
        auto va = FMeshPrimitives::CreatePyramid(0.42f, 0.72f, 0.42f);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return;
        auto& mesh = AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Pyramid";
        mesh.MeshWidth = 0.42f;
        mesh.MeshHeight = 0.72f;
        mesh.MeshDepth = 0.42f;
        ConfigurePickupMesh(mesh, IsPickupActive());
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance("SandboxDemoPickupMat");
            inst->SetAlbedoColor(VisualColor);
            inst->SetMetallic(0.42f);
            inst->SetRoughness(0.28f);
            inst->SetEmissiveColor(VisualColor);
            inst->SetEmissiveIntensity(2.2f);
            AddComponent<FMaterialComponent>(inst);
        }
    }

} // namespace Leon
