#pragma once

#include "Gameplay/USceneComponent.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Assets/UPhysicsAsset.hpp"
#include "Assets/FSkeletalMeshSocket.hpp"
#include "Gameplay/UAnimInstance.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class IPhysicsBody;
    class IPhysicsConstraint;

    /**
     * Visual skeletal mesh (Unreal USkeletalMeshComponent lite).
     * Attach under CapsuleComponent via SetupAttachment; no collision of its own.
     * Writes the skinning palette to the owner's FSkinnedMeshRenderState EnTT POD.
     */
    class USkeletalMeshComponent : public USceneComponent {
    public:
        USkeletalMeshComponent(const std::string& InName = "SkeletalMeshComponent");
        ~USkeletalMeshComponent() override = default;

        void BeginPlay() override;
        void Tick(float DeltaSeconds) override;
        void EndPlay() override;

        void SetSkeletalMesh(const TRef<USkeletalMesh>& InMesh);
        TRef<USkeletalMesh> GetSkeletalMesh() const { return SkeletalMesh; }

        void SetMeshAssetPath(const std::string& InPath);
        const std::string& GetMeshAssetPath() const { return MeshAssetPath; }

        TRef<UAnimInstance> GetAnimInstance() const { return AnimInstance; }
        TRef<UAnimInstance> GetOrCreateAnimInstance();
        void SetAnimInstance(const TRef<UAnimInstance>& InAnim);

        void PlayAnimation(const TRef<UAnimSequence>& InSequence, bool bLoop = true);
        void StopOverrideAnimation();

        void SetHiddenInGame(bool bHidden) { bHiddenInGame = bHidden; }
        bool IsHiddenInGame() const { return bHiddenInGame; }

        const std::vector<glm::mat4>& GetBonePalette() const { return BonePalette; }

        bool GetBoneMatrix(const std::string& InBoneName, glm::mat4& OutWorld) const;
        bool GetBoneLocation(const std::string& InBoneName, glm::vec3& OutLocation) const;
        bool GetSocketLocation(const std::string& InSocketName, glm::vec3& OutLocation) const;
        void AddSocket(const FSkeletalMeshSocket& InSocket);
        const FSkeletalMeshSocket* FindSocket(const std::string& InName) const;

        void SetPhysicsAsset(const TRef<UPhysicsAsset>& InAsset) { PhysicsAsset = InAsset; }
        TRef<UPhysicsAsset> GetPhysicsAsset() const { return PhysicsAsset; }
        bool TryEnableRagdoll(const glm::vec3& InImpulse);
        void StopRagdoll();
        bool IsRagdoll() const { return bSimulatingRagdoll; }

    private:
        void EnsureRenderComponent();
        void PushToRenderComponent();
        glm::mat4 GetRelativeMatrix() const;
        glm::mat4 GetComponentWorldMatrix() const;
        void EnsureComponentSpace();

        TRef<USkeletalMesh> SkeletalMesh;
        TRef<UAnimInstance> AnimInstance;
        TRef<UPhysicsAsset> PhysicsAsset;
        std::string MeshAssetPath;
        std::vector<glm::mat4> BonePalette;
        std::vector<glm::mat4> ComponentSpaceTransforms;
        std::vector<FSkeletalMeshSocket> Sockets;
        std::vector<IPhysicsBody*> RagdollBodies;
        std::vector<IPhysicsConstraint*> RagdollConstraints;
        std::vector<std::string> RagdollBoneNames;
        FPose EvaluatedPose;
        bool bHiddenInGame = false;
        bool bSimulatingRagdoll = false;
    };

} // namespace Leon
