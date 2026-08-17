#pragma once

#include "Core/Base.hpp"
#include "Physics/ECollisionTypes.hpp"
#include "Physics/FHitResult.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <vector>

namespace Leon {

    class AActor;
    class UActorComponent;

    struct FPhysicsBodyCreateInfo {
        EPhysicsShapeType Shape = EPhysicsShapeType::Box;
        EPhysicsMotionType Motion = EPhysicsMotionType::Static;
        ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
        FCollisionResponseContainer Responses;
        glm::vec3 Location{0.0f};
        glm::quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 BoxHalfExtent{0.5f};
        float SphereRadius = 0.5f;
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 0.95f;
        float Mass = 1.0f;
        float LinearDamping = 0.01f;
        float AngularDamping = 0.05f;
        float Restitution = 0.0f;
        float Friction = 0.7f;
        bool bEnableGravity = true;
        bool bSimulatePhysics = false;
        AActor* Actor = nullptr;
        UActorComponent* Component = nullptr;
    };

    class IPhysicsBody {
    public:
        virtual ~IPhysicsBody() = default;
        virtual void SetTransform(const glm::vec3& InLocation, const glm::quat& InRotation) = 0;
        virtual void GetTransform(glm::vec3& OutLocation, glm::quat& OutRotation) const = 0;
        virtual void AddForce(const glm::vec3& InForce) = 0;
        virtual void AddImpulse(const glm::vec3& InImpulse) = 0;
        virtual void SetLinearVelocity(const glm::vec3& InVelocity) = 0;
        virtual void SetAngularVelocity(const glm::vec3& InVelocity) = 0;
        virtual glm::vec3 GetLinearVelocity() const = 0;
        virtual glm::vec3 GetAngularVelocity() const = 0;
        virtual bool IsSimulating() const = 0;
        virtual void SetSimulatePhysics(bool bSimulate) = 0;
        virtual AActor* GetActor() const = 0;
        virtual UActorComponent* GetComponent() const = 0;
        virtual ECollisionChannel GetObjectType() const = 0;
        virtual ECollisionResponse GetResponseToChannel(ECollisionChannel InChannel) const = 0;
    };

    class IPhysicsScene {
    public:
        virtual ~IPhysicsScene() = default;
        virtual void SetWorld(class UWorld* InWorld) { (void)InWorld; }
        virtual void Tick(float InDeltaSeconds) = 0;

        virtual IPhysicsBody* CreateRigidBody(const FPhysicsBodyCreateInfo& InInfo) = 0;
        virtual void DestroyRigidBody(IPhysicsBody* InBody) = 0;

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
        static TRef<IPhysicsScene> CreateScene();
        static bool IsRegistered();
    };

} // namespace Leon
