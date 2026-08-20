#include "Gameplay/USceneComponent.hpp"
#include "Gameplay/AActor.hpp"

#include <algorithm>
#include <cmath>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Leon {

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

    glm::mat4 USceneComponent::GetRelativeMatrix() const {
        return glm::translate(glm::mat4(1.0f), RelativeLocation) *
               glm::toMat4(glm::quat(glm::radians(RelativeRotation))) * glm::scale(glm::mat4(1.0f), RelativeScale);
    }

    glm::mat4 USceneComponent::GetComponentWorldMatrix() const {
        if (AttachParent)
            return AttachParent->GetComponentWorldMatrix() * GetRelativeMatrix();
        if (Owner)
            return Owner->GetTransform().GetTransform() * GetRelativeMatrix();
        return GetRelativeMatrix();
    }

    glm::vec3 USceneComponent::GetComponentLocation() const {
        return glm::vec3(GetComponentWorldMatrix()[3]);
    }

    glm::vec3 USceneComponent::GetComponentRotation() const {
        if (AttachParent)
            return AttachParent->GetComponentRotation() + RelativeRotation;
        if (Owner)
            return Owner->GetActorRotation() + RelativeRotation;
        return RelativeRotation;
    }

    glm::vec3 USceneComponent::GetComponentScale() const {
        if (AttachParent)
            return AttachParent->GetComponentScale() * RelativeScale;
        if (Owner)
            return Owner->GetActorScale() * RelativeScale;
        return RelativeScale;
    }

    void USceneComponent::SetWorldLocationAndRotation(const glm::vec3& InLocation, const glm::vec3& InEulerDegrees) {
        // Flow: physics → component
        // 1. RootComponent: actor owns world pose (relative stays identity)
        // 2. Attached: solve relative from parent world inverse
        // 3. Unattached non-root: relative to actor transform
        if (!AttachParent && Owner && Owner->GetRootComponent() == this) {
            Owner->SetActorLocation(InLocation);
            Owner->SetActorRotation(InEulerDegrees);
            RelativeLocation = glm::vec3(0.0f);
            RelativeRotation = glm::vec3(0.0f);
            return;
        }
        if (AttachParent) {
            const glm::mat4 invParent = glm::inverse(AttachParent->GetComponentWorldMatrix());
            RelativeLocation = glm::vec3(invParent * glm::vec4(InLocation, 1.0f));
            RelativeRotation = InEulerDegrees - AttachParent->GetComponentRotation();
            return;
        }
        if (Owner) {
            RelativeLocation = InLocation - Owner->GetActorLocation();
            RelativeRotation = InEulerDegrees - Owner->GetActorRotation();
            return;
        }
        RelativeLocation = InLocation;
        RelativeRotation = InEulerDegrees;
    }

} // namespace Leon
