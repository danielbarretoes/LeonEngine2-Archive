#pragma once

#include "Gameplay/AActor.hpp"
#include "Gameplay/APlayerCameraManager.hpp"
#include "Gameplay/APlayerState.hpp"
#include "Gameplay/APawn.hpp"

namespace Leon {

    class AHUD;

    enum class EInputMode { GameOnly, UIOnly, GameAndUI };

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

        APawn* GetPawn() const { return Pawn; }
        template <typename T> T* GetPawn() const { return dynamic_cast<T*>(Pawn); }

        APlayerState* GetPlayerState() const { return PlayerState; }
        void SetPlayerState(APlayerState* InPlayerState) { PlayerState = InPlayerState; }

        APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }
        void SetPlayerCameraManager(APlayerCameraManager* InManager) { PlayerCameraManager = InManager; }

        AHUD* GetHUD() const { return MyHUD; }
        template <typename T> T* GetHUD() const { return dynamic_cast<T*>(MyHUD); }
        void SetHUD(AHUD* InHUD) { MyHUD = InHUD; }

        // --- FInput Mode & Cursor ---
        void SetInputModeGameOnly();
        void SetInputModeUIOnly();
        void SetInputModeGameAndUI();
        EInputMode GetInputMode() const { return InputMode; }

        void SetShowMouseCursor(bool bShow);
        bool ShouldShowMouseCursor() const { return bShowMouseCursor; }

        /** @brief True when gameplay (pawn) input should be processed. */
        bool IsGameInputAllowed() const {
            return InputMode == EInputMode::GameOnly || InputMode == EInputMode::GameAndUI;
        }

        /** @brief True when UI widgets should receive mouse events. */
        bool IsUIInputAllowed() const {
            return InputMode == EInputMode::UIOnly || InputMode == EInputMode::GameAndUI;
        }

        void SetViewTarget(AActor* InNewTarget);
        AActor* GetViewTarget() const;

        void UpdateCameraManager(float DeltaSeconds);
        void GetPlayerViewPoint(FPerspectiveCamera& OutCamera) const;

    protected:
        APawn* Pawn = nullptr;
        APlayerState* PlayerState = nullptr;
        APlayerCameraManager* PlayerCameraManager = nullptr;
        AHUD* MyHUD = nullptr;
        EInputMode InputMode = EInputMode::GameAndUI;
        bool bShowMouseCursor = true;
    };

} // namespace Leon
