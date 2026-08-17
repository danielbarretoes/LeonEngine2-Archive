#pragma once

#include "Gameplay/UActorComponent.hpp"
#include "Physics/ECollisionTypes.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class IPhysicsBody;

    /**
     * Scene component with collision / physics. Gameplay never includes Jolt headers.
     */
    class UPrimitiveComponent : public UActorComponent {
    public:
        UPrimitiveComponent(const std::string& InName = "PrimitiveComponent");
        ~UPrimitiveComponent() override;

        void BeginPlay() override;
        void EndPlay() override;
        void Tick(float DeltaSeconds) override;

        void SetCollisionEnabled(ECollisionEnabled InEnabled);
        ECollisionEnabled GetCollisionEnabled() const { return CollisionEnabled; }

        void SetCollisionObjectType(ECollisionChannel InType);
        ECollisionChannel GetCollisionObjectType() const { return ObjectType; }

        void SetCollisionResponseToChannel(ECollisionChannel InChannel, ECollisionResponse InResponse);
        ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel InChannel) const;
        void SetCollisionResponseToAllChannels(ECollisionResponse InResponse);

        void SetCollisionProfileName(const std::string& InProfile);
        const std::string& GetCollisionProfileName() const { return CollisionProfile; }

        void SetGenerateOverlapEvents(bool bEnabled) { bGenerateOverlapEvents = bEnabled; }
        bool GetGenerateOverlapEvents() const { return bGenerateOverlapEvents; }

        void SetSimulatePhysics(bool bSimulate);
        bool IsSimulatingPhysics() const { return bSimulatePhysics; }

        void SetEnableGravity(bool bEnable);
        bool IsGravityEnabled() const { return bEnableGravity; }

        void SetMass(float InMass);
        float GetMass() const { return Mass; }
        void SetLinearDamping(float InDamping) { LinearDamping = InDamping; }
        float GetLinearDamping() const { return LinearDamping; }
        void SetAngularDamping(float InDamping) { AngularDamping = InDamping; }
        float GetAngularDamping() const { return AngularDamping; }
        void SetRestitution(float InValue) { Restitution = InValue; }
        float GetRestitution() const { return Restitution; }
        void SetFriction(float InValue) { Friction = InValue; }
        float GetFriction() const { return Friction; }

        IPhysicsBody* GetPhysicsBody() const { return PhysicsBody; }
        void UnregisterPhysics();
        void RegisterPhysics();
        void SyncPhysicsTransform();

        void SetRelativeLocation(const glm::vec3& InLocation) { RelativeLocation = InLocation; }
        const glm::vec3& GetRelativeLocation() const { return RelativeLocation; }

        glm::vec3 GetComponentLocation() const;

        virtual FPhysicsBodyCreateInfo MakeBodyCreateInfo() const;

    protected:
        glm::vec3 RelativeLocation{0.0f};
        ECollisionEnabled CollisionEnabled = ECollisionEnabled::QueryAndPhysics;
        ECollisionChannel ObjectType = ECollisionChannel::WorldStatic;
        FCollisionResponseContainer Responses;
        std::string CollisionProfile = "BlockAll";
        bool bGenerateOverlapEvents = false;
        bool bSimulatePhysics = false;
        bool bEnableGravity = true;
        float Mass = 1.0f;
        float LinearDamping = 0.01f;
        float AngularDamping = 0.05f;
        float Restitution = 0.0f;
        float Friction = 0.7f;
        IPhysicsBody* PhysicsBody = nullptr;
    };

    class UShapeComponent : public UPrimitiveComponent {
    public:
        UShapeComponent(const std::string& InName = "ShapeComponent");
    };

    class UBoxComponent : public UShapeComponent {
    public:
        UBoxComponent(const std::string& InName = "BoxComponent");
        void SetBoxExtent(const glm::vec3& InExtent) { BoxExtent = InExtent; }
        const glm::vec3& GetBoxExtent() const { return BoxExtent; }
        FPhysicsBodyCreateInfo MakeBodyCreateInfo() const override;

    private:
        glm::vec3 BoxExtent{0.5f};
    };

    class USphereComponent : public UShapeComponent {
    public:
        USphereComponent(const std::string& InName = "SphereComponent");
        void SetSphereRadius(float InRadius) { SphereRadius = InRadius; }
        float GetSphereRadius() const { return SphereRadius; }
        FPhysicsBodyCreateInfo MakeBodyCreateInfo() const override;

    private:
        float SphereRadius = 0.5f;
    };

    class UCapsuleComponent : public UShapeComponent {
    public:
        UCapsuleComponent(const std::string& InName = "CapsuleComponent");
        void SetCapsuleSize(float InRadius, float InHalfHeight);
        float GetUnscaledCapsuleRadius() const { return CapsuleRadius; }
        float GetUnscaledCapsuleHalfHeight() const { return CapsuleHalfHeight; }
        FPhysicsBodyCreateInfo MakeBodyCreateInfo() const override;

    private:
        float CapsuleRadius = 0.4f;
        float CapsuleHalfHeight = 0.95f;
    };

} // namespace Leon
