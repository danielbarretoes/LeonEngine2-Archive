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

        TRef<FVertexArray> va = nullptr;
        std::string meshType = InShapeType;
        float meshSize = 1.0f;
        float meshWidth = 1.0f;
        float meshHeight = 1.0f;
        float meshDepth = 1.0f;
        float meshRadius = 0.5f;
        float meshMetersPerUv = 1.0f;
        unsigned int subdivX = 32;
        unsigned int subdivZ = 16;

        if (InShapeType == "Cube" || InShapeType == "Box") {
            meshType = "Box";
            meshWidth = meshHeight = meshDepth = 1.0f;
            va = FMeshPrimitives::CreateBox(meshWidth, meshHeight, meshDepth, meshMetersPerUv);
            if (auto box = actor->AddActorComponent<UBoxComponent>("Box")) {
                box->SetBoxExtent(glm::vec3(0.5f));
                box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Sphere") {
            meshType = "Sphere";
            meshRadius = 0.5f;
            va = FMeshPrimitives::CreateSphere(meshRadius, subdivX, subdivZ);
            if (auto sphere = actor->AddActorComponent<USphereComponent>("Sphere")) {
                sphere->SetSphereRadius(meshRadius);
                sphere->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Cylinder") {
            meshType = "Cylinder";
            meshRadius = 0.5f;
            meshHeight = 1.0f;
            va = FMeshPrimitives::CreateCylinder(meshRadius, meshRadius, meshHeight, subdivX, true);
            if (auto box = actor->AddActorComponent<UBoxComponent>("Box")) {
                box->SetBoxExtent(glm::vec3(meshRadius, meshHeight * 0.5f, meshRadius));
                box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else if (InShapeType == "Plane") {
            meshType = "Plane";
            meshWidth = 2.0f;
            meshDepth = 2.0f;
            subdivX = 1;
            subdivZ = 1;
            va = FMeshPrimitives::CreatePlane(meshWidth, meshDepth, subdivX, subdivZ);
            if (auto box = actor->AddActorComponent<UBoxComponent>("Box")) {
                box->SetBoxExtent(glm::vec3(meshWidth * 0.5f, 0.05f, meshDepth * 0.5f));
                box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
                box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
        } else {
            return actor;
        }

        if (!FApplication::HasInstance() || !va)
            return actor;

        auto shader = UAssetManager::GetShader("Engine/Resources/Shaders/PBR_Lit.glsl");
        if (!shader)
            return actor;

        auto& mesh = actor->AddComponent<FMeshComponent>(va, shader);
        mesh.MeshType = meshType;
        mesh.MeshSize = meshSize;
        mesh.MeshWidth = meshWidth;
        mesh.MeshHeight = meshHeight;
        mesh.MeshDepth = meshDepth;
        mesh.MeshRadius = meshRadius;
        mesh.MeshMetersPerUv = meshMetersPerUv;
        mesh.MeshSubdivX = subdivX;
        mesh.MeshSubdivZ = subdivZ;
        mesh.Mobility = EComponentMobility::Static;
        mesh.LightmapResolution = 64;
        mesh.bCastShadows = true;
        mesh.bReceiveShadows = true;

        auto matInst = UAssetManager::GetWorldGridMaterialInstance();
        if (matInst) {
            actor->AddComponent<FMaterialComponent>(matInst, "Engine/Materials/M_WorldGrid.lmat");
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
