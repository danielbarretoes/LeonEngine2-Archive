#include "ALeonTournamentCaptureTheFlagGameMode.hpp"

namespace Leon {

    ALeonTournamentCaptureTheFlagGameMode::ALeonTournamentCaptureTheFlagGameMode(entt::entity InHandle, UWorld* InWorld,
                                                                                 const std::string& InName)
        : ALeonTournamentGameMode(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentCaptureTheFlagGameMode");
        SetActiveGameMode(ELeonTournamentGameModeId::CaptureTheFlag);
    }

    void ALeonTournamentCaptureTheFlagGameMode::InitGame() {
        ALeonTournamentGameMode::InitGame();
        SetActiveGameMode(ELeonTournamentGameModeId::CaptureTheFlag);
    }

} // namespace Leon
