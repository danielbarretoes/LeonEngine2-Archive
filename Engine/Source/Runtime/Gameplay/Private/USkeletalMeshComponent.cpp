#include "Gameplay/USkeletalMeshComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Assets/FAnimRuntime.hpp"
#include "Assets/UAssetManager.hpp"
#include "Engine/Components.hpp"
#include "Engine/ENetTypes.hpp"
#include "Core/FFrameProfiler.hpp"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Leon {

    USkeletalMeshComponent::USkeletalMeshComponent(const std::string& InName) : USceneComponent(InName) {}

    void USkeletalMeshComponent::BeginPlay() {
        EnsureRenderComponent();
        if (!SkeletalMesh && !MeshAssetPath.empty())
            SetMeshAssetPath(MeshAssetPath);
        PushToRenderComponent();
    }

    glm::mat4 USkeletalMeshComponent::GetRelativeMatrix() const {
        glm::vec3 rot = RelativeRotation;
        if (SkeletalMesh && SkeletalMesh->GetAssetForwardAxis() == EAssetForwardAxis::SourcePosZ)
            rot.y += 180.0f;
        glm::mat4 r = glm::toMat4(glm::quat(glm::radians(rot)));
        return glm::translate(glm::mat4(1.0f), RelativeLocation) * r * glm::scale(glm::mat4(1.0f), RelativeScale);
    }

    void USkeletalMeshComponent::EnsureRenderComponent() {
        AActor* owner = GetOwner();
        if (!owner)
            return;
        if (!owner->HasComponent<FSkinnedMeshRenderState>())
            owner->AddComponent<FSkinnedMeshRenderState>();
    }

    void USkeletalMeshComponent::SetSkeletalMesh(const TRef<USkeletalMesh>& InMesh) {
        SkeletalMesh = InMesh;
        if (SkeletalMesh && SkeletalMesh->GetAssetPath().size())
            MeshAssetPath = SkeletalMesh->GetAssetPath();
        auto anim = GetOrCreateAnimInstance();
        if (SkeletalMesh && SkeletalMesh->GetSkeleton()) {
            anim->Initialize(SkeletalMesh->GetSkeleton());
            if (SkeletalMesh->GetSkeleton())
                FAnimRuntime::RestPose(*SkeletalMesh->GetSkeleton(), EvaluatedPose);
        }
        EnsureRenderComponent();
        PushToRenderComponent();
    }

    void USkeletalMeshComponent::SetMeshAssetPath(const std::string& InPath) {
        MeshAssetPath = InPath;
        if (!InPath.empty())
            SetSkeletalMesh(UAssetManager::GetSkeletalMesh(InPath));
    }

    TRef<UAnimInstance> USkeletalMeshComponent::GetOrCreateAnimInstance() {
        if (!AnimInstance)
            AnimInstance = MakeRef<UAnimInstance>("AnimInstance");
        return AnimInstance;
    }

    void USkeletalMeshComponent::SetAnimInstance(const TRef<UAnimInstance>& InAnim) {
        AnimInstance = InAnim;
        if (AnimInstance && SkeletalMesh && SkeletalMesh->GetSkeleton())
            AnimInstance->Initialize(SkeletalMesh->GetSkeleton());
    }

    void USkeletalMeshComponent::PlayAnimation(const TRef<UAnimSequence>& InSequence, bool bLoop) {
        GetOrCreateAnimInstance()->SetOverrideSequence(InSequence, bLoop);
    }

    void USkeletalMeshComponent::StopOverrideAnimation() {
        if (AnimInstance)
            AnimInstance->ClearOverrideSequence();
    }

    void USkeletalMeshComponent::Tick(float DeltaSeconds) {
        FFrameProfiler::FScope animScope(&FFrameProfiler::Working().AnimationMs);
        if (bHiddenInGame) {
            PushToRenderComponent();
            return;
        }
        if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
            return;

        auto anim = GetOrCreateAnimInstance();
        if (AActor* owner = GetOwner()) {
            if (auto* character = dynamic_cast<ACharacter*>(owner)) {
                if (owner->GetLocalRole() != ENetRole::SimulatedProxy)
                    character->UpdateAnimInstance(*anim);
                else
                    anim->ApplyRepState(character->GetAnimRepState());
            }
        }

        anim->NativeUpdateAnimation(DeltaSeconds);
        anim->Evaluate(EvaluatedPose);

        std::vector<glm::mat4> component;
        FAnimRuntime::LocalToComponent(*SkeletalMesh->GetSkeleton(), EvaluatedPose, component);
        if (SkeletalMesh->HasMeshBindPoses())
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), component,
                                               SkeletalMesh->GetInverseBindPoses(), BonePalette);
        else
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), component, BonePalette);
        PushToRenderComponent();
    }

    void USkeletalMeshComponent::PushToRenderComponent() {
        AActor* owner = GetOwner();
        if (!owner)
            return;
        EnsureRenderComponent();
        auto& render = owner->GetComponent<FSkinnedMeshRenderState>();
        render.SkeletalMesh = SkeletalMesh;
        render.AssetPath = MeshAssetPath;
        render.BonePalette = BonePalette;
        render.RelativeLocation = RelativeLocation;
        glm::vec3 rot = RelativeRotation;
        if (SkeletalMesh && SkeletalMesh->GetAssetForwardAxis() == EAssetForwardAxis::SourcePosZ)
            rot.y += 180.0f;
        render.RelativeRotation = rot;
        render.RelativeScale = RelativeScale;
        render.bVisible = !bHiddenInGame;
        render.bCastShadows = !bHiddenInGame;
    }

} // namespace Leon
