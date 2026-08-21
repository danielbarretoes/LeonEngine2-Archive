#include "Gameplay/ASkyLight.hpp"
#include "Engine/Components.hpp"

namespace Leon {

    ASkyLight::ASkyLight(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("ASkyLight");
    }

    void ASkyLight::PostInitializeComponents() {
        if (!HasComponent<FSkyboxComponent>()) {
            FSkyboxComponent sky;
            sky.bEnabled = true;
            AddComponent<FSkyboxComponent>(sky);
        }
        SetCanEverTick(false);
    }

} // namespace Leon
