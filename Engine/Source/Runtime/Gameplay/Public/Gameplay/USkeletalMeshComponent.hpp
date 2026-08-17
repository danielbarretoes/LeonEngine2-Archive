#pragma once

#include "Gameplay/USceneComponent.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Gameplay/UAnimInstance.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

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

    private:
        void EnsureRenderComponent();
        void PushToRenderComponent();
        glm::mat4 GetRelativeMatrix() const;

        TRef<USkeletalMesh> SkeletalMesh;
        TRef<UAnimInstance> AnimInstance;
        std::string MeshAssetPath;
        std::vector<glm::mat4> BonePalette;
        FPose EvaluatedPose;
        bool bHiddenInGame = false;
    };

} // namespace Leon
