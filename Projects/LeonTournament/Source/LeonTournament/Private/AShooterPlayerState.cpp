#include "AShooterPlayerState.hpp"

namespace Leon {

    AShooterPlayerState::AShooterPlayerState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : APlayerState(InHandle, InWorld, InName) {
        SetClass("AShooterPlayerState");
    }

    void AShooterPlayerState::AddKill() {
        ++Kills;
        SetScore(static_cast<float>(Kills));
    }

    void AShooterPlayerState::AddDeath() {
        ++Deaths;
    }

    void AShooterPlayerState::AddAssist() {
        ++Assists;
    }

    void AShooterPlayerState::ResetStats() {
        Kills = 0;
        Deaths = 0;
        Assists = 0;
        SetScore(0.0f);
    }

    void AShooterPlayerState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteI32(OutBytes, Kills);
        FNetBlob::WriteI32(OutBytes, Deaths);
        FNetBlob::WriteI32(OutBytes, Assists);
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(Team));
        FNetBlob::WriteU8(OutBytes, bBot ? 1 : 0);
    }

    void AShooterPlayerState::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t team = 0, bot = 0;
        if (!FNetBlob::ReadI32(bytes, offset, Kills) || !FNetBlob::ReadI32(bytes, offset, Deaths) ||
            !FNetBlob::ReadI32(bytes, offset, Assists) || !FNetBlob::ReadU8(bytes, offset, team) ||
            !FNetBlob::ReadU8(bytes, offset, bot))
            return;
        Team = static_cast<EShooterTeam>(team);
        bBot = bot != 0;
        SetScore(static_cast<float>(Kills));
    }

} // namespace Leon
