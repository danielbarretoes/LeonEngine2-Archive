#pragma once

#include "Core/Base.hpp"
#include <glm/glm.hpp>
#include <string>

namespace Leon {

    class USoundWave;

    /**
     * @brief Global audio output device (Unreal UAudioDevice lite).
     * Null/stub mode when Init fails or LEON_AUDIO_NULL=1 (headless tests).
     */
    class FAudioDevice {
    public:
        static FAudioDevice& Get();

        bool Init();
        void Shutdown();
        void Tick(float InDeltaSeconds);

        bool IsInitialized() const { return bInitialized; }
        bool IsNullDevice() const { return bNullDevice; }

        void SetMasterVolume(float InVolume);
        float GetMasterVolume() const { return MasterVolume; }

        void SetListener(const glm::vec3& InLocation, const glm::vec3& InForward, const glm::vec3& InUp);

        /** Fire-and-forget 2D one-shot. Safe no-op on null device. */
        void PlaySound2D(const TRef<USoundWave>& InSound, float InVolume = 1.0f);

        /** Looping 2D music track; replaces any previous music voice. */
        void PlayMusic2D(const TRef<USoundWave>& InSound, float InVolume = 1.0f);
        void StopMusic();
        bool IsMusicPlaying() const { return MusicVoiceId != 0; }

        /** Fire-and-forget 3D one-shot with distance attenuation. */
        void PlaySoundAtLocation(const TRef<USoundWave>& InSound, const glm::vec3& InLocation, float InVolume = 1.0f,
                                 float InAttenuationRadius = 2500.0f);

        /** Low-level: play decoded PCM already owned by USoundWave. Returns voice id or 0. */
        uint32_t PlayWave(const TRef<USoundWave>& InSound, float InVolume, bool bSpatial, const glm::vec3& InLocation,
                          float InAttenuationRadius, bool bLoop = false);
        void StopVoice(uint32_t InVoiceId);

    private:
        FAudioDevice() = default;
        bool bInitialized = false;
        bool bNullDevice = true;
        float MasterVolume = 1.0f;
        glm::vec3 ListenerLocation{0.0f};
        glm::vec3 ListenerForward{0.0f, 0.0f, -1.0f};
        glm::vec3 ListenerUp{0.0f, 1.0f, 0.0f};
        uint32_t NextVoiceId = 1;
        uint32_t MusicVoiceId = 0;
    };

} // namespace Leon
