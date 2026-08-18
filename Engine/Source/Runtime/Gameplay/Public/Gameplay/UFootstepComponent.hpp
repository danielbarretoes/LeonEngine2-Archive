#pragma once

#include "Gameplay/UActorComponent.hpp"

#include <algorithm>
#include <string>

namespace Leon {

    /**
     * Cadence footstep SFX on a moving grounded character. Sound path is set by the game.
     */
    class UFootstepComponent : public UActorComponent {
    public:
        UFootstepComponent(const std::string& InName = "FootstepComponent");

        void Tick(float DeltaSeconds) override;

        void SetSoundPath(const std::string& InPath) { SoundPath = InPath; }
        const std::string& GetSoundPath() const { return SoundPath; }
        void SetVolume(float InVolume) { Volume = std::max(InVolume, 0.0f); }
        void SetAttenuationRadius(float InRadius) { AttenuationRadius = std::max(InRadius, 1.0f); }
        void SetMinSpeed(float InSpeed) { MinSpeed = std::max(InSpeed, 0.0f); }
        float GetCooldownRemaining() const { return CooldownRemaining; }

    private:
        std::string SoundPath;
        float Volume = 0.45f;
        float AttenuationRadius = 1200.0f;
        float MinSpeed = 1.15f;
        float CooldownRemaining = 0.0f;
    };

} // namespace Leon
