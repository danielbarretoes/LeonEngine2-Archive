#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include "Renderer/FIBLMath.hpp"

namespace Leon::TestFixtures {

    inline std::vector<float> CreateConstantHDR(int width, int height, glm::vec3 color) {
        std::vector<float> data(static_cast<size_t>(width) * height * 4, 0.0f);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                data[idx + 0] = color.r;
                data[idx + 1] = color.g;
                data[idx + 2] = color.b;
                data[idx + 3] = 1.0f;
            }
        }
        return data;
    }

    inline std::vector<float> CreateHemisphereStepHDR(int width, int height, glm::vec3 topColor,
                                                      glm::vec3 bottomColor) {
        std::vector<float> data(static_cast<size_t>(width) * height * 4, 0.0f);
        for (int y = 0; y < height; ++y) {
            glm::vec3 col = (y < height / 2) ? topColor : bottomColor;
            for (int x = 0; x < width; ++x) {
                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                data[idx + 0] = col.r;
                data[idx + 1] = col.g;
                data[idx + 2] = col.b;
                data[idx + 3] = 1.0f;
            }
        }
        return data;
    }

    inline std::vector<float> CreateSmoothGradientHDR(int width, int height, glm::vec3 zenithColor,
                                                      glm::vec3 horizonColor, glm::vec3 groundColor) {
        std::vector<float> data(static_cast<size_t>(width) * height * 4, 0.0f);
        for (int y = 0; y < height; ++y) {
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            float elevation = (0.5f - v) * 2.0f; // from -1 (bottom pole) to +1 (zenith)
            glm::vec3 col;
            if (elevation >= 0.0f) {
                col = glm::mix(horizonColor, zenithColor, elevation);
            } else {
                col = glm::mix(horizonColor, groundColor, -elevation);
            }
            for (int x = 0; x < width; ++x) {
                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                data[idx + 0] = col.r;
                data[idx + 1] = col.g;
                data[idx + 2] = col.b;
                data[idx + 3] = 1.0f;
            }
        }
        return data;
    }

    inline std::vector<float> CreateSyntheticSolarHDR(int width, int height, glm::vec3 skyColor, glm::vec3 sunColor,
                                                      int sunX, int sunY, int sunRadius) {
        std::vector<float> data(static_cast<size_t>(width) * height * 4, 0.0f);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                size_t idx = (static_cast<size_t>(y) * width + x) * 4;
                int dx = x - sunX;
                int dy = y - sunY;
                if (dx * dx + dy * dy <= sunRadius * sunRadius) {
                    data[idx + 0] = sunColor.r;
                    data[idx + 1] = sunColor.g;
                    data[idx + 2] = sunColor.b;
                } else {
                    data[idx + 0] = skyColor.r;
                    data[idx + 1] = skyColor.g;
                    data[idx + 2] = skyColor.b;
                }
                data[idx + 3] = 1.0f;
            }
        }
        return data;
    }

} // namespace Leon::TestFixtures
