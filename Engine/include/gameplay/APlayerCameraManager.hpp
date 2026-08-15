#pragma once

#include "gameplay/AActor.hpp"
#include "renderer/PerspectiveCamera.hpp"

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
        APlayerController* GetPlayerController() const { return m_PlayerController; }
        void SetViewTarget(AActor* InNewTarget);
        AActor* GetViewTarget() const { return m_ViewTarget; }

        void UpdateCamera(float DeltaSeconds);

        const FPerspectiveCamera& GetCamera() const { return m_Camera; }
        FPerspectiveCamera& GetCamera() { return m_Camera; }

        void SetAspectRatio(float InAspect);

    private:
        APlayerController* m_PlayerController = nullptr;
        AActor* m_ViewTarget = nullptr;
        FPerspectiveCamera m_Camera{45.0f, 1280.0f / 720.0f, 0.1f, 1000.0f};
    };

} // namespace Leon
