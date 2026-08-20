#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Leon::FOpenGLTextureResize {

    inline void ComputeTargetSize(uint32_t InSrcW, uint32_t InSrcH, uint32_t InMaxDim, uint32_t& OutW, uint32_t& OutH) {
        OutW = InSrcW;
        OutH = InSrcH;
        if (InMaxDim == 0 || std::max(InSrcW, InSrcH) <= InMaxDim)
            return;

        const float scale = static_cast<float>(InMaxDim) / static_cast<float>(std::max(InSrcW, InSrcH));
        OutW = std::max(1u, static_cast<uint32_t>(static_cast<float>(InSrcW) * scale + 0.5f));
        OutH = std::max(1u, static_cast<uint32_t>(static_cast<float>(InSrcH) * scale + 0.5f));
    }

    inline void BoxDownscaleU8(const uint8_t* InSrc, uint32_t InSrcW, uint32_t InSrcH, uint32_t InChannels,
                               uint8_t* OutDst, uint32_t InDstW, uint32_t InDstH) {
        for (uint32_t y = 0; y < InDstH; ++y) {
            const uint32_t y0 = y * InSrcH / InDstH;
            const uint32_t y1 = std::max(y0 + 1u, (y + 1u) * InSrcH / InDstH);
            for (uint32_t x = 0; x < InDstW; ++x) {
                const uint32_t x0 = x * InSrcW / InDstW;
                const uint32_t x1 = std::max(x0 + 1u, (x + 1u) * InSrcW / InDstW);
                for (uint32_t c = 0; c < InChannels; ++c) {
                    double sum = 0.0;
                    uint32_t count = 0;
                    for (uint32_t sy = y0; sy < y1; ++sy) {
                        for (uint32_t sx = x0; sx < x1; ++sx) {
                            sum += InSrc[(sy * InSrcW + sx) * InChannels + c];
                            ++count;
                        }
                    }
                    OutDst[(y * InDstW + x) * InChannels + c] =
                        static_cast<uint8_t>(sum / static_cast<double>(std::max(count, 1u)));
                }
            }
        }
    }

    inline std::vector<uint8_t> DownscaleU8(const uint8_t* InSrc, uint32_t InSrcW, uint32_t InSrcH, uint32_t InChannels,
                                            uint32_t InDstW, uint32_t InDstH) {
        std::vector<uint8_t> out(static_cast<size_t>(InDstW) * InDstH * InChannels);
        if (InSrcW == InDstW && InSrcH == InDstH) {
            std::copy(InSrc, InSrc + out.size(), out.begin());
            return out;
        }
        BoxDownscaleU8(InSrc, InSrcW, InSrcH, InChannels, out.data(), InDstW, InDstH);
        return out;
    }

    inline void BoxDownscaleFloat(const float* InSrc, uint32_t InSrcW, uint32_t InSrcH, uint32_t InChannels,
                                  float* OutDst, uint32_t InDstW, uint32_t InDstH) {
        for (uint32_t y = 0; y < InDstH; ++y) {
            const uint32_t y0 = y * InSrcH / InDstH;
            const uint32_t y1 = std::max(y0 + 1u, (y + 1u) * InSrcH / InDstH);
            for (uint32_t x = 0; x < InDstW; ++x) {
                const uint32_t x0 = x * InSrcW / InDstW;
                const uint32_t x1 = std::max(x0 + 1u, (x + 1u) * InSrcW / InDstW);
                for (uint32_t c = 0; c < InChannels; ++c) {
                    double sum = 0.0;
                    uint32_t count = 0;
                    for (uint32_t sy = y0; sy < y1; ++sy) {
                        for (uint32_t sx = x0; sx < x1; ++sx) {
                            sum += InSrc[(sy * InSrcW + sx) * InChannels + c];
                            ++count;
                        }
                    }
                    OutDst[(y * InDstW + x) * InChannels + c] =
                        static_cast<float>(sum / static_cast<double>(std::max(count, 1u)));
                }
            }
        }
    }

    inline std::vector<float> DownscaleFloat(const float* InSrc, uint32_t InSrcW, uint32_t InSrcH, uint32_t InChannels,
                                             uint32_t InDstW, uint32_t InDstH) {
        std::vector<float> out(static_cast<size_t>(InDstW) * InDstH * InChannels);
        if (InSrcW == InDstW && InSrcH == InDstH) {
            std::copy(InSrc, InSrc + out.size(), out.begin());
            return out;
        }
        BoxDownscaleFloat(InSrc, InSrcW, InSrcH, InChannels, out.data(), InDstW, InDstH);
        return out;
    }

} // namespace Leon::FOpenGLTextureResize
