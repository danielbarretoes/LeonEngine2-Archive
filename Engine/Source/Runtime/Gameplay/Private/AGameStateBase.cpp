#include "Gameplay/AGameStateBase.hpp"
#include <algorithm>

namespace Leon {

    AGameStateBase::AGameStateBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void AGameStateBase::AddPlayerState(APlayerState* InPlayerState) {
        if (InPlayerState &&
            std::find(PlayerArray.begin(), PlayerArray.end(), InPlayerState) == PlayerArray.end()) {
            PlayerArray.push_back(InPlayerState);
        }
    }

    void AGameStateBase::RemovePlayerState(APlayerState* InPlayerState) {
        auto it = std::find(PlayerArray.begin(), PlayerArray.end(), InPlayerState);
        if (it != PlayerArray.end()) {
            PlayerArray.erase(it);
        }
    }

    void AGameStateBase::Tick(float DeltaSeconds) {
        ElapsedTime += DeltaSeconds;
    }

} // namespace Leon
