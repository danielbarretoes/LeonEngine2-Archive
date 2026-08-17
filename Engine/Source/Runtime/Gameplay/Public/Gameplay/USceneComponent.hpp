#pragma once

#include "Gameplay/UActorComponent.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    /**
     * @brief Transform component in the actor hierarchy (Unreal USceneComponent lite).
     *
     * RootComponent has identity relative transform; its world pose equals the actor transform.
     * Attached children resolve world location through their AttachParent.
     */
    class USceneComponent : public UActorComponent {
    public:
        explicit USceneComponent(const std::string& InName = "SceneComponent");
        ~USceneComponent() override;

        void SetRelativeLocation(const glm::vec3& InLocation) { RelativeLocation = InLocation; }
        const glm::vec3& GetRelativeLocation() const { return RelativeLocation; }

        void SetRelativeRotation(const glm::vec3& InRotation) { RelativeRotation = InRotation; }
        const glm::vec3& GetRelativeRotation() const { return RelativeRotation; }

        void SetRelativeScale3D(const glm::vec3& InScale) { RelativeScale = InScale; }
        const glm::vec3& GetRelativeScale3D() const { return RelativeScale; }

        /** Attach to a parent scene component (Unreal SetupAttachment). */
        void SetupAttachment(USceneComponent* InParent);
        void AttachToComponent(USceneComponent* InParent) { SetupAttachment(InParent); }
        void DetachFromComponent();

        USceneComponent* GetAttachParent() const { return AttachParent; }
        const std::vector<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }

        /** World-space location (actor root + relative chain). */
        virtual glm::vec3 GetComponentLocation() const;
        virtual glm::vec3 GetComponentRotation() const;

        bool IsRegistered() const { return bRegistered; }
        void SetRegistered(bool bInRegistered) { bRegistered = bInRegistered; }

    protected:
        glm::vec3 RelativeLocation{0.0f};
        glm::vec3 RelativeRotation{0.0f};
        glm::vec3 RelativeScale{1.0f, 1.0f, 1.0f};
        USceneComponent* AttachParent = nullptr;
        std::vector<USceneComponent*> AttachChildren;
        bool bRegistered = false;
    };

} // namespace Leon
