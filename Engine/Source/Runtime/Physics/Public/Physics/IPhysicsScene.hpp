#pragma once

#include "Core/Base.hpp"
#include "Physics/ECollisionTypes.hpp"
#include "Physics/FHitResult.hpp"
#include "Physics/UPhysicalMaterial.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <functional>
#include <vector>

namespace Leon {

    class AActor;
    class UActorComponent;
    class UStaticMesh;

    static constexpr float kPhysicsFixedDeltaSeconds = 1.0f / 60.0f;
    static constexpr int32_t kPhysicsMaxSubsteps = 4;
    static constexpr float kPhysicsMaxFrameDeltaSeconds = 0.1f;

    struct FCollisionQueryParams {
        AActor* IgnoreActor = nullptr;
        UActorComponent* IgnoreComponent = nullptr;
        bool bTraceComplex = true;
    };

    enum class EPhysicsContactKind : uint8_t { Hit = 0, OverlapEnter = 1, OverlapExit = 2 };

    struct FPhysicsContact {
        AActor* ActorA = nullptr;
        AActor* ActorB = nullptr;
        UActorComponent* ComponentA = nullptr;
        UActorComponent* ComponentB = nullptr;
        glm::vec3 Location{0.0f};
        glm::vec3 Normal{0.0f, 1.0f, 0.0f};
        EPhysicsContactKind Kind = EPhysicsContactKind::Hit;
        bool bEnter = true;
    };

    struct FPhysicsBodyCreateInfo {
        EPhysicsShapeType Shape = EPhysicsShapeType::Box;
        EPhysicsMotionType Motion = EPhysicsMotionType::Static;
        ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
        ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
        FCollisionResponseContainer Responses;
        glm::vec3 Location{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 BoxHalfExtent{0.5f};
        float SphereRadius = 0.5f;
        float CapsuleRadius = 0.4f;
        /** Unreal-style: half of total capsule height including hemispheres. */
        float CapsuleHalfHeight = 0.9f;
        float Mass = 1.0f;
        float LinearDamping = 0.01f;
        float AngularDamping = 0.05f;
        float Restitution = 0.0f;
        float Friction = 0.7f;
        bool bEnableGravity = true;
        bool bSimulatePhysics = false;
        bool bUseCCD = false;
        /**
         * When true (default), SyncDynamicTransforms writes this body pose to Component/Actor.
         * Ragdoll bone bodies keep Component for queries but apply pose via ApplyRagdollPoseFromBodies.
         */
        bool bSyncComponentTransform = true;
        AActor* Actor = nullptr;
        UActorComponent* Component = nullptr;
        UPhysicalMaterial* PhysicalMaterial = nullptr;
        const UStaticMesh* CollisionMesh = nullptr;
        glm::vec3 CollisionMeshScale{1.0f};
        glm::mat4 CollisionMeshWorld{1.0f};
    };

    /** Approximate volume (m³) for Mass = Density * volume when Mass is left at default 1. */
    inline float ApproximatePhysicsShapeVolume(const FPhysicsBodyCreateInfo& InInfo) {
        constexpr float kPi = 3.14159265358979323846f;
        if (InInfo.Shape == EPhysicsShapeType::Sphere) {
            const float r = InInfo.SphereRadius > 0.01f ? InInfo.SphereRadius : 0.01f;
            return (4.0f / 3.0f) * kPi * r * r * r;
        }
        if (InInfo.Shape == EPhysicsShapeType::Capsule) {
            const float r = InInfo.CapsuleRadius > 0.01f ? InInfo.CapsuleRadius : 0.01f;
            const float half = InInfo.CapsuleHalfHeight > r ? InInfo.CapsuleHalfHeight : r;
            const float cyl = half > r ? half - r : 0.0f;
            return kPi * r * r * (2.0f * cyl) + (4.0f / 3.0f) * kPi * r * r * r;
        }
        if (InInfo.Shape == EPhysicsShapeType::Box) {
            const float hx = InInfo.BoxHalfExtent.x > 0.01f ? InInfo.BoxHalfExtent.x : 0.01f;
            const float hy = InInfo.BoxHalfExtent.y > 0.01f ? InInfo.BoxHalfExtent.y : 0.01f;
            const float hz = InInfo.BoxHalfExtent.z > 0.01f ? InInfo.BoxHalfExtent.z : 0.01f;
            return 8.0f * hx * hy * hz;
        }
        return 1.0f;
    }

