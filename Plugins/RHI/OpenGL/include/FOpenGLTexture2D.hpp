#pragma once

#include "RHI/FTexture.hpp"
#include <glad/glad.h>
#include <cstdint>

namespace Leon {

    class FOpenGLTexture2D : public FTexture2D {
    public:
        /// Blank RGBA8 texture for manual SetData uploads
        FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight);

        /// Explicit format texture for float precision (BRDF LUT, offscreen HDR targets)
        FOpenGLTexture2D(uint32_t InWidth, uint32_t InHeight, ETextureFormat InFormat);

        /// File-loaded texture (LDR: PNG/JPG/TGA, or HDR: .hdr equirectangular)
        FOpenGLTexture2D(const std::string& InPath);

        ~FOpenGLTexture2D() override;

        uint32_t GetWidth() const override { return Width; }
        uint32_t GetHeight() const override { return Height; }
        uint32_t GetRendererID() const override { return RendererID; }
        const std::string& GetPath() const override { return Path; }

        void SetData(void* InData, uint32_t InSize) override;

        /// Upload raw float data (for RG16F / RGBA16F / RGBA32F textures)
        void SetDataFloat(const void* InData, uint32_t InSize) override;

        void Bind(uint32_t InSlot = 0) const override;
        bool IsLoaded() const override { return bIsLoaded; }

        bool operator==(const FTexture& InOther) const override { return RendererID == InOther.GetRendererID(); }

    private:
        std::string Path;
        bool bIsLoaded = false;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t RendererID = 0;
        size_t AllocatedBytes = 0;
        GLenum InternalFormat = 0;
        GLenum DataFormat = 0;
    };

} // namespace Leon
