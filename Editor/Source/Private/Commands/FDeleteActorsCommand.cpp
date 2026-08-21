#include "Editor/Commands/FDeleteActorsCommand.hpp"
#include "Assets/UAssetManager.hpp"
#include "Core/FLog.hpp"
#include "Editor/Context/FEditorSelection.hpp"
#include "Gameplay/APlayerStart.hpp"
#include "Gameplay/ATriggerVolume.hpp"
#include "Gameplay/UClassRegistry.hpp"
#include "Renderer/FMeshPrimitives.hpp"
#include "RHI/IRenderDriver.hpp"

namespace Leon::Editor {

    FActorEditorSnapshot CaptureActorSnapshot(AActor& InActor) {
        FActorEditorSnapshot snap;
        snap.Name = InActor.GetName();
        snap.ClassName = InActor.GetClass();
        snap.Guid = InActor.GetActorGuid();
        snap.FolderPath = InActor.GetFolderPath();

        if (InActor.HasComponent<FTransformComponent>()) {
            snap.bHasTransform = true;
            snap.Transform = InActor.GetComponent<FTransformComponent>();
        }
        if (InActor.HasComponent<FStaticMeshComponent>()) {
            snap.bHasStaticMesh = true;
            snap.StaticMesh = InActor.GetComponent<FStaticMeshComponent>();
        }
        if (InActor.HasComponent<FMeshComponent>()) {
            snap.bHasMesh = true;
            snap.Mesh = InActor.GetComponent<FMeshComponent>();
            // VertexArray/Shader are GPU refs — rebuild on restore from metadata.
            snap.Mesh.VertexArray = nullptr;
            snap.Mesh.Shader = nullptr;
        }
        if (InActor.HasComponent<FMaterialComponent>()) {
            snap.bHasMaterial = true;
            snap.Material = InActor.GetComponent<FMaterialComponent>();
        }
        if (InActor.HasComponent<FDirectionalLightComponent>()) {
            snap.bHasDirLight = true;
            snap.DirLight = InActor.GetComponent<FDirectionalLightComponent>();
        }
        if (InActor.HasComponent<FPointLightComponent>()) {
            snap.bHasPointLight = true;
            snap.PointLight = InActor.GetComponent<FPointLightComponent>();
        }
        if (InActor.HasComponent<FSpotLightComponent>()) {
            snap.bHasSpotLight = true;
            snap.SpotLight = InActor.GetComponent<FSpotLightComponent>();
        }
        if (InActor.HasComponent<FCameraComponent>()) {
            snap.bHasCamera = true;
            snap.Camera = InActor.GetComponent<FCameraComponent>();
        }
        if (InActor.HasComponent<FBoxCollisionComponent>()) {
            snap.bHasBoxCollision = true;
            snap.BoxCollision = InActor.GetComponent<FBoxCollisionComponent>();
        }
        if (auto* start = dynamic_cast<APlayerStart*>(&InActor)) {
            snap.bHasPlayerStart = true;
            snap.PlayerStartTag = start->GetPlayerStartTag();
            snap.PlayerStartTeamIndex = start->GetTeamIndex();
            snap.bPlayerStartEnabled = start->IsEnabled();
        }
        if (auto* trigger = dynamic_cast<ATriggerVolume*>(&InActor)) {
            snap.bHasTriggerVolume = true;
            snap.bTriggerEnabled = trigger->IsEnabled();
        }
        if (InActor.HasComponent<FSkyboxComponent>()) {
            snap.bHasSkybox = true;
            snap.Skybox = InActor.GetComponent<FSkyboxComponent>();
        }
        return snap;
    }

    namespace {

        TRef<FVertexArray> RebuildProceduralMesh(const FMeshComponent& InMesh) {
            if (!FRenderDriverRegistry::GetActiveDriver())
                return nullptr;

            const std::string& t = InMesh.MeshType;
            if (t == "Box")
                return FMeshPrimitives::CreateBox(InMesh.MeshWidth, InMesh.MeshHeight, InMesh.MeshDepth,
                                                  InMesh.MeshMetersPerUv);
            if (t == "Cube")
                return FMeshPrimitives::CreateCube(InMesh.MeshSize);
            if (t == "Plane")
                return FMeshPrimitives::CreatePlane(InMesh.MeshWidth, InMesh.MeshDepth, InMesh.MeshSubdivX,
                                                    InMesh.MeshSubdivZ);
            if (t == "Sphere")
                return FMeshPrimitives::CreateSphere(InMesh.MeshRadius, InMesh.MeshSubdivX, InMesh.MeshSubdivZ);
            if (t == "Cylinder")
                return FMeshPrimitives::CreateCylinder(InMesh.MeshRadius, InMesh.MeshRadius, InMesh.MeshHeight,
                                                       InMesh.MeshSubdivX, true);
            if (t == "Cone")
                return FMeshPrimitives::CreateCylinder(InMesh.MeshRadius, 0.0f, InMesh.MeshHeight, InMesh.MeshSubdivX,
                                                       true);
            if (t == "Ramp")
                return FMeshPrimitives::CreateRamp(InMesh.MeshWidth, InMesh.MeshHeight, InMesh.MeshDepth);
            if (t == "Pyramid")
                return FMeshPrimitives::CreatePyramid(InMesh.MeshWidth, InMesh.MeshHeight, InMesh.MeshDepth);
            if (t == "Quad")
                return FMeshPrimitives::CreateQuad(InMesh.MeshWidth, InMesh.MeshHeight);
            return FMeshPrimitives::CreateCube(InMesh.MeshSize > 0.0f ? InMesh.MeshSize : 1.0f);
        }

    } // namespace

