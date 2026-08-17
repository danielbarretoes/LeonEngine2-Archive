#include "Audio/FAudioDevice.hpp"
#include "Audio/USoundWave.hpp"
#include "Core/FLog.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <unordered_map>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace Leon {
namespace {

    struct FActiveVoice {
        uint32_t Id = 0;
        ma_sound Sound{};
        bool bValid = false;
    };

    ma_engine* GEngine = nullptr;
    std::mutex GVoiceMutex;
    std::unordered_map<uint32_t, FActiveVoice> GVoices;

    float DistanceAttenuation(float InDistance, float InRadius) {
        if (InRadius <= 1.0f)
            return 1.0f;
        const float t = std::clamp(InDistance / InRadius, 0.0f, 1.0f);
        return (1.0f - t) * (1.0f - t);
    }

} // namespace

    FAudioDevice& FAudioDevice::Get() {
        static FAudioDevice Instance;
        return Instance;
    }

    bool FAudioDevice::Init() {
        if (bInitialized)
            return true;

        const char* nullEnv = std::getenv("LEON_AUDIO_NULL");
        if (nullEnv && nullEnv[0] == '1') {
            bNullDevice = true;
            bInitialized = true;
            LE_CORE_INFO("FAudioDevice: null device (LEON_AUDIO_NULL=1)");
            return true;
        }

        GEngine = new ma_engine();
        ma_engine_config config = ma_engine_config_init();
        ma_result result = ma_engine_init(&config, GEngine);
        if (result != MA_SUCCESS) {
            delete GEngine;
            GEngine = nullptr;
            bNullDevice = true;
            bInitialized = true;
            LE_CORE_WARN("FAudioDevice: ma_engine_init failed ({0}); using null device", static_cast<int>(result));
            return true;
        }

        bNullDevice = false;
        bInitialized = true;
        ma_engine_set_volume(GEngine, MasterVolume);
        LE_CORE_INFO("FAudioDevice: initialized (miniaudio)");
        return true;
    }

    void FAudioDevice::Shutdown() {
        {
            std::lock_guard<std::mutex> lock(GVoiceMutex);
            for (auto& [id, voice] : GVoices) {
                if (voice.bValid) {
                    ma_sound_uninit(&voice.Sound);
                    voice.bValid = false;
                }
            }
            GVoices.clear();
        }
        if (GEngine) {
            ma_engine_uninit(GEngine);
            delete GEngine;
            GEngine = nullptr;
        }
        bInitialized = false;
        bNullDevice = true;
    }

    void FAudioDevice::Tick(float /*InDeltaSeconds*/) {
        if (bNullDevice || !GEngine)
            return;

        std::lock_guard<std::mutex> lock(GVoiceMutex);
        std::vector<uint32_t> finished;
        for (auto& [id, voice] : GVoices) {
            if (!voice.bValid)
                continue;
            if (!ma_sound_is_playing(&voice.Sound)) {
                ma_sound_uninit(&voice.Sound);
                voice.bValid = false;
                finished.push_back(id);
            }
        }
        for (uint32_t id : finished)
            GVoices.erase(id);
    }

    void FAudioDevice::SetMasterVolume(float InVolume) {
        MasterVolume = std::clamp(InVolume, 0.0f, 1.0f);
        if (GEngine && !bNullDevice)
            ma_engine_set_volume(GEngine, MasterVolume);
    }

    void FAudioDevice::SetListener(const glm::vec3& InLocation, const glm::vec3& InForward, const glm::vec3& InUp) {
        ListenerLocation = InLocation;
        ListenerForward = InForward;
        ListenerUp = InUp;
        if (GEngine && !bNullDevice) {
            ma_engine_listener_set_position(GEngine, 0, InLocation.x, InLocation.y, InLocation.z);
            ma_engine_listener_set_direction(GEngine, 0, InForward.x, InForward.y, InForward.z);
            ma_engine_listener_set_world_up(GEngine, 0, InUp.x, InUp.y, InUp.z);
        }
    }

    void FAudioDevice::PlaySound2D(const TRef<USoundWave>& InSound, float InVolume) {
        PlayWave(InSound, InVolume, false, glm::vec3(0.0f), 0.0f);
    }

    void FAudioDevice::PlaySoundAtLocation(const TRef<USoundWave>& InSound, const glm::vec3& InLocation, float InVolume,
                                           float InAttenuationRadius) {
        PlayWave(InSound, InVolume, true, InLocation, InAttenuationRadius);
    }

    uint32_t FAudioDevice::PlayWave(const TRef<USoundWave>& InSound, float InVolume, bool bSpatial,
                                    const glm::vec3& InLocation, float InAttenuationRadius) {
        if (!bInitialized || bNullDevice || !GEngine || !InSound || !InSound->IsValid())
            return 0;

        const std::string& path = InSound->GetAssetPath();
        if (path.empty())
            return 0;

        float volume = std::clamp(InVolume, 0.0f, 2.0f);
        if (bSpatial) {
            const float dist = glm::length(InLocation - ListenerLocation);
            volume *= DistanceAttenuation(dist, InAttenuationRadius);
            if (volume < 0.01f)
                return 0;
        }

        const uint32_t id = NextVoiceId++;
        std::lock_guard<std::mutex> lock(GVoiceMutex);
        FActiveVoice& voice = GVoices[id];
        voice.Id = id;
        voice.bValid = false;

        ma_result result = ma_sound_init_from_file(GEngine, path.c_str(), 0, nullptr, nullptr, &voice.Sound);
        if (result != MA_SUCCESS) {
            GVoices.erase(id);
            LE_CORE_WARN("FAudioDevice: failed to play '{0}' ({1})", path, static_cast<int>(result));
            return 0;
        }

        ma_sound_set_volume(&voice.Sound, volume);
        if (bSpatial) {
            ma_sound_set_spatialization_enabled(&voice.Sound, MA_TRUE);
            ma_sound_set_position(&voice.Sound, InLocation.x, InLocation.y, InLocation.z);
        } else {
            ma_sound_set_spatialization_enabled(&voice.Sound, MA_FALSE);
        }

        result = ma_sound_start(&voice.Sound);
        if (result != MA_SUCCESS) {
            ma_sound_uninit(&voice.Sound);
            GVoices.erase(id);
            return 0;
        }

        voice.bValid = true;
        return id;
    }

    void FAudioDevice::StopVoice(uint32_t InVoiceId) {
        if (InVoiceId == 0 || bNullDevice)
            return;
        std::lock_guard<std::mutex> lock(GVoiceMutex);
        auto it = GVoices.find(InVoiceId);
        if (it == GVoices.end())
            return;
        if (it->second.bValid) {
            ma_sound_stop(&it->second.Sound);
            ma_sound_uninit(&it->second.Sound);
            it->second.bValid = false;
        }
        GVoices.erase(it);
    }

} // namespace Leon
