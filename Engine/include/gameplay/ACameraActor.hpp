#pragma once

#include "gameplay/AActor.hpp"

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

        UCameraComponent& GetCameraComponent() { return GetComponent<UCameraComponent>(); }
        const UCameraComponent& GetCameraComponent() const { return GetComponent<UCameraComponent>(); }
    };

} // namespace Leon
