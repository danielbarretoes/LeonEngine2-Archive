#include "Gameplay/AActor.hpp"
#include "Engine/UWorld.hpp"

namespace Leon {

    AActor::AActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : UObject(InName), EntityHandle(InHandle), World(InWorld) {}

    void AActor::ExecuteBeginPlay() {
        BeginPlay();
        for (auto& comp : ActorComponents) {
            if (comp && !comp->HasBegunPlay()) {
                comp->BeginPlay();
                comp->MarkBegunPlay();
            }
        }
    }

    void AActor::ExecuteTick(float DeltaSeconds) {
        Tick(DeltaSeconds);
        for (auto& comp : ActorComponents) {
            if (comp && comp->IsComponentTickEnabled()) {
                comp->Tick(DeltaSeconds);
            }
        }
    }

    void AActor::ExecuteEndPlay() {
        for (auto& comp : ActorComponents) {
            if (comp) {
                comp->EndPlay();
            }
        }
        EndPlay();
        ActorComponents.clear();
    }

    void AActor::SetName(const std::string& InName) {
        UObject::SetName(InName);
        if (HasComponent<FTagComponent>()) {
            GetComponent<FTagComponent>().Tag = InName;
        } else {
            AddComponent<FTagComponent>(InName);
        }
    }

    void AActor::Destroy() {
        if (World) {
            World->DestroyActor(this);
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
