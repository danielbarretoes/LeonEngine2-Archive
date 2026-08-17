#pragma once

#include "Physics/FCollisionQuery.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <memory>
#include <vector>

namespace Leon {

    class FSimplePhysicsBody final : public IPhysicsBody {
    public:
        FPhysicsBodyCreateInfo Info;
        glm::vec3 Location{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 LinearVelocity{0.0f};
        glm::vec3 AngularVelocity{0.0f};
        glm::vec3 PendingForce{0.0f};
        glm::vec3 PendingImpulse{0.0f};
        bool bSimulating = false;

        void SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) override;
        void GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const override;
        void AddForce(const glm::vec3& InForce) override;
        void AddImpulse(const glm::vec3& InImpulse) override;
        void SetLinearVelocity(const glm::vec3& InVelocity) override;
        void SetAngularVelocity(const glm::vec3& InVelocity) override;
        glm::vec3 GetLinearVelocity() const override;
        glm::vec3 GetAngularVelocity() const override;
        bool IsSimulating() const override;
        void SetSimulatePhysics(bool bSimulate) override;
        AActor* GetActor() const override;
        UActorComponent* GetComponent() const override;
        ECollisionChannel GetObjectType() const override;
        ECollisionResponse GetResponseToChannel(ECollisionChannel InChannel) const override;
    };

    /**
     * Engine fallback / canonical geometric scene. The Jolt plugin replaces this factory
     * when registered; gameplay only talks to IPhysicsScene.
     */
    class FSimplePhysicsScene : public IPhysicsScene {
    public:
        void SetWorld(class UWorld* InWorld) override { World = InWorld; }
        UWorld* GetWorld() const { return World; }

        void Tick(float InDeltaSeconds) override;
        IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) override;
        void DestroyRigidBody(IPhysicsBody* InBody) override;

        bool LineTraceSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                      AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t LineTraceMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, ECollisionChannel InChannel,
                                        AActor* InIgnore, std::vector<FHitResult>& OutHits) const override;
        bool SweepSingleByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                  ECollisionChannel InChannel, AActor* InIgnore, FHitResult& OutHit) const override;
        int32_t SweepMultiByChannel(const glm::vec3& InStart, const glm::vec3& InEnd, float InRadius,
                                    ECollisionChannel InChannel, AActor* InIgnore,
                                    std::vector<FHitResult>& OutHits) const override;
        bool OverlapAnyTestByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent, ECollisionChannel InChannel,
                                     AActor* InIgnore) const override;
        int32_t OverlapMultiByChannel(const glm::vec3& InPos, const glm::vec3& InHalfExtent,
                                      ECollisionChannel InChannel, AActor* InIgnore,
                                      std::vector<FHitResult>& OutHits) const override;

    private:
        void CollectColliders(AActor* InIgnore, std::vector<FColliderDesc>& Out) const;

        UWorld* World = nullptr;
        std::vector<std::unique_ptr<FSimplePhysicsBody>> Bodies;
    };

} // namespace Leon
