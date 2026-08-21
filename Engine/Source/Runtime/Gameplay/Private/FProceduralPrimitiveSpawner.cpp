#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Assets/FEngineBuiltins.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FApplication.hpp"
#include "Core/FLog.hpp"
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
        // Collision uses unit box * scale; visual Box meshes bake size into the VA (actor scale = 1).
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
                                                      float InMetersPerUv, bool bUsePlanarReflection,
                                                      bool bVisibleInReflection) {
        AActor* actor = SpawnStaticBox(InWorld, InName, InLocation, InScale);
        if (!actor)
            return nullptr;
        if (!FApplication::HasInstance())
            return actor;

        const float mpu = InMetersPerUv > 0.0f ? InMetersPerUv : 1.0f;
        auto va = FMeshPrimitives::CreateBox(InScale.x, InScale.y, InScale.z, mpu);
        auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
        if (!va || !shader)
            return actor;

        // Size is in the mesh; keep collision via previous scale, then clear visual scale.
        actor->SetActorScale({1.0f, 1.0f, 1.0f});
        if (auto box = actor->FindActorComponent<UBoxComponent>())
            box->SetBoxExtent(InScale * 0.5f);

        auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = "Box";
        mesh.MeshSize = 1.0f;
        mesh.MeshWidth = InScale.x;
        mesh.MeshHeight = InScale.y;
        mesh.MeshDepth = InScale.z;
        mesh.MeshMetersPerUv = mpu;
        mesh.Mobility = EComponentMobility::Static;
        mesh.LightmapResolution = 64;
        mesh.bCastShadows = true;
        mesh.bReceiveShadows = true;
        mesh.bVisibleInReflection = bVisibleInReflection;

        if (!InMaterialPath.empty()) {
            if (auto mat = UAssetManager::GetMaterialInstance(InMaterialPath)) {
                mat->SetAlbedoColor(InColor);
                // UVs already encode meters / InMetersPerUv — keep material tiling at 1.
                mat->SetUVTiling({1.0f, 1.0f});
                mat->SetUsePlanarReflection(bUsePlanarReflection);
                actor->AddComponent<FMaterialComponent>(mat, InMaterialPath);
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

    AActor* FProceduralPrimitiveSpawner::SpawnShape(UWorld* InWorld, const std::string& InShapeType,
                                                    const std::string& InName, const glm::vec3& InLocation) {
        if (!InWorld)
            return nullptr;

        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        if (!actor)
            return nullptr;

        actor->SetActorLocation(InLocation);
        actor->SetActorScale({1.0f, 1.0f, 1.0f});

        const char* meshPath = nullptr;
        if (InShapeType == "Cube" || InShapeType == "Box") {
            meshPath = FEngineBuiltins::kMeshCube;
            if (auto box = actor->AddActorComponent<UBoxComponent>("Box")) {
                box->SetBoxExtent(glm::vec3(0.5f));
                box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Sphere") {
            meshPath = FEngineBuiltins::kMeshSphere;
            if (auto sphere = actor->AddActorComponent<USphereComponent>("Sphere")) {
                sphere->SetSphereRadius(0.5f);
                sphere->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Cylinder") {
            meshPath = FEngineBuiltins::kMeshCylinder;
            // Capsule approximates a standing cylinder (r=0.5, height=1 → half-height=1).
            if (auto capsule = actor->AddActorComponent<UCapsuleComponent>("Capsule")) {
                capsule->SetCapsuleSize(0.5f, 1.0f);
                capsule->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Plane") {
            meshPath = FEngineBuiltins::kMeshPlane;
            if (auto box = actor->AddActorComponent<UBoxComponent>("Box")) {
                box->SetBoxExtent(glm::vec3(1.0f, 0.05f, 1.0f));
                box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else {
            return actor;
        }

        if (!FApplication::HasInstance() || !meshPath)
            return actor;

        auto staticMesh = UAssetManager::GetStaticMesh(meshPath);
        if (!staticMesh) {
            LE_CORE_WARN("FProceduralPrimitiveSpawner: Missing built-in mesh \"{}\", falling back to procedural VA",
                         meshPath);
            TRef<FVertexArray> va;
            if (InShapeType == "Cube" || InShapeType == "Box")
                va = FMeshPrimitives::CreateBox(1.0f, 1.0f, 1.0f, 1.0f);
            else if (InShapeType == "Sphere")
                va = FMeshPrimitives::CreateSphere(0.5f, 32, 16);
            else if (InShapeType == "Cylinder")
                va = FMeshPrimitives::CreateCylinder(0.5f, 0.5f, 1.0f, 32);
            else if (InShapeType == "Plane")
                va = FMeshPrimitives::CreatePlane(2.0f, 2.0f, 1, 1);

            auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
            if (va && shader) {
                auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
                mesh.MeshType = InShapeType;
                mesh.Mobility = EComponentMobility::Static;
                mesh.LightmapResolution = 64;
                mesh.bCastShadows = true;
                mesh.bReceiveShadows = true;
                if (auto matInst = UAssetManager::GetWorldGridMaterialInstance()) {
                    actor->AddComponent<FMaterialComponent>(matInst, FEngineBuiltins::kWorldGridMaterial);
                }
            }
            return actor;
        }

        auto& smc = actor->AddComponent<FStaticMeshComponent>(staticMesh, meshPath);
        smc.Mobility = EComponentMobility::Static;
        smc.LightmapResolution = 64;
        smc.bCastShadows = true;
        smc.bReceiveShadows = true;
        smc.Shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");

        // Slot default is M_WorldGrid; keep an explicit material component for Details/serialization.
        if (auto matInst = UAssetManager::GetWorldGridMaterialInstance()) {
            actor->AddComponent<FMaterialComponent>(matInst, FEngineBuiltins::kWorldGridMaterial);
            smc.MaterialOverrides = {matInst};
            smc.MaterialOverridePaths = {FEngineBuiltins::kWorldGridMaterial};
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
        const glm::vec3 dir = glm::length(InDir) > 1e-5f ? glm::normalize(InDir) : SpotLightLocalAimAxis();
        actor->SetActorRotation(SpotLightEulerFromWorldDirection(dir));
        FSpotLightComponent light;
        light.bEnabled = true;
        light.Mobility = InMobility;
        SyncSpotLightFromTransform(light.Light, InPos, actor->GetActorRotation());
        light.Light.Color = InColor;
        light.Light.Intensity = InIntensity;
        light.Light.Radius = InRadius;
        light.Light.CutOff = InInnerDeg;
        light.Light.OuterCutOff = InOuterDeg;
        actor->AddComponent<FSpotLightComponent>(light);
        return actor;
    }

} // namespace Leon