    AActor* RestoreActorSnapshot(UWorld& InWorld, const FActorEditorSnapshot& InSnapshot) {
        AActor* actor = nullptr;
        const std::string& className = InSnapshot.ClassName;
        if (!className.empty() && UClassRegistry::Get().HasClass(className)) {
            actor = UClassRegistry::Get().CreateActorOfClass(
                className, &InWorld, InSnapshot.Name.empty() ? className : InSnapshot.Name);
        }
        if (!actor)
            actor = InWorld.SpawnActor(InSnapshot.Name.empty() ? "RestoredActor" : InSnapshot.Name);
        if (!actor)
            return nullptr;

        if (!InSnapshot.ClassName.empty())
            actor->SetClass(InSnapshot.ClassName);
        if (InSnapshot.Guid.IsValid())
            actor->SetActorGuid(InSnapshot.Guid);
        if (!InSnapshot.FolderPath.empty()) {
            actor->SetFolderPath(InSnapshot.FolderPath);
            InWorld.RegisterEditorFolder(InSnapshot.FolderPath);
        }

        if (InSnapshot.bHasPlayerStart) {
            if (auto* start = dynamic_cast<APlayerStart*>(actor)) {
                start->SetPlayerStartTag(InSnapshot.PlayerStartTag);
                start->SetTeamIndex(InSnapshot.PlayerStartTeamIndex);
                start->SetEnabled(InSnapshot.bPlayerStartEnabled);
            }
        }
        if (InSnapshot.bHasTriggerVolume) {
            if (auto* trigger = dynamic_cast<ATriggerVolume*>(actor))
                trigger->SetEnabled(InSnapshot.bTriggerEnabled);
        }

        if (InSnapshot.bHasTransform) {
            if (!actor->HasComponent<FTransformComponent>())
                actor->AddComponent<FTransformComponent>();
            actor->GetComponent<FTransformComponent>() = InSnapshot.Transform;
        }

        if (InSnapshot.bHasStaticMesh) {
            auto& smc = actor->AddComponent<FStaticMeshComponent>(InSnapshot.StaticMesh);
            if (!smc.StaticMesh && !smc.AssetPath.empty())
                smc.StaticMesh = UAssetManager::GetStaticMesh(smc.AssetPath);
        }

        if (InSnapshot.bHasMesh) {
            FMeshComponent mesh = InSnapshot.Mesh;
            mesh.VertexArray = RebuildProceduralMesh(mesh);
            if (FRenderDriverRegistry::GetActiveDriver() && !mesh.ShaderPath.empty()) {
                mesh.Shader = UAssetManager::GetShader(mesh.ShaderPath);
                if (!mesh.Shader)
                    mesh.Shader = FShader::Create(mesh.ShaderPath);
            }
            actor->AddComponent<FMeshComponent>(mesh);
        }

        if (InSnapshot.bHasMaterial)
            actor->AddComponent<FMaterialComponent>(InSnapshot.Material);
        if (InSnapshot.bHasDirLight)
            actor->AddComponent<FDirectionalLightComponent>(InSnapshot.DirLight);
        if (InSnapshot.bHasPointLight)
            actor->AddComponent<FPointLightComponent>(InSnapshot.PointLight);
        if (InSnapshot.bHasSpotLight)
            actor->AddComponent<FSpotLightComponent>(InSnapshot.SpotLight);
        if (InSnapshot.bHasCamera)
            actor->AddComponent<FCameraComponent>(InSnapshot.Camera);
        if (InSnapshot.bHasBoxCollision)
            actor->AddComponent<FBoxCollisionComponent>(InSnapshot.BoxCollision);
        if (InSnapshot.bHasSkybox) {
            if (actor->HasComponent<FSkyboxComponent>())
                actor->GetComponent<FSkyboxComponent>() = InSnapshot.Skybox;
            else
                actor->AddComponent<FSkyboxComponent>(InSnapshot.Skybox);
        }

        return actor;
    }

    FDeleteActorsCommand::FDeleteActorsCommand(UWorld* InWorld, FEditorSelection* InSelection,
                                               std::vector<AActor*> InActors)
        : World(InWorld), Selection(InSelection) {
        for (AActor* actor : InActors) {
            if (!actor || actor->IsPendingKill())
                continue;
            Snapshots.push_back(CaptureActorSnapshot(*actor));
            LiveActors.push_back(actor);
        }

        if (Snapshots.size() == 1) {
            Description = "Delete " + Snapshots.front().Name;
        } else {
            Description = "Delete " + std::to_string(Snapshots.size()) + " Actors";
        }
    }

    void FDeleteActorsCommand::Execute() {
        if (!World || LiveActors.empty())
            return;

        if (Selection)
            Selection->ClearActorSelection();

        for (AActor* actor : LiveActors) {
            if (actor && !actor->IsPendingKill())
                World->DestroyActor(actor);
        }
        LiveActors.clear();
    }

    void FDeleteActorsCommand::Undo() {
        if (!World || Snapshots.empty())
            return;

        LiveActors.clear();
        for (const FActorEditorSnapshot& snap : Snapshots) {
            if (AActor* restored = RestoreActorSnapshot(*World, snap))
                LiveActors.push_back(restored);
        }

        if (Selection && !LiveActors.empty())
            Selection->SetSelectedActors(LiveActors);

        LE_CORE_INFO("Undo: restored {} actor(s)", LiveActors.size());
    }

    std::string FDeleteActorsCommand::GetDescription() const {
        return Description;
    }

} // namespace Leon::Editor
