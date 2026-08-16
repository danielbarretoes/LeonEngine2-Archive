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

        const std::string& GetPlayerStartTag() const { return PlayerStartTag; }
        void SetPlayerStartTag(const std::string& InTag) { PlayerStartTag = InTag; }

    private:
        std::string PlayerStartTag;
    };

} // namespace Leon
