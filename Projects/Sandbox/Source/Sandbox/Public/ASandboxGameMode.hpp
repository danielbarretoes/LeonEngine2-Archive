#pragma once

#include "Gameplay/AGameMode.hpp"

namespace Leon {

    /**
     * Sandbox GameMode — fly spectator, floor planar capture, and an APickup demo.
     */
    class ASandboxGameMode : public AGameMode {
    public:
        ASandboxGameMode() = default;
        ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxGameMode");

        void InitGame() override;
        void StartPlay() override;

    private:
        void SetupPlanarReflections();
        void SpawnShowcaseDemos();
    };

} // namespace Leon
