#include "Gameplay/USceneComponent.hpp"
#include "Gameplay/AActor.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    namespace {
        /** Rotate a local offset by parent yaw (degrees, Y-up) — Unreal-lite ComponentToWorld for planar attach. */
        glm::vec3 RotateOffsetByYaw(const glm::vec3& InOffset, float InYawDegrees) {
            const float rad = glm::radians(InYawDegrees);
            const float c = std::cos(rad);
            const float s = std::sin(rad);
            return {InOffset.x * c + InOffset.z * s, InOffset.y, -InOffset.x * s + InOffset.z * c};
        }
    } // namespace

    USceneComponent::USceneComponent(const std::string& InName) : UActorComponent(InName) {}

    USceneComponent::~USceneComponent() {
        DetachFromComponent();
        for (USceneComponent* child : AttachChildren) {
            if (child && child->AttachParent == this)
                child->AttachParent = nullptr;
        }
        AttachChildren.clear();
    }

    void USceneComponent::SetupAttachment(USceneComponent* InParent) {
        if (InParent == this)
            return;
        DetachFromComponent();
        AttachParent = InParent;
        if (AttachParent)
            AttachParent->AttachChildren.push_back(this);
    }

    void USceneComponent::DetachFromComponent() {
        if (!AttachParent)
            return;
        auto& siblings = AttachParent->AttachChildren;
        siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        AttachParent = nullptr;
    }

    glm::vec3 USceneComponent::GetComponentLocation() const {
        if (AttachParent) {
            const glm::vec3 parentLoc = AttachParent->GetComponentLocation();
            const float parentYaw = AttachParent->GetComponentRotation().y;
            return parentLoc + RotateOffsetByYaw(RelativeLocation, parentYaw);
        }

        if (Owner) {
            // Root (or unattached): actor transform is world pose; relative must stay identity for RootComponent.
            return Owner->GetActorLocation() + RelativeLocation;
        }
        return RelativeLocation;
    }

    glm::vec3 USceneComponent::GetComponentRotation() const {
        if (AttachParent)
            return AttachParent->GetComponentRotation() + RelativeRotation;
        if (Owner)
            return Owner->GetActorRotation() + RelativeRotation;
        return RelativeRotation;
    }

} // namespace Leon
