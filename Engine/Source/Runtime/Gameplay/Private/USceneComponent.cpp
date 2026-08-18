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
               glm::toMat4(glm::quat(glm::radians(RelativeRotation))) *
               glm::scale(glm::mat4(1.0f), RelativeScale);
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

} // namespace Leon
