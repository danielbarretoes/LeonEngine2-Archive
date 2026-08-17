#include "Audio/USoundWave.hpp"
#include "Core/FLog.hpp"
#include "Core/FProjectPaths.hpp"

#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace Leon {

    namespace {
        std::mutex GWaveCacheMutex;
        std::unordered_map<std::string, TRef<USoundWave>> GWaveCache;
    } // namespace

    USoundWave::USoundWave(const std::string& InName) : UObject(InName) {}

    bool USoundWave::LoadFromFile(const std::string& InPhysicalPath) {
        Samples.clear();
        SampleRate = 0;
        Channels = 0;
        AssetPath.clear();

        if (InPhysicalPath.empty() || !std::filesystem::exists(InPhysicalPath)) {
            LE_CORE_WARN("USoundWave: file not found '{0}'", InPhysicalPath);
            return false;
        }

        // Playback uses miniaudio from the file path once; avoid a second full PCM decode here.
        AssetPath = InPhysicalPath;
        SampleRate = 1;
        Channels = 1;
        return true;
    }

    bool USoundWave::LoadFromVirtualPath(const std::string& InVirtualPath) {
        const std::string physical = FProjectPaths::ResolveVirtualPath(InVirtualPath);
        if (LoadFromFile(physical)) {
            SetName(InVirtualPath);
            return true;
        }
        return false;
    }

    TRef<USoundWave> USoundWave::Load(const std::string& InVirtualOrPhysicalPath) {
        if (InVirtualOrPhysicalPath.empty())
            return nullptr;

        {
            std::lock_guard<std::mutex> lock(GWaveCacheMutex);
            auto it = GWaveCache.find(InVirtualOrPhysicalPath);
            if (it != GWaveCache.end())
                return it->second;
        }

        auto wave = CreateRef<USoundWave>();
        bool ok = false;
        if (InVirtualOrPhysicalPath.rfind("/Game/", 0) == 0 || InVirtualOrPhysicalPath.rfind("/Engine/", 0) == 0)
            ok = wave->LoadFromVirtualPath(InVirtualOrPhysicalPath);
        else
            ok = wave->LoadFromFile(InVirtualOrPhysicalPath);

        if (!ok)
            return nullptr;

        std::lock_guard<std::mutex> lock(GWaveCacheMutex);
        GWaveCache[InVirtualOrPhysicalPath] = wave;
        return wave;
    }

} // namespace Leon
