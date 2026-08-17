#include "ALeonTournamentPlayerState.hpp"

namespace Leon {

    ALeonTournamentPlayerState::ALeonTournamentPlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APlayerState(InHandle, InWorld, InName) {
        SetClass("ALeonTournamentPlayerState");
    }

    void ALeonTournamentPlayerState::AddKill() {
        if (!IsNetworkAuthority())
            return;
        ++Kills;
        SetScore(static_cast<float>(Kills));
    }

    void ALeonTournamentPlayerState::AddDeath() {
        if (!IsNetworkAuthority())
            return;
        ++Deaths;
    }

    void ALeonTournamentPlayerState::AddAssist() {
        if (!IsNetworkAuthority())
            return;
        ++Assists;
    }

    void ALeonTournamentPlayerState::SetTeam(ELeonTournamentTeam InTeam) {
        if (!IsNetworkAuthority())
            return;
        Team = InTeam;
    }

    void ALeonTournamentPlayerState::SetIsBot(bool bInBot) {
        if (!IsNetworkAuthority())
            return;
        bBot = bInBot;
    }

    void ALeonTournamentPlayerState::SetCharacterSkin(ELeonTournamentCharacterSkin InSkin) {
        if (!IsNetworkAuthority())
            return;
        CharacterSkin = InSkin;
    }

    void ALeonTournamentPlayerState::ResetStats() {
        if (!IsNetworkAuthority())
            return;
        Kills = 0;
        Deaths = 0;
        Assists = 0;
        SetScore(0.0f);
    }

    void ALeonTournamentPlayerState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteI32(OutBytes, Kills);
        FNetBlob::WriteI32(OutBytes, Deaths);
        FNetBlob::WriteI32(OutBytes, Assists);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(Team));
        FNetBlob::WriteU8(OutBytes, bBot ? 1 : 0);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(CharacterSkin));
    }

    void ALeonTournamentPlayerState::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t team = 0, bot = 0, skin = 0;
        if (!FNetBlob::ReadI32(bytes, offset, Kills) || !FNetBlob::ReadI32(bytes, offset, Deaths) ||
            !FNetBlob::ReadI32(bytes, offset, Assists) || !FNetBlob::ReadU8(bytes, offset, team) ||
            !FNetBlob::ReadU8(bytes, offset, bot))
            return;
        Team = static_cast<ELeonTournamentTeam>(team);
        bBot = bot != 0;
        if (FNetBlob::ReadU8(bytes, offset, skin))
            CharacterSkin = static_cast<ELeonTournamentCharacterSkin>(skin);
        SetScore(static_cast<float>(Kills));
    }

} // namespace Leon
