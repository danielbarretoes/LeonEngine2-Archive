#pragma once

#include "Gameplay/AGameModeBase.hpp"

namespace Leon {

    /**
     * @brief Sandbox project GameMode — configures HUD and default gameplay classes.
     */
    class ASandboxGameMode : public AGameModeBase {
    public:
        ASandboxGameMode() = default;
        ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxGameMode");

        void InitGame() override;
    };

} // namespace Leon
