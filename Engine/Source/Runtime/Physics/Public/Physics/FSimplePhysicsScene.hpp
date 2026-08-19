#pragma once

#include "Physics/FCollisionQuery.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <memory>
#include <vector>

namespace Leon {

    class FSimplePhysicsScene;

    class FSimplePhysicsBody final : public IPhysicsBody {
    public:
        FSimplePhysicsScene* Scene = nullptr;
        FPhysicsBodyCreateInfo Info;
        glm::vec3 Location{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 LinearVelocity{0.0f};
        glm::vec3 AngularVelocity{0.0f};
        glm::vec3 PendingForce{0.0f};
        glm::vec3 PendingImpulse{0.0f};
        glm::vec3 PendingTorque{0.0f};
        glm::vec3 PendingAngularImpulse{0.0f};
        bool bSimulating = false;

        void Destroy() override;
        void SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) override;
        void GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const override;
        void AddForce(const glm::vec3& InForce) override;
        void AddImpulse(const glm::vec3& InImpulse) override;
        void AddTorque(const glm::vec3& InTorque) override;
        void AddAngularImpulse(const glm::vec3& InImpulse) override;
        void SetLinearVelocity(const glm::vec3& InVelocity) override;
        void SetAngularVelocity(const glm::vec3& InVelocity) override;
        glm::vec3 GetLinearVelocity() const override;
        glm::vec3 GetAngularVelocity() const override;
        bool IsSimulating() const override;
        void SetSimulatePhysics(bool bSimulate) override;
        void SetMass(float InMass) override;
        float GetMass() const override;
        void SetEnableGravity(bool bEnable) override;
        void SetLinearDamping(float InDamping) override;
        void SetAngularDamping(float InDamping) override;
        void SetFriction(float InFriction) override;
        void SetRestitution(float InRestitution) override;
        void SetCollisionEnabled(ECollisionEnabled InEnabled) override;
        void SetCollisionResponses(const FCollisionResponseContainer& InResponses) override;
        void SetObjectType(ECollisionChannel InType) override;
        AActor* GetActor() const override;
        UActorComponent* GetComponent() const override;
        ECollisionChannel GetObjectType() const override;
        ECollisionResponse GetResponseToChannel(ECollisionChannel InChannel) const override;
        ECollisionEnabled GetCollisionEnabled() const override;
        EPhysicsMotionType GetMotionType() const override;
    };

    class FSimplePhysicsConstraint final : public IPhysicsConstraint {
    public:
        FSimplePhysicsBody* BodyA = nullptr;
        FSimplePhysicsBody* BodyB = nullptr;
        float RestLength = 0.0f;
    };

    /**
     * Engine fallback geometric scene used when the Jolt plugin is not registered.
     */
    class FSimplePhysicsScene : public IPhysicsScene {
    public:
        void SetWorld(class UWorld* InWorld) override { World = InWorld; }
        UWorld* GetWorld() const { return World; }

        void Tick(float InDeltaSeconds) override;
        void SyncKinematicTransforms() override;
        void SyncDynamicTransforms() override;
        int32_t GetRigidBodyCount() const override;

        IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) override;
        void DestroyRigidBody(IPhysicsBody* InBody) override;
        IPhysicsConstraint* CreateConstraint(const FPhysicsConstraintCreateInfo& InInfo) override;
        void DestroyConstraint(IPhysicsConstraint* InConstraint) override;

        bool LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                      AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                        AActor* InIgnore, std::vector<FHitResult>& OutHits) const override;
        bool SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                  ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                    ECollisionChannel InChannel, AActor* InIgnore,
                                    std::vector<FHitResult>& OutHits) const override;
        bool SweepCapsuleSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                         float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                         FHitResult& OutHit) const override;
        int32_t SweepCapsuleMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                           float InHalfHeight, ECollisionChannel InChannel, AActor* InIgnore,
                                           std::vector<FHitResult>& OutHits) const override;
        bool OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent, ECollisionChannel InChannel,
                                     AActor* InIgnore) const override;
        int32_t OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                      ECollisionChannel InChannel, AActor* InIgnore,
                                      std::vector<FHitResult>& OutHits) const override;

    private:
        void CollectColliders(AActor* InIgnore, std::vector<FColliderDesc>& Out) const;
        void RebuildStaticColliderCache() const;
        void Integrate(float InDeltaSeconds);

        UWorld* World = nullptr;
        std::vector<std::unique_ptr<FSimplePhysicsBody>> Bodies;
        std::vector<std::unique_ptr<FSimplePhysicsConstraint>> Constraints;
        mutable std::vector<FColliderDesc> CachedStaticColliders;
        mutable size_t CachedActorCount = 0;
        mutable bool bStaticCacheValid = false;
        float PhysicsAccumulator = 0.0f;
    };

} // namespace Leon
