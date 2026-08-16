#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace Leon {

    struct FLightmapChart {
        uint32_t InstanceIndex = 0;
        uint32_t Resolution = 64;
        int32_t AtlasX = 0;
        int32_t AtlasY = 0;
        uint32_t PackedWidth = 0;
        uint32_t PackedHeight = 0;
        glm::vec2 Scale{1.0f, 1.0f};
        glm::vec2 Bias{0.0f, 0.0f};
    };

    /**
     * @brief Packs per-instance lightmap resolutions into a power-of-two atlas.
     */
    class FLightmapBuilder {
    public:
        static bool PackCharts(std::vector<FLightmapChart>& InOutCharts, uint32_t& OutAtlasWidth,
                                uint32_t& OutAtlasHeight, uint32_t InPadding = 2);

        static uint32_t NextPowerOfTwo(uint32_t InValue);
    };

} // namespace Leon
