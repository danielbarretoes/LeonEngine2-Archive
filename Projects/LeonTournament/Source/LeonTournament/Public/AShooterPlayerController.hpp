#pragma once

#include "Gameplay/APlayerController.hpp"

namespace Leon {

    class AShooterPlayerController : public APlayerController {
    public:
        AShooterPlayerController() = default;
        AShooterPlayerController(entt::entity InHandle, UWorld* InWorld,
                                 const std::string& InName = "ShooterPlayerController");

        void Tick(float DeltaSeconds) override;
        bool IsScoreboardHeld() const { return bScoreboardHeld; }
        bool ConsumeEscapePressed();

        void NotifyConfirmedHit(bool bKill);
        void NotifyTookDamage();

        bool IsHitMarkerActive() const { return HitMarkerRemaining > 0.0f; }
        bool IsKillConfirmActive() const { return KillConfirmRemaining > 0.0f; }
        bool IsDamageFlashActive() const { return DamageFlashRemaining > 0.0f; }

    private:
        bool bScoreboardHeld = false;
        bool bEscapeWasDown = false;
        bool bEscapePressed = false;
        float HitMarkerRemaining = 0.0f;
        float KillConfirmRemaining = 0.0f;
        float DamageFlashRemaining = 0.0f;
    };

} // namespace Leon
