#include "gameplay/ACameraActor.hpp"

namespace Leon {

    ACameraActor::ACameraActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void ACameraActor::PostInitializeComponents() {
        if (!HasComponent<UCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<UCameraComponent>(camera);
        }
    }

} // namespace Leon
