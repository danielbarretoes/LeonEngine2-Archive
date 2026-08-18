#include "Gameplay/USkeletalMeshComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/ACharacter.hpp"
#include "Assets/FAnimRuntime.hpp"
#include "Assets/UAssetManager.hpp"
#include "Engine/Components.hpp"
#include "Engine/ENetTypes.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/IPhysicsScene.hpp"
#include "Core/FFrameProfiler.hpp"

#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace Leon {

    USkeletalMeshComponent::USkeletalMeshComponent(const std::string& InName) : USceneComponent(InName) {}

    void USkeletalMeshComponent::BeginPlay() {
        EnsureRenderComponent();
        if (!SkeletalMesh && !MeshAssetPath.empty())
            SetMeshAssetPath(MeshAssetPath);
        PushToRenderComponent();
    }

    void USkeletalMeshComponent::EndPlay() {
        StopRagdoll();
        USceneComponent::EndPlay();
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
        if (bSimulatingRagdoll) {
            const glm::mat4 meshWorld = GetComponentWorldMatrix();
            const glm::mat4 invMesh = glm::inverse(meshWorld);
            for (size_t i = 0; i < RagdollBodies.size() && i < RagdollBoneNames.size(); ++i) {
                if (!RagdollBodies[i] || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
                    continue;
                const int32_t bone = SkeletalMesh->GetSkeleton()->FindBoneIndex(RagdollBoneNames[i]);
                if (bone < 0)
                    continue;
                EnsureComponentSpace();
                if (bone >= static_cast<int32_t>(ComponentSpaceTransforms.size()))
                    continue;
                glm::vec3 loc;
                glm::quat rot;
                RagdollBodies[i]->GetTransform(loc, rot);
                glm::mat4 boneWorld = glm::translate(glm::mat4(1.0f), loc) * glm::toMat4(glm::normalize(rot));
                ComponentSpaceTransforms[static_cast<size_t>(bone)] = invMesh * boneWorld;
            }
            if (SkeletalMesh && SkeletalMesh->GetSkeleton()) {
                if (SkeletalMesh->HasMeshBindPoses())
                    FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms,
                                                       SkeletalMesh->GetInverseBindPoses(), BonePalette);
                else
                    FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms,
                                                       BonePalette);
            }
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
        ComponentSpaceTransforms = std::move(component);
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

    glm::mat4 USkeletalMeshComponent::GetComponentWorldMatrix() const {
        glm::mat4 parent(1.0f);
        if (AttachParent) {
            parent = glm::translate(glm::mat4(1.0f), AttachParent->GetComponentLocation()) *
                     glm::toMat4(glm::quat(glm::radians(AttachParent->GetComponentRotation())));
        } else if (GetOwner()) {
            parent = glm::translate(glm::mat4(1.0f), GetOwner()->GetActorLocation()) *
                     glm::toMat4(glm::quat(glm::radians(GetOwner()->GetActorRotation())));
        }
        return parent * GetRelativeMatrix();
    }

    void USkeletalMeshComponent::EnsureComponentSpace() {
        if (!ComponentSpaceTransforms.empty() || !SkeletalMesh || !SkeletalMesh->GetSkeleton())
            return;
        if (EvaluatedPose.LocalTransforms.empty())
            FAnimRuntime::RestPose(*SkeletalMesh->GetSkeleton(), EvaluatedPose);
        FAnimRuntime::LocalToComponent(*SkeletalMesh->GetSkeleton(), EvaluatedPose, ComponentSpaceTransforms);
    }

    bool USkeletalMeshComponent::GetBoneMatrix(const std::string& InBoneName, glm::mat4& OutWorld) const {
        const_cast<USkeletalMeshComponent*>(this)->EnsureComponentSpace();
        if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
            return false;
        const int32_t idx = SkeletalMesh->GetSkeleton()->FindBoneIndex(InBoneName);
        if (idx < 0 || idx >= static_cast<int32_t>(ComponentSpaceTransforms.size()))
            return false;
        OutWorld = GetComponentWorldMatrix() * ComponentSpaceTransforms[static_cast<size_t>(idx)];
        return true;
    }

    bool USkeletalMeshComponent::GetBoneLocation(const std::string& InBoneName, glm::vec3& OutLocation) const {
        glm::mat4 world;
        if (!GetBoneMatrix(InBoneName, world))
            return false;
        OutLocation = glm::vec3(world[3]);
        return true;
    }

    void USkeletalMeshComponent::AddSocket(const FSkeletalMeshSocket& InSocket) { Sockets.push_back(InSocket); }

    const FSkeletalMeshSocket* USkeletalMeshComponent::FindSocket(const std::string& InName) const {
        for (const auto& socket : Sockets) {
            if (socket.SocketName == InName)
                return &socket;
        }
        if (SkeletalMesh)
            return SkeletalMesh->FindSocket(InName);
        return nullptr;
    }

    bool USkeletalMeshComponent::GetSocketLocation(const std::string& InSocketName, glm::vec3& OutLocation) const {
        const FSkeletalMeshSocket* socket = FindSocket(InSocketName);
        if (socket) {
            glm::mat4 boneWorld;
            if (!GetBoneMatrix(socket->BoneName, boneWorld))
                return false;
            const glm::mat4 offset = glm::translate(glm::mat4(1.0f), socket->RelativeLocation) *
                                     glm::toMat4(glm::quat(glm::radians(socket->RelativeRotation)));
            OutLocation = glm::vec3((boneWorld * offset)[3]);
            return true;
        }
        return GetBoneLocation(InSocketName, OutLocation);
    }

    bool USkeletalMeshComponent::TryEnableRagdoll(const glm::vec3& InImpulse) {
        AActor* owner = GetOwner();
        UWorld* world = owner ? owner->GetWorld() : nullptr;
        IPhysicsScene* scene = world ? world->GetPhysicsScene() : nullptr;
        if (!scene || !PhysicsAsset || PhysicsAsset->GetBodies().empty())
            return false;
        EnsureComponentSpace();
        StopRagdoll();

        auto cleanup = [&]() {
            for (IPhysicsConstraint* c : RagdollConstraints) {
                if (c)
                    scene->DestroyConstraint(c);
            }
            for (IPhysicsBody* b : RagdollBodies) {
                if (b)
                    scene->DestroyRigidBody(b);
            }
            RagdollConstraints.clear();
            RagdollBodies.clear();
            RagdollBoneNames.clear();
        };

        for (const FPhysicsAssetBody& desc : PhysicsAsset->GetBodies()) {
            glm::mat4 boneWorld;
            if (!GetBoneMatrix(desc.BoneName, boneWorld)) {
                cleanup();
                return false;
            }
            FPhysicsBodyCreateInfo info;
            info.Motion = EPhysicsMotionType::Dynamic;
            info.bSimulatePhysics = true;
            info.bEnableGravity = true;
            info.ObjectType = ECollisionChannel::Pawn;
            info.Mass = 8.0f;
            info.LinearDamping = 0.4f;
            info.Location = glm::vec3(boneWorld[3]) + desc.Offset;
            info.Component = this;
            if (desc.Shape == EPhysicsAssetBodyShape::Sphere) {
                info.Shape = EPhysicsShapeType::Sphere;
                info.SphereRadius = desc.Radius;
            } else if (desc.Shape == EPhysicsAssetBodyShape::Box) {
                info.Shape = EPhysicsShapeType::Box;
                info.BoxHalfExtent = desc.BoxExtent;
            } else {
                info.Shape = EPhysicsShapeType::Capsule;
                info.CapsuleRadius = desc.Radius;
                info.CapsuleHalfHeight = desc.CapsuleHalfHeight;
            }
            IPhysicsBody* body = scene->CreateRigidBody(info);
            if (!body) {
                cleanup();
                return false;
            }
            RagdollBodies.push_back(body);
            RagdollBoneNames.push_back(desc.BoneName);
        }

        std::vector<FPhysicsAssetConstraint> links = PhysicsAsset->GetConstraints();
        if (links.empty() && SkeletalMesh && SkeletalMesh->GetSkeleton()) {
            const auto& bones = SkeletalMesh->GetSkeleton()->GetBones();
            for (size_t i = 0; i < RagdollBoneNames.size(); ++i) {
                const int32_t idx = SkeletalMesh->GetSkeleton()->FindBoneIndex(RagdollBoneNames[i]);
                if (idx < 0)
                    continue;
                const int32_t parent = bones[static_cast<size_t>(idx)].ParentIndex;
                if (parent < 0)
                    continue;
                const std::string& parentName = bones[static_cast<size_t>(parent)].Name;
                for (size_t j = 0; j < RagdollBoneNames.size(); ++j) {
                    if (RagdollBoneNames[j] == parentName) {
                        FPhysicsAssetConstraint c;
                        c.BoneA = parentName;
                        c.BoneB = RagdollBoneNames[i];
                        links.push_back(c);
                        break;
                    }
                }
            }
        }

        for (const FPhysicsAssetConstraint& link : links) {
            IPhysicsBody* a = nullptr;
            IPhysicsBody* b = nullptr;
            for (size_t i = 0; i < RagdollBoneNames.size(); ++i) {
                if (RagdollBoneNames[i] == link.BoneA)
                    a = RagdollBodies[i];
                if (RagdollBoneNames[i] == link.BoneB)
                    b = RagdollBodies[i];
            }
            if (!a || !b)
                continue;
            FPhysicsConstraintCreateInfo info;
            info.BodyA = a;
            info.BodyB = b;
            info.RestLength = link.RestLength;
            IPhysicsConstraint* constraint = scene->CreateConstraint(info);
            if (!constraint) {
                cleanup();
                return false;
            }
            RagdollConstraints.push_back(constraint);
        }

        if (!RagdollBodies.empty())
            RagdollBodies.front()->AddImpulse(InImpulse);
        bSimulatingRagdoll = true;
        return true;
    }

    void USkeletalMeshComponent::StopRagdoll() {
        AActor* owner = GetOwner();
        UWorld* world = owner ? owner->GetWorld() : nullptr;
        IPhysicsScene* scene = world ? world->GetPhysicsScene() : nullptr;
        if (scene) {
            for (IPhysicsConstraint* c : RagdollConstraints) {
                if (c)
                    scene->DestroyConstraint(c);
            }
            for (IPhysicsBody* b : RagdollBodies) {
                if (b)
                    scene->DestroyRigidBody(b);
            }
        }
        RagdollConstraints.clear();
        RagdollBodies.clear();
        RagdollBoneNames.clear();
        bSimulatingRagdoll = false;
    }

} // namespace Leon
