#pragma once

#include "Gameplay/AGameModeBase.hpp"

namespace Leon {

    /**
     * Sandbox GameMode — fly spectator, HUD chip, and runtime demos of engine features
     * that the baked Showcase/Night maps do not spawn themselves (planar planes, APickup).
     */
    class ASandboxGameMode : public AGameModeBase {
    public:
        ASandboxGameMode() = default;
        ASandboxGameMode(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "SandboxGameMode");

        void InitGame() override;
        void StartPlay() override;

    private:
        void SetupPlanarReflections();
        void SpawnShowcaseDemos();
        void SpawnNightDemos();
    };

} // namespace Leon
