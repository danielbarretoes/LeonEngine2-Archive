#pragma once

#include "Gameplay/UObject.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Sound asset (Unreal USoundWave lite). Resolves virtual/physical path; playback reads the file via
     * miniaudio.
     */
    class USoundWave : public UObject {
    public:
        explicit USoundWave(const std::string& InName = "SoundWave");
        ~USoundWave() override = default;

        bool LoadFromFile(const std::string& InPhysicalPath);
        bool LoadFromVirtualPath(const std::string& InVirtualPath);

        bool IsValid() const { return !AssetPath.empty() && SampleRate > 0 && Channels > 0; }

        const std::vector<float>& GetSamples() const { return Samples; }
        uint32_t GetSampleRate() const { return SampleRate; }
        uint32_t GetChannels() const { return Channels; }
        const std::string& GetAssetPath() const { return AssetPath; }

        /** Cached load by virtual or physical path (shared across PlaySound calls). */
        static TRef<USoundWave> Load(const std::string& InVirtualOrPhysicalPath);

    private:
        std::vector<float> Samples;
        uint32_t SampleRate = 0;
        uint32_t Channels = 0;
        std::string AssetPath;
    };

} // namespace Leon
