#pragma once

#include "Gameplay/APlayerController.hpp"

#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class ALeonTournamentPlayerController : public APlayerController {
    public:
        ALeonTournamentPlayerController() = default;
        ALeonTournamentPlayerController(entt::entity InHandle, UWorld* InWorld,
                                        const std::string& InName = "LeonTournamentPlayerController");

        void Tick(float DeltaSeconds) override;
        bool IsScoreboardHeld() const { return bScoreboardHeld; }
        bool ConsumeEscapePressed();

        void NotifyConfirmedHit(bool bKill);
        void NotifyTookDamage();
        void NotifyLocalDeath();
        void PushBanner(const std::string& InText, float InSeconds, const glm::vec4& InColor);
        void ClearBanner();

        bool IsHitMarkerActive() const { return HitMarkerRemaining > 0.0f; }
        bool IsKillConfirmActive() const { return KillConfirmRemaining > 0.0f; }
        bool IsDamageFlashActive() const { return DamageFlashRemaining > 0.0f; }
        bool IsBannerActive() const { return BannerRemaining > 0.0f; }
        const std::string& GetBannerText() const { return BannerText; }
        const glm::vec4& GetBannerColor() const { return BannerColor; }
        const std::string& GetKillFeedText() const { return KillFeedText; }

        bool IsPauseMenuOpen() const { return bPauseMenuOpen; }
        void SetPauseMenuOpen(bool bOpen);
        void TogglePauseMenu();

    private:
        bool bScoreboardHeld = false;
        bool bEscapeWasDown = false;
        bool bEscapePressed = false;
        bool bPauseMenuOpen = false;
        float HitMarkerRemaining = 0.0f;
        float KillConfirmRemaining = 0.0f;
        float DamageFlashRemaining = 0.0f;
        float BannerRemaining = 0.0f;
        float KillStreakWindowRemaining = 0.0f;
        int KillStreak = 0;
        std::string BannerText;
        glm::vec4 BannerColor{1.0f};
        std::string KillFeedText;
    };

} // namespace Leon
