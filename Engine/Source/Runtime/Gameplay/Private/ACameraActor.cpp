#include "Gameplay/ACameraActor.hpp"

namespace Leon {

    ACameraActor::ACameraActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ACameraActor");
    }

    void ACameraActor::PostInitializeComponents() {
        if (!HasComponent<FCameraComponent>()) {
            FPerspectiveCamera camera(45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
            AddComponent<FCameraComponent>(camera);
        }
    }

} // namespace Leon
