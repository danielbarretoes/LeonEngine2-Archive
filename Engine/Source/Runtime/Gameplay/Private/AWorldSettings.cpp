#include "Gameplay/AWorldSettings.hpp"

namespace Leon {

    AWorldSettings::AWorldSettings(entt::entity InHandle, UWorld* InWorld, const std::string& InName)
        : AActor(InHandle, InWorld, InName) {
        SetClass("AWorldSettings");
    }

    void AWorldSettings::PostInitializeComponents() {
        if (!HasComponent<FWorldSettingsComponent>()) {
            AddComponent<FWorldSettingsComponent>();
        }
    }

    FWorldSettingsComponent& AWorldSettings::GetWorldSettings() {
        return GetComponent<FWorldSettingsComponent>();
    }

    const FWorldSettingsComponent& AWorldSettings::GetWorldSettings() const {
        return GetComponent<FWorldSettingsComponent>();
    }

} // namespace Leon
