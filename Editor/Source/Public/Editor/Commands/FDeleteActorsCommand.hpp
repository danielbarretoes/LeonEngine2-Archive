#pragma once

#include "Editor/Commands/IEditorCommand.hpp"
#include "Assets/FAssetTypes.hpp"
#include "Engine/Components.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"

#include <string>
#include <vector>

namespace Leon::Editor {

    class FEditorSelection;

    /** Serializable subset of an actor for Delete undo/redo. */
    struct FActorEditorSnapshot {
        std::string Name;
        std::string ClassName;
        FUUID Guid;
        std::string FolderPath;

        bool bHasTransform = false;
        FTransformComponent Transform;

        bool bHasStaticMesh = false;
        FStaticMeshComponent StaticMesh;

        bool bHasMesh = false;
        FMeshComponent Mesh;

        bool bHasMaterial = false;
        FMaterialComponent Material;

        bool bHasDirLight = false;
        FDirectionalLightComponent DirLight;

        bool bHasPointLight = false;
        FPointLightComponent PointLight;

        bool bHasSpotLight = false;
        FSpotLightComponent SpotLight;

        bool bHasCamera = false;
        FCameraComponent Camera;

        bool bHasBoxCollision = false;
        FBoxCollisionComponent BoxCollision;

        bool bHasPlayerStart = false;
        std::string PlayerStartTag;
        int32_t PlayerStartTeamIndex = 0;
        bool bPlayerStartEnabled = true;

        bool bHasTriggerVolume = false;
        bool bTriggerEnabled = true;

        bool bHasSkybox = false;
        FSkyboxComponent Skybox;
    };

    FActorEditorSnapshot CaptureActorSnapshot(AActor& InActor);
    AActor* RestoreActorSnapshot(UWorld& InWorld, const FActorEditorSnapshot& InSnapshot);

    class FDeleteActorsCommand : public IEditorCommand {
    public:
        FDeleteActorsCommand(UWorld* InWorld, FEditorSelection* InSelection, std::vector<AActor*> InActors);

        void Execute() override;
        void Undo() override;
        [[nodiscard]] std::string GetDescription() const override;

    private:
        UWorld* World = nullptr;
        FEditorSelection* Selection = nullptr;
        std::vector<FActorEditorSnapshot> Snapshots;
        std::vector<AActor*> LiveActors;
        std::string Description;
    };

} // namespace Leon::Editor
