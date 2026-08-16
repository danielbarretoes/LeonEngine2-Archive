#include "FOpenGLTexture2D.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Core/FLog.hpp"
#include "RHI/FRenderer.hpp"
#include <stb_image.h>
#include <cmath>
#include <algorithm>

namespace Leon {

    static uint32_t CalculateMipLevels(uint32_t InWidth, uint32_t InHeight) {
        return static_cast<uint32_t>(std::floor(std::log2(std::max(InWidth, InHeight)))) + 1;
    }

    // -------------------------------------------------------------------------
    // Blank RGBA8 texture (used for render target / manual SetData uploads)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight) : Width(InWidth), Height(InHeight) {
        InternalFormat = GL_RGBA8;
        DataFormat = GL_RGBA;
        AllocatedBytes = static_cast<size_t>(Width * Height * 4);

        glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
        glTextureStorage2D(RendererID, 1, InternalFormat, Width, Height);

        glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);
        bIsLoaded = true;

        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    // -------------------------------------------------------------------------
    // RG16F / RGBA16F / RGBA32F float texture (used for BRDF LUT & HDR)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat) {
        Width = InWidth;
        Height = InHeight;

        switch (InFormat) {
        case ETextureFormat::RG16F:
            InternalFormat = GL_RG16F;
            DataFormat = GL_RG;
            AllocatedBytes = static_cast<size_t>(Width * Height * 2 * sizeof(uint16_t));
            break;
        case ETextureFormat::RGBA16F:
            InternalFormat = GL_RGBA16F;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(Width * Height * 4 * sizeof(uint16_t));
            break;
        case ETextureFormat::RGBA32F:
            InternalFormat = GL_RGBA32F;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(Width * Height * 4 * sizeof(float));
            break;
        default: // RGBA8
            InternalFormat = GL_RGBA8;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(Width * Height * 4);
            break;
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
        glTextureStorage2D(RendererID, 1, InternalFormat, Width, Height);

        glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        bIsLoaded = true;

        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    // -------------------------------------------------------------------------
    // File-loaded texture (Native .ltex, LDR: PNG/JPG/TGA, or HDR: .hdr)
    // -------------------------------------------------------------------------
    FOpenGLTexture2D::FOpenGLTexture2D(const std::string& InPath) : Path(InPath) {
        // Native .ltex container
        if (InPath.length() >= 5 && InPath.substr(InPath.length() - 5) == ".ltex") {
            FNativeTextureData nativeData;
            if (!nativeData.LoadFromFile(InPath)) {
                LE_CORE_ERROR("Failed to load native .ltex texture from: {0}", InPath);
                return;
            }

            bIsLoaded = true;
            Width = nativeData.Header.Width;
            Height = nativeData.Header.Height;
            InternalFormat = (nativeData.Header.ColorSpace == 1) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(nativeData.Header.TotalDataSize);

            glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
            uint32_t mipLevels = std::max(1u, nativeData.Header.MipCount);
            glTextureStorage2D(RendererID, mipLevels, InternalFormat, Width, Height);

            GLenum minFilter = (mipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
            glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, minFilter);
            glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GLenum wrap = (nativeData.Header.WrapMode == 1) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, wrap);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, wrap);

            for (const auto& mip : nativeData.Mips) {
                if (!mip.Pixels.empty()) {
                    glTextureSubImage2D(RendererID, mip.Level, 0, 0, mip.Width, mip.Height, DataFormat,
                                        GL_UNSIGNED_BYTE, mip.Pixels.data());
                }
            }

            FRenderer::OnGPUAlloc(AllocatedBytes);
            LE_CORE_INFO("Loaded Native Texture: {0} ({1}x{2}, {3} mips, {4} KB)", InPath, Width, Height, mipLevels,
                         AllocatedBytes / 1024);
            return;
        }

        // Native .lhdr container
        if (InPath.length() >= 5 && InPath.substr(InPath.length() - 5) == ".lhdr") {
            FNativeHDRData hdrData;
            if (!hdrData.LoadFromFile(InPath)) {
                LE_CORE_ERROR("Failed to load native .lhdr texture from: {0}", InPath);
                return;
            }

            bIsLoaded = true;
            Width = hdrData.Header.Width;
            Height = hdrData.Header.Height;
            InternalFormat = GL_RGBA32F;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(hdrData.Header.TotalDataSize);

            uint32_t levels = CalculateMipLevels(Width, Height);

            glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
            glTextureStorage2D(RendererID, levels, InternalFormat, Width, Height);

            glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            if (!hdrData.Pixels.empty()) {
                glTextureSubImage2D(RendererID, 0, 0, 0, Width, Height, DataFormat, GL_FLOAT,
                                    hdrData.Pixels.data());
                glGenerateTextureMipmap(RendererID);
            }

            FRenderer::OnGPUAlloc(AllocatedBytes);
            LE_CORE_INFO("Loaded Native HDR Texture: {0} ({1}x{2}, RGBA32F, {3} KB)", InPath, Width, Height,
                         AllocatedBytes / 1024);
            return;
        }

        int width, height, channels;
        stbi_set_flip_vertically_on_load(1);

        if (stbi_is_hdr(InPath.c_str())) {
            // HDR equirectangular map — load raw, no preprocessing
            float* data = stbi_loadf(InPath.c_str(), &width, &height, &channels, 4);
            if (!data) {
                LE_CORE_ERROR("Failed to load HDR texture from: {0}", InPath);
                return;
            }

            bIsLoaded = true;
            Width = static_cast<uint32_t>(width);
            Height = static_cast<uint32_t>(height);
            InternalFormat = GL_RGBA32F;
            DataFormat = GL_RGBA;
            AllocatedBytes = static_cast<size_t>(Width * Height * 4 * sizeof(float));

            uint32_t levels = CalculateMipLevels(Width, Height);

            glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
            glTextureStorage2D(RendererID, levels, InternalFormat, Width, Height);

            glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTextureSubImage2D(RendererID, 0, 0, 0, Width, Height, DataFormat, GL_FLOAT, data);
            glGenerateTextureMipmap(RendererID);

            stbi_image_free(data);
            FRenderer::OnGPUAlloc(AllocatedBytes);

            LE_CORE_INFO("Loaded HDR Environment Map: {0} ({1}x{2}, RGBA32F, {3} KB)", InPath, Width, Height,
                         AllocatedBytes / 1024);
            return;
        }

        // Standard LDR texture (PNG, JPG, BMP, TGA)
        stbi_uc* data = stbi_load(InPath.c_str(), &width, &height, &channels, 0);
        if (!data) {
            LE_CORE_ERROR("Failed to load texture from: {0}", InPath);
            return;
        }

        bIsLoaded = true;
        Width = static_cast<uint32_t>(width);
        Height = static_cast<uint32_t>(height);

        uint32_t bpp = 4;
        if (channels == 4) {
            InternalFormat = GL_RGBA8;
            DataFormat = GL_RGBA;
            bpp = 4;
        } else if (channels == 3) {
            InternalFormat = GL_RGB8;
            DataFormat = GL_RGB;
            bpp = 3;
        } else if (channels == 1) {
            InternalFormat = GL_R8;
            DataFormat = GL_RED;
            bpp = 1;
        } else {
            LE_CORE_ERROR("Texture format not supported for: {0} ({1} channels)", InPath, channels);
            stbi_image_free(data);
            return;
        }

        AllocatedBytes = static_cast<size_t>(Width * Height * bpp);
        uint32_t levels = CalculateMipLevels(Width, Height);

        glCreateTextures(GL_TEXTURE_2D, 1, &RendererID);
        glTextureStorage2D(RendererID, levels, InternalFormat, Width, Height);

        glTextureParameteri(RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTextureSubImage2D(RendererID, 0, 0, 0, Width, Height, DataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateTextureMipmap(RendererID);

        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        stbi_image_free(data);
        FRenderer::OnGPUAlloc(AllocatedBytes);
    }

    FOpenGLTexture2D::~FOpenGLTexture2D() {
        if (RendererID) {
            FRenderer::OnGPUFree(AllocatedBytes);
            glDeleteTextures(1, &RendererID);
        }
    }

    void FOpenGLTexture2D::SetData(void* InData, uint32_t InSize) {
        uint32_t bpp = (DataFormat == GL_RGBA) ? 4 : (DataFormat == GL_RGB ? 3 : (DataFormat == GL_RG ? 2 : 1));
        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTextureSubImage2D(RendererID, 0, 0, 0, Width, Height, DataFormat, GL_UNSIGNED_BYTE, InData);

        if (bpp != 4)
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    void FOpenGLTexture2D::SetDataFloat(const void* InData, uint32_t /*InSize*/) {
        glTextureSubImage2D(RendererID, 0, 0, 0, Width, Height, DataFormat, GL_FLOAT, InData);
    }

    void FOpenGLTexture2D::Bind(uint32_t InSlot) const {
        glBindTextureUnit(InSlot, RendererID);
    }

} // namespace Leon
