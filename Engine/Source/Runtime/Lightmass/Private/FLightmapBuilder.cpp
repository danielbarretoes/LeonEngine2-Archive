#include "Lightmass/FLightmapBuilder.hpp"

#include <algorithm>

namespace Leon {

    uint32_t FLightmapBuilder::NextPowerOfTwo(uint32_t InValue) {
        uint32_t v = std::max(1u, InValue);
        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return v + 1;
    }

    bool FLightmapBuilder::PackCharts(std::vector<FLightmapChart>& InOutCharts, uint32_t& OutAtlasWidth,
                                      uint32_t& OutAtlasHeight, uint32_t InPadding) {
        if (InOutCharts.empty()) {
            OutAtlasWidth = 1;
            OutAtlasHeight = 1;
            return false;
        }

        std::vector<size_t> order(InOutCharts.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(),
                  [&](size_t a, size_t b) { return InOutCharts[a].Resolution > InOutCharts[b].Resolution; });

        uint32_t cursorX = InPadding;
        uint32_t cursorY = InPadding;
        uint32_t rowHeight = 0;
        uint32_t maxX = 0;
        uint32_t maxY = 0;
        uint32_t shelfWidth = 0;
        for (const auto& c : InOutCharts)
            shelfWidth += c.Resolution + InPadding * 2;
        shelfWidth = NextPowerOfTwo(std::max(64u, shelfWidth / 2));

        for (size_t idx : order) {
            auto& chart = InOutCharts[idx];
            uint32_t w = chart.Resolution;
            uint32_t h = chart.Resolution;
            if (cursorX + w + InPadding > shelfWidth) {
                cursorX = InPadding;
                cursorY += rowHeight + InPadding;
                rowHeight = 0;
            }
            chart.AtlasX = static_cast<int32_t>(cursorX);
            chart.AtlasY = static_cast<int32_t>(cursorY);
            chart.PackedWidth = w;
            chart.PackedHeight = h;
            cursorX += w + InPadding;
            rowHeight = std::max(rowHeight, h);
            maxX = std::max(maxX, cursorX);
            maxY = std::max(maxY, cursorY + h + InPadding);
        }

        OutAtlasWidth = NextPowerOfTwo(maxX);
        OutAtlasHeight = NextPowerOfTwo(maxY);
        for (auto& chart : InOutCharts) {
            chart.Scale = {static_cast<float>(chart.PackedWidth) / static_cast<float>(OutAtlasWidth),
                           static_cast<float>(chart.PackedHeight) / static_cast<float>(OutAtlasHeight)};
            chart.Bias = {static_cast<float>(chart.AtlasX) / static_cast<float>(OutAtlasWidth),
                          static_cast<float>(chart.AtlasY) / static_cast<float>(OutAtlasHeight)};
        }
        return true;
    }

} // namespace Leon
