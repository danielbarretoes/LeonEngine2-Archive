#include "gameplay/AActor.hpp"
#include "world/UWorld.hpp"

namespace Leon {

    AActor::AActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : UObject(InName), m_EntityHandle(InHandle), m_World(InWorld) {}

    void AActor::SetName(const std::string& InName) {
        UObject::SetName(InName);
        if (HasComponent<FTagComponent>()) {
            GetComponent<FTagComponent>().Tag = InName;
        } else {
            AddComponent<FTagComponent>(InName);
        }
    }

    void AActor::Destroy() {
        if (m_World) {
            m_World->DestroyActor(this);
        }
    }

    FTransformComponent& AActor::GetTransform() {
        return GetComponent<FTransformComponent>();
    }

    const FTransformComponent& AActor::GetTransform() const {
        return GetComponent<FTransformComponent>();
    }

    glm::vec3 AActor::GetActorLocation() const {
        return GetTransform().Translation;
    }

    void AActor::SetActorLocation(const glm::vec3& InLocation) {
        GetTransform().Translation = InLocation;
    }

    glm::vec3 AActor::GetActorRotation() const {
        return GetTransform().Rotation;
    }

    void AActor::SetActorRotation(const glm::vec3& InRotation) {
        GetTransform().Rotation = InRotation;
    }

    glm::vec3 AActor::GetActorScale() const {
        return GetTransform().Scale;
    }

    void AActor::SetActorScale(const glm::vec3& InScale) {
        GetTransform().Scale = InScale;
    }

} // namespace Leon
