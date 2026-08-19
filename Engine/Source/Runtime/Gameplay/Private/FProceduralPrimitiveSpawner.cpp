#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"
#include "Renderer/FMaterial.hpp"
#include "Renderer/FMeshPrimitives.hpp"

namespace Leon {

    AActor* FProceduralPrimitiveSpawner::SpawnStaticBox(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InLocation, const glm::vec3& InScale) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InLocation);
        actor->SetActorScale(InScale);
        auto box = actor->AddActorComponent<UBoxComponent>("Box");
        box->SetBoxExtent(glm::vec3(0.5f));
        box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
        box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        return actor;
    }

    AActor* FProceduralPrimitiveSpawner::SpawnMeshBox(UWorld* InWorld, const std::string& InName,
                                                      const glm::vec3& InLocation, const glm::vec3& InScale,
                                                      const glm::vec3& InColor, const std::string& InMaterialPath,
                                                      float InUvTile, bool bUsePlanarReflection,
                                                      bool bVisibleInReflection) {
        AActor* actor = SpawnStaticBox(InWorld, InName, InLocation, InScale);
        if (!actor)
            return nullptr;
        if (!FApplication::HasInstance())
            return actor;

        auto va = FMeshPrimitives::CreateCube(1.0f);
        auto shader = UAssetManager::GetShader("Engine/Assets/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return actor;

        auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Cube";
        mesh.MeshSize = 1.0f;
        mesh.Mobility = EComponentMobility::Static;
        mesh.LightmapResolution = 64;
        mesh.bCastShadows = true;
        mesh.bReceiveShadows = true;
        mesh.bVisibleInReflection = bVisibleInReflection;

        if (!InMaterialPath.empty()) {
            if (auto mat = UAssetManager::GetMaterialInstance(InMaterialPath)) {
                mat->SetAlbedoColor(InColor);
                if (InUvTile > 0.0f)
                    mat->SetUVTiling({InUvTile, InUvTile});
                mat->SetUsePlanarReflection(bUsePlanarReflection);
                actor->AddComponent<FMaterialComponent>(mat);
                return actor;
            }
        }
        if (auto parent = UAssetManager::GetDefaultMaterial()) {
            auto inst = parent->CreateInstance(InName + "Mat");
            inst->SetAlbedoColor(InColor);
            actor->AddComponent<FMaterialComponent>(inst);
        }
        return actor;
    }

    AActor* FProceduralPrimitiveSpawner::SpawnPointLight(UWorld* InWorld, const std::string& InName,
                                                         const glm::vec3& InPos, const glm::vec3& InColor,
                                                         float InIntensity, float InRadius, ELightMobility InMobility) {
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

    AActor* FProceduralPrimitiveSpawner::SpawnSpotLight(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InPos, const glm::vec3& InDir,
                                                        const glm::vec3& InColor, float InIntensity, float InRadius,
                                                        float InInnerDeg, float InOuterDeg, ELightMobility InMobility) {
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
