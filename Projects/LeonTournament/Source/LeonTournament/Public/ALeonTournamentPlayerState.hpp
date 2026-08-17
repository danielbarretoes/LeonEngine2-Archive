#pragma once

#include "Gameplay/APlayerState.hpp"
#include "FLeonTournamentTypes.hpp"
#include "Engine/FNetBlob.hpp"

namespace Leon {

    class ALeonTournamentPlayerState : public APlayerState {
    public:
        ALeonTournamentPlayerState() = default;
        ALeonTournamentPlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "LeonTournamentPlayerState");

        int32_t GetKills() const { return Kills; }
        int32_t GetDeaths() const { return Deaths; }
        int32_t GetAssists() const { return Assists; }
        ELeonTournamentTeam GetTeam() const { return Team; }
        bool IsBot() const { return bBot; }

        void AddKill();
        void AddDeath();
        void AddAssist();
        void SetTeam(ELeonTournamentTeam InTeam);
        void SetIsBot(bool bInBot);
        void ResetStats();

        void SerializeReplication(std::vector<uint8_t>& OutBytes) const override;
        void DeserializeReplication(const uint8_t* InData, size_t InSize) override;

    private:
        int32_t Kills = 0;
        int32_t Deaths = 0;
        int32_t Assists = 0;
        ELeonTournamentTeam Team = ELeonTournamentTeam::None;
        bool bBot = false;
    };

} // namespace Leon
