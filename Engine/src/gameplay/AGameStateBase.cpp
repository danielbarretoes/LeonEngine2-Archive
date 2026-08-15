#include "gameplay/AGameStateBase.hpp"
#include <algorithm>

namespace Leon {

    AGameStateBase::AGameStateBase(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {}

    void AGameStateBase::AddPlayerState(APlayerState* InPlayerState) {
        if (InPlayerState && std::find(m_PlayerArray.begin(), m_PlayerArray.end(), InPlayerState) == m_PlayerArray.end()) {
            m_PlayerArray.push_back(InPlayerState);
        }
    }

    void AGameStateBase::RemovePlayerState(APlayerState* InPlayerState) {
        auto it = std::find(m_PlayerArray.begin(), m_PlayerArray.end(), InPlayerState);
        if (it != m_PlayerArray.end()) {
            m_PlayerArray.erase(it);
        }
    }

    void AGameStateBase::Tick(float DeltaSeconds) {
        m_ElapsedTime += DeltaSeconds;
    }

} // namespace Leon
