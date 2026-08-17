#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Assets/USkeletalMesh.hpp"
#include "Assets/UAnimSequence.hpp"
#include "Gameplay/UAnimInstance.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    /**
     * Ticks AnimInstance, writes the skinning palette to the owner's FSkeletalMeshComponent.
     */
    class USkeletalMeshComponent : public UActorComponent {
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

        void SetRelativeLocation(const glm::vec3& InLocation) { RelativeLocation = InLocation; }
        const glm::vec3& GetRelativeLocation() const { return RelativeLocation; }
        void SetRelativeRotation(const glm::vec3& InRotation) { RelativeRotation = InRotation; }
        const glm::vec3& GetRelativeRotation() const { return RelativeRotation; }
        void SetRelativeScale(const glm::vec3& InScale) { RelativeScale = InScale; }
        const glm::vec3& GetRelativeScale() const { return RelativeScale; }

        const std::vector<glm::mat4>& GetBonePalette() const { return BonePalette; }

    private:
        void EnsureRenderComponent();
        void PushToRenderComponent();
        glm::mat4 GetRelativeMatrix() const;

        TRef<USkeletalMesh> SkeletalMesh;
        TRef<UAnimInstance> AnimInstance;
        std::string MeshAssetPath;
        glm::vec3 RelativeLocation{0.0f};
        glm::vec3 RelativeRotation{0.0f};
        glm::vec3 RelativeScale{1.0f};
        std::vector<glm::mat4> BonePalette;
        FPose EvaluatedPose;
    };

} // namespace Leon
