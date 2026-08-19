#include "Gameplay/AGameStateBase.hpp"
#include "Gameplay/AGameModeBase.hpp"
#include "Engine/UWorld.hpp"
#include <algorithm>

namespace Leon {

    AGameStateBase::AGameStateBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AGameStateBase");
    }

    AGameModeBase* AGameStateBase::GetGameMode() const {
        return World ? World->GetGameMode() : nullptr;
    }

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

    std::vector<APlayerState*> AGameStateBase::GetPlayerArraySortedByScore() const {
        std::vector<APlayerState*> ranked = PlayerArray;
        std::sort(ranked.begin(), ranked.end(), [](APlayerState* a, APlayerState* b) {
            if (!a)
                return false;
            if (!b)
                return true;
            if (a->GetScore() != b->GetScore())
                return a->GetScore() > b->GetScore();
            return a->GetPlayerName() < b->GetPlayerName();
        });
        return ranked;
    }

    void AGameStateBase::Tick(float DeltaSeconds) {
        if (GetLocalRole() == ENetRole::SimulatedProxy)
            return;
        ElapsedTime += DeltaSeconds;
    }

} // namespace Leon
