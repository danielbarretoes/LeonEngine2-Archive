#include "OpenGLTexture2D.hpp"
#include "core/Log.hpp"
#include "renderer/Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <stb_image.h>
#include <vector>

namespace Leon {

    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight) : m_Width(InWidth), m_Height(InHeight) {
        m_InternalFormat = GL_RGBA8;
        m_DataFormat = GL_RGBA;
        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4);

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);
        m_IsLoaded = true;

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTexture2D::FOpenGLTexture2D(const std::string& InPath) : m_Path(InPath) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        // Check if the file is a 32-bit floating point HDR image (.hdr)
        if (stbi_is_hdr(InPath.c_str())) {
            // Request 4 channels (RGBA) for reliable OpenGL memory alignment and mipmap generation
            float* data = stbi_loadf(InPath.c_str(), &width, &height, &channels, 4);
            if (!data) {
                LE_CORE_ERROR("Failed to load HDR texture image from: {0}", InPath);
                return;
            }

            m_IsLoaded = true;
            m_Width = static_cast<uint32_t>(width);
            m_Height = static_cast<uint32_t>(height);

            // Pre-filter HDR solar delta-spike into a smooth Gaussian profile
            // to ensure continuous, non-pixelated spherical mipmaps across all roughness levels
            float maxVal = 0.0f;
            int maxX = 0, maxY = 0;
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    float r = data[(y * width + x) * 4 + 0];
                    float g = data[(y * width + x) * 4 + 1];
                    float b = data[(y * width + x) * 4 + 2];
                    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                    if (lum > maxVal) {
                        maxVal = lum;
                        maxX = x;
                        maxY = y;
                    }
                }
            }

            // If an extreme solar delta spike exists (> 100.0)
            if (maxVal > 100.0f) {
                int radius = 18;
                float sigma = 7.0f;
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        float distSq = static_cast<float>(dx * dx + dy * dy);
                        if (distSq <= static_cast<float>(radius * radius)) {
                            int px = (maxX + dx + width) % width;
                            int py = std::clamp(maxY + dy, 0, height - 1);
                            float weight = std::exp(-distSq / (2.0f * sigma * sigma));
                            for (int c = 0; c < 3; ++c) {
                                float val = data[(py * width + px) * 4 + c];
                                if (val > 40.0f) {
                                    data[(py * width + px) * 4 + c] = std::max(val * 0.20f, maxVal * 0.12f * weight);
                                }
                            }
                        }
                    }
                }
            }

            // Use full 32-bit floating point precision (GL_RGBA32F) to preserve high solar radiance
            m_InternalFormat = GL_RGBA32F;
            m_DataFormat = GL_RGBA;
            m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * 4 * sizeof(float));

            glGenTextures(1, &m_RendererID);
            glBindTexture(GL_TEXTURE_2D, m_RendererID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_FLOAT, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(data);
            FRenderer::OnGPUAlloc(m_AllocatedBytes);

            LE_CORE_INFO("Loaded HDR Environment Map: {0} ({1}x{2}, RGBA32F, {3} KB)", InPath, m_Width, m_Height,
                         m_AllocatedBytes / 1024);
            return;
        }

        // Standard LDR Texture (PNG, JPG, BMP, TGA)
        stbi_uc* data = stbi_load(InPath.c_str(), &width, &height, &channels, 0);

        if (!data) {
            LE_CORE_ERROR("Failed to load texture image from: {0}", InPath);
            return;
        }

        m_IsLoaded = true;
        m_Width = static_cast<uint32_t>(width);
        m_Height = static_cast<uint32_t>(height);

        GLenum internalFormat = 0, dataFormat = 0;
        uint32_t bpp = 4;
        if (channels == 4) {
            internalFormat = GL_RGBA8;
            dataFormat = GL_RGBA;
            bpp = 4;
        } else if (channels == 3) {
            internalFormat = GL_RGB8;
            dataFormat = GL_RGB;
            bpp = 3;
        } else if (channels == 1) {
            internalFormat = GL_R8;
            dataFormat = GL_RED;
            bpp = 1;
        }

        m_InternalFormat = internalFormat;
        m_DataFormat = dataFormat;
        m_AllocatedBytes = static_cast<size_t>(m_Width * m_Height * bpp);

        if (internalFormat == 0 || dataFormat == 0) {
            LE_CORE_ERROR("Texture format not supported for: {0} ({1} channels)", InPath, channels);
            stbi_image_free(data);
            return;
        }

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (bpp != 4) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }

        glTexImage2D(GL_TEXTURE_2D, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        if (bpp != 4) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }

        stbi_image_free(data);

        FRenderer::OnGPUAlloc(m_AllocatedBytes);
    }

    FOpenGLTexture2D::~FOpenGLTexture2D() {
        if (m_RendererID) {
            FRenderer::OnGPUFree(m_AllocatedBytes);
            glDeleteTextures(1, &m_RendererID);
        }
    }

    void FOpenGLTexture2D::SetData(void* InData, uint32_t InSize) {
        uint32_t bpp = (m_DataFormat == GL_RGBA) ? 4 : (m_DataFormat == GL_RGB ? 3 : 1);
        LE_CORE_ASSERT(InSize == m_Width * m_Height * bpp, "Data must be entire texture!");
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        if (bpp != 4) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, GL_UNSIGNED_BYTE, InData);
        if (bpp != 4) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }
    }

    void FOpenGLTexture2D::Bind(uint32_t InSlot) const {
        glActiveTexture(GL_TEXTURE0 + InSlot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

} // namespace Leon
