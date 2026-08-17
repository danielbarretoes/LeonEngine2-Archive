#pragma once

#include "Gameplay/UActorComponent.hpp"
#include <glm/glm.hpp>

namespace Leon {

    class USoundWave;

    /**
     * @brief Actor-attached sound player (Unreal UAudioComponent lite).
     */
    class UAudioComponent : public UActorComponent {
    public:
        explicit UAudioComponent(const std::string& InName = "AudioComponent");
        ~UAudioComponent() override;

        void SetSound(const TRef<USoundWave>& InSound);
        const TRef<USoundWave>& GetSound() const { return Sound; }

        void SetVolumeMultiplier(float InVolume) { VolumeMultiplier = InVolume; }
        float GetVolumeMultiplier() const { return VolumeMultiplier; }

        void SetbSpatialized(bool bInSpatial) { bSpatialized = bInSpatial; }
        void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

        void Play();
        void Stop();
        bool IsPlaying() const { return ActiveVoiceId != 0; }

        void Tick(float DeltaSeconds) override;
        void EndPlay() override;

    private:
        TRef<USoundWave> Sound;
        float VolumeMultiplier = 1.0f;
        bool bSpatialized = false;
        float AttenuationRadius = 2500.0f;
        uint32_t ActiveVoiceId = 0;
    };

} // namespace Leon
