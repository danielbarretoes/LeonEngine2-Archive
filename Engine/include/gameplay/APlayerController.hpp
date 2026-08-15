#pragma once

#include "gameplay/AActor.hpp"
#include "gameplay/APlayerCameraManager.hpp"
#include "gameplay/APlayerState.hpp"
#include "gameplay/APawn.hpp"

namespace Leon {

    /**
     * @brief Unreal Engine aligned PlayerController managing input, pawn possession, and the camera manager.
     */
    class APlayerController : public AActor {
    public:
        APlayerController() = default;
        APlayerController(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PlayerController");
        ~APlayerController() override = default;

        void PostInitializeComponents() override;
        void Tick(float DeltaSeconds) override;

        virtual void Possess(APawn* InPawn);
        virtual void UnPossess();

        APawn* GetPawn() const { return m_Pawn; }
        template <typename T> T* GetPawn() const { return dynamic_cast<T*>(m_Pawn); }

        APlayerState* GetPlayerState() const { return m_PlayerState; }
        void SetPlayerState(APlayerState* InPlayerState) { m_PlayerState = InPlayerState; }

        APlayerCameraManager* GetPlayerCameraManager() const { return m_PlayerCameraManager; }
        void SetPlayerCameraManager(APlayerCameraManager* InManager) { m_PlayerCameraManager = InManager; }

        void SetViewTarget(AActor* InNewTarget);
        AActor* GetViewTarget() const;

        void UpdateCameraManager(float DeltaSeconds);
        void GetPlayerViewPoint(FPerspectiveCamera& OutCamera) const;

    protected:
        APawn* m_Pawn = nullptr;
        APlayerState* m_PlayerState = nullptr;
        APlayerCameraManager* m_PlayerCameraManager = nullptr;
    };

} // namespace Leon