    class IPhysicsBody {
    public:
        virtual ~IPhysicsBody() = default;
        /**
         * Destroy this body via its owning scene. Safe when UWorld is already gone;
         * UPrimitiveComponent::UnregisterPhysics must call this so bodies are never orphaned.
         */
        virtual void Destroy() = 0;
        /** Teleport the body (Unreal FBodyInstance::SetBodyTransform). */
        virtual void SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) = 0;
        virtual void GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const = 0;
        virtual void AddForce(const glm::vec3& InForce) = 0;
        virtual void AddImpulse(const glm::vec3& InImpulse) = 0;
        virtual void AddTorque(const glm::vec3& InTorque) = 0;
        virtual void AddAngularImpulse(const glm::vec3& InImpulse) = 0;
        virtual void SetLinearVelocity(const glm::vec3& InVelocity) = 0;
        virtual void SetAngularVelocity(const glm::vec3& InVelocity) = 0;
        virtual glm::vec3 GetLinearVelocity() const = 0;
        virtual glm::vec3 GetAngularVelocity() const = 0;
        virtual bool IsSimulating() const = 0;
        virtual void SetSimulatePhysics(bool bSimulate) = 0;
        virtual void SetMass(float InMass) = 0;
        virtual float GetMass() const = 0;
        virtual void SetEnableGravity(bool bEnable) = 0;
        virtual void SetLinearDamping(float InDamping) = 0;
        virtual void SetAngularDamping(float InDamping) = 0;
        virtual void SetFriction(float InFriction) = 0;
        virtual void SetRestitution(float InRestitution) = 0;
        virtual void SetCollisionEnabled(ECollisionEnabled InEnabled) = 0;
        virtual void SetCollisionResponses(const FCollisionResponseContainer& InResponses) = 0;
        virtual void SetObjectType(ECollisionChannel InType) = 0;
        virtual AActor* GetActor() const = 0;
        virtual UActorComponent* GetComponent() const = 0;
        virtual ECollisionChannel GetObjectType() const = 0;
        virtual ECollisionResponse GetResponseToChannel(ECollisionChannel InChannel) const = 0;
        virtual ECollisionEnabled GetCollisionEnabled() const = 0;
        virtual EPhysicsMotionType GetMotionType() const = 0;
    };

    class IPhysicsConstraint {
    public:
        virtual ~IPhysicsConstraint() = default;
    };

    enum class EPhysicsConstraintType : uint8_t { Distance = 0, Fixed = 1, Hinge = 2, SwingTwist = 3 };

    struct FPhysicsConstraintCreateInfo {
        IPhysicsBody* BodyA = nullptr;
        IPhysicsBody* BodyB = nullptr;
        EPhysicsConstraintType Type = EPhysicsConstraintType::Distance;
        float RestLength = 0.0f;
        glm::vec3 Axis{0.0f, 1.0f, 0.0f};
        float Swing1LimitRadians = 0.7f;
        float Swing2LimitRadians = 0.7f;
        float TwistLimitRadians = 0.5f;
        float LinearDamping = 0.0f;
    };

    class IPhysicsScene {
    public:
        virtual ~IPhysicsScene() = default;
        virtual void SetWorld(class UWorld* InWorld) { (void)InWorld; }
        virtual void Tick(float InDeltaSeconds) = 0;
        virtual void SyncKinematicTransforms() {}
        virtual void SyncDynamicTransforms() {}
        virtual void DrainContacts() {}
        virtual void CreatePhysicsState(AActor* InActor) { (void)InActor; }
        virtual void DestroyPhysicsState(AActor* InActor) { (void)InActor; }
        virtual void NotifyBeginPlayFinished() {}
        virtual void IgnoreCollision(IPhysicsBody* InBodyA, IPhysicsBody* InBodyB) {
            (void)InBodyA;
            (void)InBodyB;
        }
        virtual int32_t GetRigidBodyCount() const = 0;

        virtual IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) = 0;
        virtual void DestroyRigidBody(IPhysicsBody* InBody) = 0;
        virtual IPhysicsConstraint* CreateConstraint(const FPhysicsConstraintCreateInfo& InInfo) = 0;
        virtual void DestroyConstraint(IPhysicsConstraint* InConstraint) = 0;

        virtual bool LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                              ECollisionChannel InChannel, AActor* InIgnore,
                                              FHitResult& OutHit) const = 0;
        virtual int32_t LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd,
                                                ECollisionChannel InChannel, AActor* InIgnore,
                                                std::vector<FHitResult>& OutHits) const = 0;
        virtual bool SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                          ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const = 0;
        virtual int32_t SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                            ECollisionChannel InChannel, AActor* InIgnore,
                                            std::vector<FHitResult>& OutHits) const = 0;
        /** Unreal-style capsule: InHalfHeight is half of total height including hemispheres. */
        virtual bool SweepCapsuleSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                 float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                                 FHitResult& OutHit) const = 0;
        virtual int32_t SweepCapsuleMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                                   float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                                   std::vector<FHitResult>& OutHits) const = 0;
        virtual bool OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                             ECollisionChannel InChannel, AActor* InIgnore) const = 0;
        virtual int32_t OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                              ECollisionChannel InChannel, AActor* InIgnore,
                                              std::vector<FHitResult>& OutHits) const = 0;
    };

    class FPhysicsModule {
    public:
        using FCreateScene = std::function<TRef<IPhysicsScene>()>;
        static void Register(FCreateScene InFactory);
        static void Unregister();
        static TRef<IPhysicsScene> CreateScene();
        static bool IsRegistered();
    };

} // namespace Leon
