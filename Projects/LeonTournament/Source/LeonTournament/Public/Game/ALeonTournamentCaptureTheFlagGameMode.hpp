#pragma once

#include "ALeonTournamentGameMode.hpp"

namespace Leon {

    /** CTF rules layered on LeonTournament match flow (warmup, rematch, bots). */
    class ALeonTournamentCaptureTheFlagGameMode : public ALeonTournamentGameMode {
    public:
        ALeonTournamentCaptureTheFlagGameMode() = default;
        ALeonTournamentCaptureTheFlagGameMode(entt::entity InHandle, UWorld* InWorld,
                                              const std::string& InName = "LeonTournamentCaptureTheFlagGameMode");

        void InitGame() override;
    };

} // namespace Leon
