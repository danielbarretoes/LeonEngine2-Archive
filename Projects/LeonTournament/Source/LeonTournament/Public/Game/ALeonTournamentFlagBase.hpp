#pragma once

#include "Gameplay/AActor.hpp"
#include "FLeonTournamentTypes.hpp"

namespace Leon {

    /** Home capture zone for one team in CTF. */
    class ALeonTournamentFlagBase : public AActor {
    public:
        ALeonTournamentFlagBase() = default;
        ALeonTournamentFlagBase(entt::entity InHandle, UWorld* InWorld,
                                const std::string& InName = "LeonTournamentFlagBase");

        void BeginPlay() override;

        ELeonTournamentTeam GetTeam() const { return Team; }
        void SetTeam(ELeonTournamentTeam InTeam) { Team = InTeam; }
        float GetCaptureRadius() const { return CaptureRadius; }
        void SetCaptureRadius(float InRadius) { CaptureRadius = InRadius; }

        bool IsCharacterInside(const class ALeonTournamentCharacter& InCharacter) const;
        void BuildVisual();

    private:
        ELeonTournamentTeam Team = ELeonTournamentTeam::None;
        float CaptureRadius = 3.5f;
    };

} // namespace Leon
