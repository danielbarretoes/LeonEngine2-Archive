#pragma once

#include "Gameplay/AActor.hpp"

namespace Leon {

    /**
     * @brief Dedicated world-settings actor. Bake knobs live on FWorldSettingsComponent.
     */
    class AWorldSettings : public AActor {
    public:
        AWorldSettings() = default;
        AWorldSettings(entt::entity InHandle, UWorld* InWorld, const std::string& InName = "WorldSettings");
        ~AWorldSettings() override = default;

        void PostInitializeComponents() override;

        FWorldSettingsComponent& GetWorldSettings();
        const FWorldSettingsComponent& GetWorldSettings() const;
    };

} // namespace Leon
