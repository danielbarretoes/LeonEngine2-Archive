#pragma once

#include "Gameplay/AActor.hpp"

#include <string>

namespace Leon {

    /**
     * @brief Level-placed spawn marker. Login uses ChoosePlayerStart instead of a hardcoded location.
     */
    class APlayerStart : public AActor {
    public:
        APlayerStart() = default;
        APlayerStart(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "PlayerStart");
        ~APlayerStart() override = default;

        void Tick(float DeltaSeconds) override;

        const std::string& GetPlayerStartTag() const { return PlayerStartTag; }
        void SetPlayerStartTag(const std::string& InTag) { PlayerStartTag = InTag; }

        int32_t GetTeamIndex() const { return TeamIndex; }
        void SetTeamIndex(int32_t InTeam) { TeamIndex = InTeam; }
        bool IsEnabled() const { return bEnabled; }
        void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

    private:
        std::string PlayerStartTag;
        int32_t TeamIndex = 0;
        bool bEnabled = true;
    };

} // namespace Leon
