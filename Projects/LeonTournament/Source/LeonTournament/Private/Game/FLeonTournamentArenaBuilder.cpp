#include "FLeonTournamentArenaBuilder.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMeshPrimitives.hpp"

namespace Leon {

    namespace {
        const char* ArenaMaterialPath(ELeonTournamentArenaSurface InSurface) {
            switch (InSurface) {
            case ELeonTournamentArenaSurface::Floor:
                return "/Game/Materials/M_LabFloor.lmat";
            case ELeonTournamentArenaSurface::Wall:
                return "/Game/Materials/M_LabWall.lmat";
            case ELeonTournamentArenaSurface::Metal:
                return "/Game/Materials/M_ArenaMetal.lmat";
            case ELeonTournamentArenaSurface::Accent:
                return "/Game/Materials/M_ArenaAccent.lmat";
            case ELeonTournamentArenaSurface::Ceiling:
                return "/Game/Materials/M_LabProp.lmat";
            case ELeonTournamentArenaSurface::Prop:
            default:
                return "/Game/Materials/M_LabProp.lmat";
            }
        }
    } // namespace

    AActor* FLeonTournamentArenaBuilder::SpawnBox(UWorld* InWorld, const std::string& InName,
                                                  const glm::vec3& InLocation, const glm::vec3& InScale,
                                                  ELeonTournamentArenaSurface InSurface, const glm::vec3& InTint,
                                                  float InUvTile) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InLocation);
        actor->SetActorScale(InScale);
        auto box = actor->AddActorComponent<UBoxComponent>("Box");
        box->SetBoxExtent(glm::vec3(0.5f));
        box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
        box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if (FApplication::HasInstance()) {
            auto va = FMeshPrimitives::CreateCube(1.0f);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (va && shader) {
                auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
                mesh.MeshType = "Cube";
                mesh.MeshSize = 1.0f;
                mesh.Mobility = EComponentMobility::Static;
                mesh.LightmapResolution = 64;
                mesh.bCastShadows = true;
                mesh.bReceiveShadows = true;
                mesh.bVisibleInReflection = true;
                if (auto mat = UAssetManager::GetMaterialInstance(ArenaMaterialPath(InSurface))) {
                    mat->SetAlbedoColor(InTint);
                    if (InUvTile > 0.0f)
                        mat->SetUVTiling({InUvTile, InUvTile});
                    if (InSurface == ELeonTournamentArenaSurface::Floor ||
                        InSurface == ELeonTournamentArenaSurface::Metal)
                        mat->SetUsePlanarReflection(true);
                    actor->AddComponent<FMaterialComponent>(mat);
                } else if (auto parent = UAssetManager::GetDefaultMaterial()) {
                    auto inst = parent->CreateInstance(InName + "Mat");
                    inst->SetAlbedoColor(InTint);
                    actor->AddComponent<FMaterialComponent>(inst);
                }
            }
        }
        return actor;
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSimpleBox(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InLocation, const glm::vec3& InScale,
                                                        const glm::vec3& InColor) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InLocation);
        actor->SetActorScale(InScale);
        auto box = actor->AddActorComponent<UBoxComponent>("Box");
        box->SetBoxExtent(glm::vec3(0.5f));
        box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
        box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if (FApplication::HasInstance()) {
            auto va = FMeshPrimitives::CreateCube(1.0f);
            auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
            if (va && shader) {
                auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
                mesh.MeshType = "Cube";
                mesh.MeshSize = 1.0f;
                mesh.Mobility = EComponentMobility::Static;
                if (auto parent = UAssetManager::GetDefaultMaterial()) {
                    auto inst = parent->CreateInstance(InName + "Mat");
                    inst->SetAlbedoColor(InColor);
                    actor->AddComponent<FMaterialComponent>(inst);
                }
            }
        }
        return actor;
    }

    AActor* FLeonTournamentArenaBuilder::SpawnPointLight(UWorld* InWorld, const std::string& InName,
                                                         const glm::vec3& InPos, const glm::vec3& InColor,
                                                         float InIntensity, float InRadius,
                                                         ELightMobility InMobility) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InPos);
        FPointLightComponent light;
        light.bEnabled = true;
        light.Mobility = InMobility;
        light.Light.Position = InPos;
        light.Light.Color = InColor;
        light.Light.Intensity = InIntensity;
        light.Light.Radius = InRadius;
        actor->AddComponent<FPointLightComponent>(light);
        return actor;
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSpotLight(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InPos, const glm::vec3& InDir,
                                                        const glm::vec3& InColor, float InIntensity, float InRadius,
                                                        float InInnerDeg, float InOuterDeg,
                                                        ELightMobility InMobility) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InPos);
        FSpotLightComponent light;
        light.bEnabled = true;
        light.Mobility = InMobility;
        light.Light.Position = InPos;
        light.Light.Direction = glm::normalize(InDir);
        light.Light.Color = InColor;
        light.Light.Intensity = InIntensity;
        light.Light.Radius = InRadius;
        light.Light.CutOff = InInnerDeg;
        light.Light.OuterCutOff = InOuterDeg;
        actor->AddComponent<FSpotLightComponent>(light);
        return actor;
    }

} // namespace Leon
