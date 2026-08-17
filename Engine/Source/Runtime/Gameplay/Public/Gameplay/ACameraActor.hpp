#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned CameraActor placed in maps to provide cinematic or static viewpoints.
     */
    class ACameraActor : public AActor {
    public:
        ACameraActor() = default;
        ACameraActor(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "CameraActor");
        ~ACameraActor() override = default;

        void PostInitializeComponents() override;

        FCameraComponent& GetCameraComponent() { return GetComponent<FCameraComponent>(); }
        const FCameraComponent& GetCameraComponent() const { return GetComponent<FCameraComponent>(); }
    };

} // namespace Leon
