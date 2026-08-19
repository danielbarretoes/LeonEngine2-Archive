#include "Gameplay/AGameState.hpp"
#include "Engine/FNetBlob.hpp"

#include <algorithm>

namespace Leon {

    AGameState::AGameState(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AGameStateBase(InHandle, InWorld, InName) {
        SetClass("AGameState");
        SetReplicates(true);
        SetAlwaysRelevant(true);
    }

    void AGameState::HandleMatchStateChange(EMatchState InPrevious, EMatchState InCurrent) {
        (void)InPrevious;
        (void)InCurrent;
    }

    void AGameState::SetMatchState(EMatchState InState) {
        if (!IsNetworkAuthority())
            return;
        if (MatchState == InState)
            return;
        const EMatchState previous = MatchState;
        MatchState = InState;
        HandleMatchStateChange(previous, MatchState);
    }

    void AGameState::SetRemainingTime(float InTime) {
        if (!IsNetworkAuthority())
            return;
        RemainingTime = InTime;
    }

    void AGameState::Tick(float DeltaSeconds) {
        AGameStateBase::Tick(DeltaSeconds);
        // Authority owns the clock; SimulatedProxy extrapolates until the next snapshot.
        if (MatchState != EMatchState::InProgress || RemainingTime <= 0.0f)
            return;
        RemainingTime = std::max(0.0f, RemainingTime - DeltaSeconds);
    }

    void AGameState::SerializeReplication(std::vector<uint8_t>& OutBytes) const {
        FNetBlob::WriteU8(OutBytes, static_cast<uint8_t>(MatchState));
        FNetBlob::WriteF32(OutBytes, RemainingTime);
        FNetBlob::WriteF32(OutBytes, GetElapsedTime());
    }

    void AGameState::DeserializeReplication(const uint8_t* InData, size_t InSize) {
        std::vector<uint8_t> bytes(InData, InData + InSize);
        size_t offset = 0;
        uint8_t state = 0;
        float elapsed = 0.0f;
        if (!FNetBlob::ReadU8(bytes, offset, state) || !FNetBlob::ReadF32(bytes, offset, RemainingTime) ||
            !FNetBlob::ReadF32(bytes, offset, elapsed))
            return;
        const EMatchState incoming = static_cast<EMatchState>(state);
        SetElapsedTime(elapsed);
        if (MatchState != incoming) {
            const EMatchState previous = MatchState;
            MatchState = incoming;
            HandleMatchStateChange(previous, MatchState);
        }
    }

} // namespace Leon
