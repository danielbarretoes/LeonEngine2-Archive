#pragma once

#include <string>

namespace Leon {

    struct FMemoryStats {
        size_t WorkingSetBytes = 0;     // Current physical RAM in use by this process
        size_t PeakWorkingSetBytes = 0; // Peak physical RAM used
        size_t TotalPhysicalBytes = 0;  // Total installed system RAM
        size_t AvailablePhysicalBytes = 0;
        size_t DedicatedVideoMemoryBytes = 0; // Total dedicated VRAM
        size_t UsedVideoMemoryBytes = 0;      // VRAM currently in use
    };

    struct FGPUInfo {
        std::string Vendor;
        std::string Renderer;
        std::string Version;
        std::string ShadingLanguageVersion;
    };

    class FPlatformMemory {
    public:
        static FMemoryStats GetMemoryStats();
        static FGPUInfo GetGPUInfo();
    };

} // namespace Leon
