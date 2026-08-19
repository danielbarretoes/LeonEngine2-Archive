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
#include "Gameplay/FGameplayDebugger.hpp"
#include "Renderer/FDebugRenderer.hpp"

#include <algorithm>
#include <cctype>
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
        if (bSimulatingRagdoll)
            return;
        // Capsule ragdoll fallback: keep the last evaluated pose; do not keep driving montages/locomotion.
        if (AActor* owner = GetOwner()) {
            if (auto* character = dynamic_cast<ACharacter*>(owner); character && character->IsRagdoll()) {
                PushToRenderComponent();
                return;
            }
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

        FAnimRuntime::LocalToComponent(*SkeletalMesh->GetSkeleton(), EvaluatedPose, ComponentSpaceTransforms);
        if (SkeletalMesh->HasMeshBindPoses())
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms,
                                               SkeletalMesh->GetInverseBindPoses(), BonePalette);
        else
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms, BonePalette);
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
        // Match USceneComponent: full parent chain (location, rotation, scale).
        if (AttachParent)
            return AttachParent->GetComponentWorldMatrix() * GetRelativeMatrix();
        if (GetOwner())
            return GetOwner()->GetTransform().GetTransform() * GetRelativeMatrix();
        return GetRelativeMatrix();
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

    void USkeletalMeshComponent::AddSocket(const FSkeletalMeshSocket& InSocket) {
        Sockets.push_back(InSocket);
    }

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
        // Refresh component-space from the last evaluated pose (do not keep a stale cache).
        if (SkeletalMesh && SkeletalMesh->GetSkeleton()) {
            if (EvaluatedPose.LocalTransforms.empty())
                FAnimRuntime::RestPose(*SkeletalMesh->GetSkeleton(), EvaluatedPose);
            FAnimRuntime::LocalToComponent(*SkeletalMesh->GetSkeleton(), EvaluatedPose, ComponentSpaceTransforms);
        }
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
            const int32_t boneIdx = SkeletalMesh && SkeletalMesh->GetSkeleton()
                                        ? SkeletalMesh->GetSkeleton()->FindBoneIndex(desc.BoneName)
                                        : -1;
            if (boneIdx < 0 || boneIdx >= static_cast<int32_t>(ComponentSpaceTransforms.size())) {
                cleanup();
                return false;
            }
            glm::vec3 worldPos;
            glm::quat worldRot;
            ComponentToPhysicsWorld(GetComponentWorldMatrix(), ComponentSpaceTransforms[static_cast<size_t>(boneIdx)],
                                    worldPos, worldRot);
            FPhysicsBodyCreateInfo info;
            info.Motion = EPhysicsMotionType::Dynamic;
            info.bSimulatePhysics = true;
            info.bEnableGravity = true;
            info.ObjectType = ECollisionChannel::WorldDynamic;
            info.CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
            info.Mass = desc.Mass > 0.0f ? desc.Mass : 5.0f;
            info.LinearDamping = 0.35f;
            info.AngularDamping = 0.35f;
            info.Friction = 1.0f;
            info.Restitution = 0.0f;
            info.Location = worldPos + desc.Offset;
            info.Rotation = worldRot;
            info.Component = this;
            info.bUseCCD = true;
            info.bSyncComponentTransform = false;
            // Slightly larger than authored so thin floors catch the corpse reliably.
            constexpr float kSizeBoost = 1.15f;
            if (desc.Shape == EPhysicsAssetBodyShape::Sphere) {
                info.Shape = EPhysicsShapeType::Sphere;
                info.SphereRadius = std::max(desc.Radius * kSizeBoost, 0.08f);
            } else if (desc.Shape == EPhysicsAssetBodyShape::Box) {
                info.Shape = EPhysicsShapeType::Box;
                info.BoxHalfExtent = glm::max(desc.BoxExtent * kSizeBoost, glm::vec3(0.08f));
            } else {
                info.Shape = EPhysicsShapeType::Capsule;
                info.CapsuleRadius = std::max(desc.Radius * kSizeBoost, 0.08f);
                info.CapsuleHalfHeight = std::max(desc.CapsuleHalfHeight * kSizeBoost, info.CapsuleRadius + 0.04f);
            }
            IPhysicsBody* body = scene->CreateRigidBody(info);
            if (!body) {
                cleanup();
                return false;
            }
            RagdollBodies.push_back(body);
            RagdollBoneNames.push_back(desc.BoneName);
        }

        auto nameContains = [](const std::string& InName, const char* InToken) {
            std::string lower = InName;
            for (char& ch : lower)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return lower.find(InToken) != std::string::npos;
        };

        std::vector<FPhysicsAssetConstraint> links = PhysicsAsset->GetConstraints();
        if (SkeletalMesh && SkeletalMesh->GetSkeleton()) {
            const auto& bones = SkeletalMesh->GetSkeleton()->GetBones();
            auto findBodyIndex = [&](const std::string& InName) -> int32_t {
                for (size_t i = 0; i < RagdollBoneNames.size(); ++i) {
                    if (RagdollBoneNames[i] == InName)
                        return static_cast<int32_t>(i);
                }
                return -1;
            };
            // Always ensure every body is linked to its nearest simulated ancestor.
            // Authored constraints often skip limbs when shoulders/etc. have no body.
            for (size_t i = 0; i < RagdollBoneNames.size(); ++i) {
                const int32_t idx = SkeletalMesh->GetSkeleton()->FindBoneIndex(RagdollBoneNames[i]);
                if (idx < 0)
                    continue;
                int32_t ancestor = bones[static_cast<size_t>(idx)].ParentIndex;
                int32_t ancestorBody = -1;
                while (ancestor >= 0 && ancestor < static_cast<int32_t>(bones.size())) {
                    ancestorBody = findBodyIndex(bones[static_cast<size_t>(ancestor)].Name);
                    if (ancestorBody >= 0)
                        break;
                    ancestor = bones[static_cast<size_t>(ancestor)].ParentIndex;
                }
                if (ancestorBody < 0)
                    continue;
                bool bAlready = false;
                for (const FPhysicsAssetConstraint& existing : links) {
                    if ((existing.BoneA == RagdollBoneNames[static_cast<size_t>(ancestorBody)] &&
                         existing.BoneB == RagdollBoneNames[i]) ||
                        (existing.BoneB == RagdollBoneNames[static_cast<size_t>(ancestorBody)] &&
                         existing.BoneA == RagdollBoneNames[i])) {
                        bAlready = true;
                        break;
                    }
                }
                if (bAlready)
                    continue;
                FPhysicsAssetConstraint c;
                c.BoneA = RagdollBoneNames[static_cast<size_t>(ancestorBody)];
                c.BoneB = RagdollBoneNames[i];
                c.Type = EPhysicsConstraintType::SwingTwist;
                links.push_back(c);
            }
        }

        // Self-collision between limbs fights Fixed joints and visually tears the corpse apart.
        for (size_t i = 0; i < RagdollBodies.size(); ++i) {
            for (size_t j = i + 1; j < RagdollBodies.size(); ++j) {
                if (RagdollBodies[i] && RagdollBodies[j])
                    scene->IgnoreCollision(RagdollBodies[i], RagdollBodies[j]);
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

            // Fixed joints keep the corpse as one connected body (SwingTwist was tearing limbs off
            // with our current Jolt pivot setup). Still looks like a ragdoll tumble as a unit.
            FPhysicsConstraintCreateInfo info;
            info.BodyA = a;
            info.BodyB = b;
            info.Type = EPhysicsConstraintType::Fixed;
            IPhysicsConstraint* constraint = scene->CreateConstraint(info);
            if (!constraint)
                continue;
            RagdollConstraints.push_back(constraint);
        }

        if (RagdollConstraints.empty() && RagdollBodies.size() > 1) {
            cleanup();
            return false;
        }

        CaptureRagdollRestLocals();

        // Death hit impulse: InImpulse is knockback direction * strength (gameplay units).
        // Convert to a visible Δv along the last hit direction (slight upward bias).
        glm::vec3 hitImpulse = InImpulse;
        float strength = glm::length(hitImpulse);
        glm::vec3 hitDir(0.0f, 0.25f, 1.0f);
        if (strength > 1.0e-4f)
            hitDir = hitImpulse / strength;
        else
            strength = 8.0f;
        hitDir = glm::normalize(hitDir + glm::vec3(0.0f, 0.22f, 0.0f));
        const float deltaV = std::clamp(strength * 2.5f, 8.0f, 36.0f);

        IPhysicsBody* impulseBody = RagdollBodies.empty() ? nullptr : RagdollBodies.front();
        for (size_t i = 0; i < RagdollBoneNames.size(); ++i) {
            if (nameContains(RagdollBoneNames[i], "hips") || nameContains(RagdollBoneNames[i], "pelvis") ||
                nameContains(RagdollBoneNames[i], "spine")) {
                impulseBody = RagdollBodies[i];
                break;
            }
        }
        for (size_t i = 0; i < RagdollBodies.size(); ++i) {
            IPhysicsBody* body = RagdollBodies[i];
            if (!body)
                continue;
            const float mass = std::max(body->GetMass(), 0.1f);
            const float weight = (body == impulseBody) ? 1.0f : 0.55f;
            body->AddImpulse(hitDir * (mass * deltaV * weight));
        }
        if (impulseBody) {
            // Torque so the corpse tumbles away from the shot instead of sliding rigidly.
            const glm::vec3 side = glm::normalize(glm::cross(hitDir, glm::vec3(0.0f, 1.0f, 0.0f)));
            if (glm::length(side) > 1.0e-4f)
                impulseBody->SetAngularVelocity(side * (deltaV * 0.45f));
        }
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
        RagdollLocalFromParent.clear();
        RagdollBoneScale.clear();
        RagdollBoneIsSimulated.clear();
        bSimulatingRagdoll = false;
    }

    void USkeletalMeshComponent::IgnoreCollisionWith(IPhysicsBody* InBody) {
        if (!InBody || RagdollBodies.empty())
            return;
        AActor* owner = GetOwner();
        UWorld* world = owner ? owner->GetWorld() : nullptr;
        IPhysicsScene* scene = world ? world->GetPhysicsScene() : nullptr;
        if (!scene)
            return;
        for (IPhysicsBody* bone : RagdollBodies) {
            if (bone)
                scene->IgnoreCollision(InBody, bone);
        }
    }

    void USkeletalMeshComponent::ConstrainBodiesToFloor(float InFloorZ) {
        // Lift the whole ragdoll by one translation — per-body snaps tear Fixed joints apart.
        constexpr float kSkin = 0.1f;
        const float minY = InFloorZ + kSkin;
        float lift = 0.0f;
        for (IPhysicsBody* body : RagdollBodies) {
            if (!body)
                continue;
            glm::vec3 loc;
            glm::quat rot;
            body->GetTransform(loc, rot);
            lift = std::max(lift, minY - loc.y);
        }
        if (lift <= 1.0e-4f)
            return;
        for (IPhysicsBody* body : RagdollBodies) {
            if (!body)
                continue;
            glm::vec3 loc;
            glm::quat rot;
            body->GetTransform(loc, rot);
            loc.y += lift;
            body->SetTransform(loc, rot);
            glm::vec3 v = body->GetLinearVelocity();
            if (v.y < 0.0f)
                v.y = 0.0f;
            body->SetLinearVelocity(v);
        }
    }

    glm::quat USkeletalMeshComponent::NormalizedMat3Quat(const glm::mat4& InM) {
        glm::vec3 x = glm::vec3(InM[0]);
        glm::vec3 y = glm::vec3(InM[1]);
        glm::vec3 z = glm::vec3(InM[2]);
        const float lx = glm::length(x);
        const float ly = glm::length(y);
        const float lz = glm::length(z);
        if (lx > 1e-8f)
            x /= lx;
        if (ly > 1e-8f)
            y /= ly;
        if (lz > 1e-8f)
            z /= lz;
        return glm::normalize(glm::quat_cast(glm::mat3(x, y, z)));
    }

    void USkeletalMeshComponent::ComponentToPhysicsWorld(const glm::mat4& InMeshWorld, const glm::mat4& InComponent,
                                                         glm::vec3& OutWorldPos, glm::quat& OutWorldRot) {
        OutWorldPos = glm::vec3(InMeshWorld * glm::vec4(glm::vec3(InComponent[3]), 1.0f));
        const glm::quat meshRot = NormalizedMat3Quat(InMeshWorld);
        const glm::quat boneRot = NormalizedMat3Quat(InComponent);
        OutWorldRot = glm::normalize(meshRot * boneRot);
    }

    glm::vec3 USkeletalMeshComponent::Mat3Scale(const glm::mat4& InM) {
        return {glm::length(glm::vec3(InM[0])), glm::length(glm::vec3(InM[1])), glm::length(glm::vec3(InM[2]))};
    }

    void USkeletalMeshComponent::PhysicsWorldToComponent(const glm::mat4& InMeshWorld, const glm::vec3& InWorldPos,
                                                         const glm::quat& InWorldRot, glm::mat4& OutComponent) {
        // Do NOT use inv(meshWorld)*T*R when the mesh has non-1 scale — that scales the bone basis and
        // explodes skinning (instant giant corpse). Split translation (full inverse) and rotation (unscaled).
        const glm::mat4 invMesh = glm::inverse(InMeshWorld);
        const glm::vec3 compPos = glm::vec3(invMesh * glm::vec4(InWorldPos, 1.0f));
        const glm::quat meshRot = NormalizedMat3Quat(InMeshWorld);
        const glm::quat compRot = glm::normalize(glm::inverse(meshRot) * glm::normalize(InWorldRot));
        OutComponent = glm::translate(glm::mat4(1.0f), compPos) * glm::toMat4(compRot);
    }

    void USkeletalMeshComponent::CaptureRagdollRestLocals() {
        RagdollLocalFromParent.clear();
        RagdollBoneScale.clear();
        RagdollBoneIsSimulated.clear();
        if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
            return;
        EnsureComponentSpace();
        const auto& bones = SkeletalMesh->GetSkeleton()->GetBones();
        RagdollLocalFromParent.resize(bones.size(), glm::mat4(1.0f));
        RagdollBoneScale.assign(bones.size(), glm::vec3(1.0f));
        RagdollBoneIsSimulated.assign(bones.size(), 0);
        for (size_t i = 0; i < bones.size() && i < ComponentSpaceTransforms.size(); ++i) {
            RagdollBoneScale[i] = Mat3Scale(ComponentSpaceTransforms[i]);
            const int32_t parent = bones[i].ParentIndex;
            if (parent < 0 || parent >= static_cast<int32_t>(ComponentSpaceTransforms.size()))
                RagdollLocalFromParent[i] = ComponentSpaceTransforms[i];
            else
                RagdollLocalFromParent[i] =
                    glm::inverse(ComponentSpaceTransforms[static_cast<size_t>(parent)]) * ComponentSpaceTransforms[i];
        }
        for (const std::string& name : RagdollBoneNames) {
            const int32_t idx = SkeletalMesh->GetSkeleton()->FindBoneIndex(name);
            if (idx >= 0 && idx < static_cast<int32_t>(RagdollBoneIsSimulated.size()))
                RagdollBoneIsSimulated[static_cast<size_t>(idx)] = 1;
        }
    }

    void USkeletalMeshComponent::ApplyRagdollPoseFromBodies() {
        if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
            return;
        EnsureComponentSpace();
        const auto& bones = SkeletalMesh->GetSkeleton()->GetBones();
        if (ComponentSpaceTransforms.size() < bones.size())
            ComponentSpaceTransforms.resize(bones.size(), glm::mat4(1.0f));

        struct FBoneWorld {
            int32_t Bone = -1;
            glm::vec3 Loc{0.0f};
            glm::quat Rot{1.0f, 0.0f, 0.0f, 0.0f};
        };
        std::vector<FBoneWorld> worlds;
        worlds.reserve(RagdollBodies.size());
        for (size_t i = 0; i < RagdollBodies.size() && i < RagdollBoneNames.size(); ++i) {
            if (!RagdollBodies[i])
                continue;
            const int32_t bone = SkeletalMesh->GetSkeleton()->FindBoneIndex(RagdollBoneNames[i]);
            if (bone < 0 || bone >= static_cast<int32_t>(ComponentSpaceTransforms.size()))
                continue;
            FBoneWorld w;
            w.Bone = bone;
            RagdollBodies[i]->GetTransform(w.Loc, w.Rot);
            worlds.push_back(w);
        }

        // Track actor/camera to the hips so the corpse does not float away from the mesh root.
        glm::vec3 hipsLoc;
        glm::quat hipsRot;
        if (GetRagdollRootTransform(hipsLoc, hipsRot)) {
            if (AActor* owner = GetOwner()) {
                if (owner->GetLocalRole() != ENetRole::SimulatedProxy) {
                    const glm::vec3 euler = glm::degrees(glm::eulerAngles(hipsRot));
                    owner->SetActorLocation(hipsLoc);
                    owner->SetActorRotation({0.0f, euler.y, 0.0f});
                }
            }
        }

        const glm::mat4 meshWorld = GetComponentWorldMatrix();
        for (const FBoneWorld& w : worlds) {
            PhysicsWorldToComponent(meshWorld, w.Loc, w.Rot, ComponentSpaceTransforms[static_cast<size_t>(w.Bone)]);
            if (static_cast<size_t>(w.Bone) < RagdollBoneScale.size()) {
                const glm::vec3 s = glm::max(RagdollBoneScale[static_cast<size_t>(w.Bone)], glm::vec3(1.0e-4f));
                ComponentSpaceTransforms[static_cast<size_t>(w.Bone)] *= glm::scale(glm::mat4(1.0f), s);
            }
        }

        // Non-simulated bones keep the death pose relative to their (possibly simulated) parent.
        if (RagdollLocalFromParent.size() == bones.size()) {
            for (size_t i = 0; i < bones.size(); ++i) {
                if (i < RagdollBoneIsSimulated.size() && RagdollBoneIsSimulated[i])
                    continue;
                const int32_t parent = bones[i].ParentIndex;
                if (parent < 0 || parent >= static_cast<int32_t>(ComponentSpaceTransforms.size()))
                    ComponentSpaceTransforms[i] = RagdollLocalFromParent[i];
                else
                    ComponentSpaceTransforms[i] =
                        ComponentSpaceTransforms[static_cast<size_t>(parent)] * RagdollLocalFromParent[i];
            }
        }

        if (SkeletalMesh->HasMeshBindPoses())
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms,
                                               SkeletalMesh->GetInverseBindPoses(), BonePalette);
        else
            FAnimRuntime::BuildSkinningPalette(*SkeletalMesh->GetSkeleton(), ComponentSpaceTransforms, BonePalette);
        PushToRenderComponent();

        if (FGameplayDebugger::ShowPhysics()) {
            for (size_t i = 0; i < RagdollBodies.size(); ++i) {
                if (!RagdollBodies[i])
                    continue;
                glm::vec3 loc;
                glm::quat rot;
                RagdollBodies[i]->GetTransform(loc, rot);
                float radius = 0.08f;
                float halfHeight = 0.16f;
                if (PhysicsAsset && i < PhysicsAsset->GetBodies().size()) {
                    radius = PhysicsAsset->GetBodies()[i].Radius;
                    halfHeight = PhysicsAsset->GetBodies()[i].CapsuleHalfHeight;
                }
                FDebugRenderer::DrawDebugCapsule(loc, radius, halfHeight, glm::vec4(1.0f, 0.45f, 0.12f, 1.0f));
            }
        }
    }

    bool USkeletalMeshComponent::GetRagdollRootTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const {
        auto tryToken = [&](const char* InToken) {
            for (size_t i = 0; i < RagdollBoneNames.size() && i < RagdollBodies.size(); ++i) {
                if (!RagdollBodies[i])
                    continue;
                std::string lower = RagdollBoneNames[i];
                for (char& ch : lower)
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (lower.find(InToken) == std::string::npos)
                    continue;
                RagdollBodies[i]->GetTransform(OutLocation, OutRotation);
                return true;
            }
            return false;
        };
        if (tryToken("hips") || tryToken("pelvis") || tryToken("spine"))
            return true;
        if (!RagdollBodies.empty() && RagdollBodies.front()) {
            RagdollBodies.front()->GetTransform(OutLocation, OutRotation);
            return true;
        }
        return false;
    }

} // namespace Leon
