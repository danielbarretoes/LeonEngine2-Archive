#pragma once

#include "Gameplay/AActor.hpp"
#include "Renderer/FPerspectiveCamera.hpp"

namespace Leon {

    class APlayerController;

    /**
     * @brief Unreal Engine aligned PlayerCameraManager responsible for determining the final view of a player.
     *
     * It manages view targets, blends, and resolves whether the camera comes from:
     * 1. Possessed APawn camera
     * 2. Active ViewTarget / ACameraActor placed in the level
     * 3. Fallback level view
     */
    class APlayerCameraManager : public AActor {
    public:
        APlayerCameraManager() = default;
        APlayerCameraManager(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PlayerCameraManager");
        ~APlayerCameraManager() override = default;

        void InitializeFor(APlayerController* InPC);
        APlayerController* GetPlayerController() const { return PlayerController; }
        void SetViewTarget(AActor* InNewTarget, float InBlendTime = 0.0f);
        AActor* GetViewTarget() const { return ViewTarget; }
        bool IsBlendingViewTarget() const { return bBlending; }

        void UpdateCamera(float DeltaSeconds);

        const FPerspectiveCamera& GetCamera() const { return Camera; }
        FPerspectiveCamera& GetCamera() { return Camera; }

        void SetAspectRatio(float InAspect);

    private:
        APlayerController* PlayerController = nullptr;
        AActor* ViewTarget = nullptr;
        FPerspectiveCamera Camera{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
        FPerspectiveCamera BlendFrom{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
        float BlendDuration = 0.0f;
        float BlendElapsed = 0.0f;
        bool bBlending = false;
    };

} // namespace Leon
