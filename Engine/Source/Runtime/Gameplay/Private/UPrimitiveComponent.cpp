#include "Gameplay/UPrimitiveComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/UWorld.hpp"
#include "Physics/IPhysicsScene.hpp"

#include <algorithm>
#include <glm/gtc/quaternion.hpp>

namespace Leon {

    UPrimitiveComponent::UPrimitiveComponent(const std::string& InName) : USceneComponent(InName) {}

    UPrimitiveComponent::~UPrimitiveComponent() {
        UnregisterPhysics();
    }

    void UPrimitiveComponent::BeginPlay() {
        RegisterPhysics();
    }

    void UPrimitiveComponent::EndPlay() {
        UnregisterPhysics();
        OnComponentBeginOverlap.clear();
        OnComponentEndOverlap.clear();
        OverlappingComponents.clear();
    }

    void UPrimitiveComponent::Tick(float DeltaSeconds) {
        (void)DeltaSeconds;
        if (!PhysicsBody)
            return;
        if (Owner && Owner->GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        if (!bSimulatePhysics)
            SyncPhysicsTransform();
    }

    void UPrimitiveComponent::UpdateOverlaps(const std::vector<UPrimitiveComponent*>& InCandidates) {
        if (!bGenerateOverlapEvents || CollisionEnabled == ECollisionEnabled::NoCollision || !Owner)
            return;

        const glm::vec3 myPos = GetComponentLocation();
        const glm::vec3 myHalf = GetOverlapQueryHalfExtent();
        const glm::vec3 myMin = myPos - myHalf;
        const glm::vec3 myMax = myPos + myHalf;

        std::unordered_set<UPrimitiveComponent*> now;
        now.reserve(InCandidates.size());

        for (UPrimitiveComponent* other : InCandidates) {
            if (!other || other == this || !other->GetOwner() || other->GetOwner() == Owner)
                continue;
            if (other->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
                continue;

            const glm::vec3 oPos = other->GetComponentLocation();
            const glm::vec3 oHalf = other->GetOverlapQueryHalfExtent();
            const glm::vec3 oMin = oPos - oHalf;
            const glm::vec3 oMax = oPos + oHalf;
            const bool overlaps = (myMin.x <= oMax.x && myMax.x >= oMin.x) && (myMin.y <= oMax.y && myMax.y >= oMin.y) &&
                                  (myMin.z <= oMax.z && myMax.z >= oMin.z);
            if (!overlaps)
                continue;

            now.insert(other);
            if (OverlappingComponents.find(other) == OverlappingComponents.end()) {
                FHitResult hit;
                hit.Actor = other->GetOwner();
                hit.Component = other;
                hit.Location = 0.5f * (myPos + oPos);
                hit.ImpactPoint = hit.Location;
                hit.bBlockingHit = false;
                for (auto& cb : OnComponentBeginOverlap) {
                    if (cb)
                        cb(this, other->GetOwner(), other, hit);
                }
            }
        }

        for (UPrimitiveComponent* prev : OverlappingComponents) {
            if (!prev || now.find(prev) != now.end())
                continue;
            FHitResult hit;
            hit.Actor = prev->GetOwner();
            hit.Component = prev;
            hit.Location = GetComponentLocation();
            hit.ImpactPoint = hit.Location;
            for (auto& cb : OnComponentEndOverlap) {
                if (cb)
                    cb(this, prev->GetOwner(), prev, hit);
            }
        }

        OverlappingComponents = std::move(now);
    }

    FPhysicsBodyCreateInfo UPrimitiveComponent::MakeBodyCreateInfo() const {
        FPhysicsBodyCreateInfo info;
        info.Location = GetComponentLocation();
        info.ObjectType = ObjectType;
        info.Responses = Responses;
        info.Mass = Mass;
        info.LinearDamping = LinearDamping;
        info.AngularDamping = AngularDamping;
        info.Restitution = Restitution;
        info.Friction = Friction;
        info.bEnableGravity = bEnableGravity;
        info.bSimulatePhysics = bSimulatePhysics;
        info.Motion = bSimulatePhysics ? EPhysicsMotionType::Dynamic : EPhysicsMotionType::Kinematic;
        info.Actor = Owner;
        info.Component = const_cast<UPrimitiveComponent*>(this);
        return info;
    }

    void UPrimitiveComponent::RegisterPhysics() {
        if (PhysicsBody || !Owner || !Owner->GetWorld())
            return;
        IPhysicsScene* scene = Owner->GetWorld()->GetPhysicsScene();
        if (!scene)
            return;
        PhysicsBody = scene->CreateRigidBody(MakeBodyCreateInfo());
    }

    void UPrimitiveComponent::UnregisterPhysics() {
        if (!PhysicsBody || !Owner || !Owner->GetWorld()) {
            PhysicsBody = nullptr;
            return;
        }
        if (IPhysicsScene* scene = Owner->GetWorld()->GetPhysicsScene())
            scene->DestroyRigidBody(PhysicsBody);
        PhysicsBody = nullptr;
    }

    void UPrimitiveComponent::SyncPhysicsTransform() {
        if (!PhysicsBody)
            return;
        const glm::vec3 rot = GetComponentRotation();
        const glm::quat q = glm::quat(glm::radians(rot));
        PhysicsBody->SetTransform(GetComponentLocation(), q);
    }

    void UPrimitiveComponent::SetCollisionEnabled(ECollisionEnabled InEnabled) {
        CollisionEnabled = InEnabled;
    }

    void UPrimitiveComponent::SetCollisionObjectType(ECollisionChannel InType) {
        ObjectType = InType;
    }

    void UPrimitiveComponent::SetCollisionResponseToChannel(ECollisionChannel InChannel,
                                                            ECollisionResponse InResponse) {
        Responses.Set(InChannel, InResponse);
    }

    ECollisionResponse UPrimitiveComponent::GetCollisionResponseToChannel(ECollisionChannel InChannel) const {
        return Responses.Get(InChannel);
    }

    void UPrimitiveComponent::SetCollisionResponseToAllChannels(ECollisionResponse InResponse) {
        for (uint8_t i = 0; i < kCollisionChannelCount; ++i)
            Responses.Set(static_cast<ECollisionChannel>(i), InResponse);
    }

    void UPrimitiveComponent::SetCollisionProfileName(const std::string& InProfile) {
        CollisionProfile = InProfile;
        if (InProfile == "NoCollision") {
            SetCollisionEnabled(ECollisionEnabled::NoCollision);
        } else if (InProfile == "OverlapAll") {
            SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
        } else if (InProfile == "Pawn") {
            SetCollisionObjectType(ECollisionChannel::Pawn);
            SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SetCollisionResponseToAllChannels(ECollisionResponse::Block);
            // Movement sweeps WorldStatic. Other pawns must not answer that query or
            // clustered spawns pin everyone in place. Walls still block as WorldStatic.
            SetCollisionResponseToChannel(ECollisionChannel::WorldStatic, ECollisionResponse::Ignore);
            SetCollisionResponseToChannel(ECollisionChannel::Camera, ECollisionResponse::Ignore);
        } else {
            SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            SetCollisionResponseToAllChannels(ECollisionResponse::Block);
        }
    }

    void UPrimitiveComponent::SetSimulatePhysics(bool bSimulate) {
        bSimulatePhysics = bSimulate;
        if (PhysicsBody)
            PhysicsBody->SetSimulatePhysics(bSimulate);
    }

    void UPrimitiveComponent::AddImpulse(const glm::vec3& InImpulse) {
        if (PhysicsBody)
            PhysicsBody->AddImpulse(InImpulse);
    }

    void UPrimitiveComponent::SetEnableGravity(bool bEnable) {
        bEnableGravity = bEnable;
    }

    void UPrimitiveComponent::SetMass(float InMass) {
        Mass = std::max(InMass, 0.001f);
    }

    UShapeComponent::UShapeComponent(const std::string& InName) : UPrimitiveComponent(InName) {}

    UBoxComponent::UBoxComponent(const std::string& InName) : UShapeComponent(InName) {
        SetCollisionObjectType(ECollisionChannel::WorldStatic);
    }

    glm::vec3 UBoxComponent::GetOverlapQueryHalfExtent() const {
        const glm::vec3 scale = Owner ? Owner->GetActorScale() : glm::vec3(1.0f);
        return BoxExtent * scale;
    }

    FPhysicsBodyCreateInfo UBoxComponent::MakeBodyCreateInfo() const {
        FPhysicsBodyCreateInfo info = UPrimitiveComponent::MakeBodyCreateInfo();
        info.Shape = EPhysicsShapeType::Box;
        glm::vec3 scale = Owner ? Owner->GetActorScale() : glm::vec3(1.0f);
        info.BoxHalfExtent = BoxExtent * scale;
        return info;
    }

    USphereComponent::USphereComponent(const std::string& InName) : UShapeComponent(InName) {}

    glm::vec3 USphereComponent::GetOverlapQueryHalfExtent() const {
        const float scale = Owner ? std::max(Owner->GetActorScale().x, 0.001f) : 1.0f;
        const float r = SphereRadius * scale;
        return glm::vec3(r);
    }

    FPhysicsBodyCreateInfo USphereComponent::MakeBodyCreateInfo() const {
        FPhysicsBodyCreateInfo info = UPrimitiveComponent::MakeBodyCreateInfo();
        info.Shape = EPhysicsShapeType::Sphere;
        float scale = Owner ? std::max(Owner->GetActorScale().x, 0.001f) : 1.0f;
        info.SphereRadius = SphereRadius * scale;
        return info;
    }

    UCapsuleComponent::UCapsuleComponent(const std::string& InName) : UShapeComponent(InName) {
        SetCollisionObjectType(ECollisionChannel::Pawn);
    }

    void UCapsuleComponent::SetCapsuleSize(float InRadius, float InHalfHeight) {
        CapsuleRadius = InRadius;
        CapsuleHalfHeight = std::max(InHalfHeight, InRadius);
    }

    glm::vec3 UCapsuleComponent::GetOverlapQueryHalfExtent() const {
        return {CapsuleRadius, CapsuleHalfHeight, CapsuleRadius};
    }

    FPhysicsBodyCreateInfo UCapsuleComponent::MakeBodyCreateInfo() const {
        FPhysicsBodyCreateInfo info = UPrimitiveComponent::MakeBodyCreateInfo();
        info.Shape = EPhysicsShapeType::Capsule;
        info.CapsuleRadius = CapsuleRadius;
        info.CapsuleHalfHeight = CapsuleHalfHeight;
        if (!bSimulatePhysics)
            info.Motion = EPhysicsMotionType::Kinematic;
        return info;
    }

} // namespace Leon
