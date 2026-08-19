#pragma once

#include "FLeonTournamentTypes.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace Leon {

    /** Unified Unreal Tournament–inspired palette for all LeonTournament UI. */
    struct FLeonTournamentUITheme {
        // Surfaces
        static constexpr glm::vec4 VoidBg{0.01f, 0.012f, 0.022f, 0.94f};
        static constexpr glm::vec4 PanelBg{0.028f, 0.032f, 0.052f, 0.90f};
        static constexpr glm::vec4 PanelBgStrong{0.035f, 0.040f, 0.065f, 0.96f};
        static constexpr glm::vec4 HudBarBg{0.012f, 0.016f, 0.028f, 0.72f};
        static constexpr glm::vec4 DimOverlay{0.0f, 0.0f, 0.0f, 0.74f};

        // Accent
        static constexpr glm::vec4 AccentOrange{1.0f, 0.48f, 0.06f, 1.0f};
        static constexpr glm::vec4 AccentGold{1.0f, 0.74f, 0.18f, 1.0f};
        static constexpr glm::vec4 AccentCyan{0.35f, 0.88f, 1.0f, 0.95f};

        // Typography
        static constexpr glm::vec4 TextPrimary{0.96f, 0.97f, 1.0f, 1.0f};
        static constexpr glm::vec4 TextSecondary{0.62f, 0.68f, 0.82f, 0.92f};
        static constexpr glm::vec4 TextMuted{0.46f, 0.52f, 0.64f, 0.88f};
        static constexpr glm::vec4 TextAccent{1.0f, 0.62f, 0.22f, 1.0f};

        // Teams
        static constexpr glm::vec4 Team1{1.0f, 0.30f, 0.22f, 1.0f};
        static constexpr glm::vec4 Team2{0.22f, 0.58f, 1.0f, 1.0f};

        // Gameplay feedback
        static constexpr glm::vec4 HealthHigh{0.22f, 0.96f, 0.38f, 1.0f};
        static constexpr glm::vec4 HealthMid{1.0f, 0.82f, 0.28f, 1.0f};
        static constexpr glm::vec4 HealthLow{1.0f, 0.32f, 0.28f, 1.0f};
        static constexpr glm::vec4 Ammo{0.78f, 0.86f, 1.0f, 1.0f};
        static constexpr glm::vec4 KillGold{1.0f, 0.78f, 0.18f, 1.0f};
        static constexpr glm::vec4 Banner{1.0f, 0.58f, 0.08f, 1.0f};
        static constexpr glm::vec4 DamageFlash{0.92f, 0.06f, 0.06f, 0.28f};
        static constexpr glm::vec4 Win{0.28f, 1.0f, 0.42f, 1.0f};
        static constexpr glm::vec4 Loss{1.0f, 0.28f, 0.22f, 1.0f};
        static constexpr glm::vec4 Draw{0.82f, 0.86f, 0.94f, 1.0f};

        // Buttons
        static constexpr glm::vec4 BtnNormal{0.07f, 0.09f, 0.15f, 0.96f};
        static constexpr glm::vec4 BtnHover{0.28f, 0.14f, 0.04f, 1.0f};
        static constexpr glm::vec4 BtnPressed{0.14f, 0.08f, 0.04f, 1.0f};
        static constexpr glm::vec4 BtnSelected{0.42f, 0.20f, 0.04f, 1.0f};

        static glm::vec4 TeamColor(ELeonTournamentTeam InTeam) {
            if (InTeam == ELeonTournamentTeam::Team2)
                return Team2;
            if (InTeam == ELeonTournamentTeam::Team1)
                return Team1;
            return TextSecondary;
        }

        static glm::vec4 HealthColor(float InPct) {
            if (InPct < 0.3f)
                return HealthLow;
            if (InPct < 0.6f)
                return HealthMid;
            return HealthHigh;
        }

        static glm::vec4 ResultColor(const std::string& InResult) {
            if (InResult == "WIN")
                return Win;
            if (InResult == "LOSS")
                return Loss;
            return Draw;
        }
    };

} // namespace Leon
